#include "../../modules.h"
#include "../../../../globals.h"
#include "../../../../backend/usermode/usermode.h"
#include "bulletracer.h"
#include "../../functions.h"
#include "../../modules.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include <set>
#include "../../legit/legit.h"

enum OutCode {
    INSIDE = 0,
    LEFT = 1,
    RIGHT = 2,
    BOTTOM = 4,
    TOP = 8
};

Vec4 world_to_clip(const Vec3& world_pos, const Mat4& viewmatrix) {
    Vec4 clip_coords;
    clip_coords.x = world_pos.x * viewmatrix.data[0] + world_pos.y * viewmatrix.data[1] + world_pos.z * viewmatrix.data[2] + viewmatrix.data[3];
    clip_coords.y = world_pos.x * viewmatrix.data[4] + world_pos.y * viewmatrix.data[5] + world_pos.z * viewmatrix.data[6] + viewmatrix.data[7];
    clip_coords.z = world_pos.x * viewmatrix.data[8] + world_pos.y * viewmatrix.data[9] + world_pos.z * viewmatrix.data[10] + viewmatrix.data[11];
    clip_coords.w = world_pos.x * viewmatrix.data[12] + world_pos.y * viewmatrix.data[13] + world_pos.z * viewmatrix.data[14] + viewmatrix.data[15];
    return clip_coords;
}

OutCode compute_outcode(float x, float y, float x_min, float y_min, float x_max, float y_max) {
    OutCode code = INSIDE;
    if (x < x_min) {
        code = static_cast<OutCode>(code | LEFT);
    }
    else if (x > x_max) {
        code = static_cast<OutCode>(code | RIGHT);
    }
    if (y < y_min) {
        code = static_cast<OutCode>(code | BOTTOM);
    }
    else if (y > y_max) {
        code = static_cast<OutCode>(code | TOP);
    }
    return code;
}

namespace bullet_tracer {

    BulletTracer::BulletTracer(const Vec3& start, const Vec3& end)
        : start_position(start), end_position(end), creation_time(std::chrono::steady_clock::now()) {
    }

    bool BulletTracer::is_expired() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time).count();
        return elapsed >= (duration * 1000);
    }

    float BulletTracer::get_alpha() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time).count();
        float progress = elapsed / (duration * 1000.0f);
        return std::max(0.0f, 1.0f - progress);
    }

    TracerCache::TracerCache() : last_health_check(std::chrono::steady_clock::now()),
        last_shot_time(std::chrono::steady_clock::now()),
        last_click_time(std::chrono::steady_clock::now()),
        damage_time(std::chrono::steady_clock::now()) {
    }

    PlayerHealthTracker::PlayerHealthTracker() : player_address(0), last_health(0.0f), valid(false),
        last_update(std::chrono::steady_clock::now()) {
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(tracer_mutex);
        std::lock_guard<std::mutex> health_lock(health_tracker_mutex);

        active_tracers.clear();
        health_trackers.clear();
        tracer_cache = TracerCache();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(tracer_mutex);
        std::lock_guard<std::mutex> health_lock(health_tracker_mutex);

        active_tracers.clear();
        health_trackers.clear();
    }

    static bool is_local_player(const roblox::player& player) {
        if (globals::lp.main.address != 0 && player.main.address == globals::lp.main.address) return true;
        if (!globals::lp.name.empty() && player.name == globals::lp.name) return true;
        return false;
    }

    static float get_player_health(const roblox::player& player) {
        if (!player.is_valid()) return -1.0f;

        if (globals::gamename == "Arsenal") {
            try {
                auto health_value = player.main.findfirstchild("NRPBS").findfirstchild("Health").read_double_value();
                if (health_value >= 0.0 && health_value <= 20000.0) {
                    return static_cast<float>(health_value);
                }
            }
            catch (...) {
            }
        }

        if (player.humanoid.is_valid()) {
            try {
                float humanoid_health = player.humanoid.read_health();
                if (humanoid_health >= 0.0f) {
                    return humanoid_health;
                }
            }
            catch (...) {
            }
        }

        return -1.0f;
    }

    static bool get_player_aim_point(const roblox::player& player, const Vec2& mouse_position, Vec3& out_position, float& out_distance) {
        out_position = Vec3::zero();
        out_distance = FLT_MAX;

        if (!player.is_valid()) return false;

        roblox::instance parts[5];
        int part_count = 0;

        if (player.head.is_valid()) parts[part_count++] = player.head;
        if (player.torso.is_valid()) parts[part_count++] = player.torso;
        if (player.uppertorso.is_valid()) parts[part_count++] = player.uppertorso;
        if (player.lowertorso.is_valid()) parts[part_count++] = player.lowertorso;
        if (player.hrp.is_valid()) parts[part_count++] = player.hrp;

        if (part_count == 0) return false;

        bool found = false;

        for (int i = 0; i < part_count; ++i) {
            Vec3 world_position = parts[i].get_pos();
            if (world_position.magnitude() < 1.0f) continue;

            Vec2 screen_position = roblox::worldtoscreen(world_position);

            // Only parts the game actually renders can be under the crosshair
            if (screen_position.x == -1.0f || screen_position.y == -1.0f) continue;

            float dx = mouse_position.x - screen_position.x;
            float dy = mouse_position.y - screen_position.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            if (distance < out_distance) {
                out_distance = distance;
                out_position = world_position;
                found = true;
            }
        }

        return found;
    }

    static Vec3 get_muzzle_position() {
        if (globals::lp.righthand.is_valid()) {
            Vec3 position = globals::lp.righthand.get_pos();
            if (position.magnitude() > 1.0f) return position;
        }

        if (globals::lp.head.is_valid()) {
            Vec3 position = globals::lp.head.get_pos();
            if (position.magnitude() > 1.0f) return position;
        }

        auto camera = functions::get_current_camera();
        if (camera.is_valid()) return camera.getPos();

        return Vec3::zero();
    }

    static Vec3 get_aim_direction_point(float distance) {
        auto camera = functions::get_current_camera();
        if (!camera.is_valid()) return Vec3::zero();

        Vec3 camera_position = camera.getPos();
        if (camera_position.magnitude() < 1.0f) return Vec3::zero();

        Mat3 rotation = camera.getRot();
        Vec3 forward(rotation.data[6], rotation.data[7], rotation.data[8]);
        if (forward.magnitude() < 0.001f) return Vec3::zero();

        forward = forward.normalize();

        return Vec3(
            camera_position.x + forward.x * distance,
            camera_position.y + forward.y * distance,
            camera_position.z + forward.z * distance
        );
    }

    void update_cache() {
        if (!settings::enabled) return;

        std::lock_guard<std::mutex> lock(cache_mutex);

        Vec2 mouse_position = functions::get_mouse_position();
        auto players = globals::cachedplayers;
        auto now = std::chrono::steady_clock::now();

        roblox::player best_player = {};
        Vec3 best_position = Vec3::zero();
        float best_distance = FLT_MAX;

        tracer_cache.damage_detected = false;

        for (const auto& player : players) {
            if (!player.is_valid() || is_local_player(player)) continue;

            Vec3 aim_position = Vec3::zero();
            float aim_distance = FLT_MAX;
            bool has_aim_point = get_player_aim_point(player, mouse_position, aim_position, aim_distance);

            if (has_aim_point && aim_distance <= settings::detection_range && aim_distance < best_distance) {
                best_distance = aim_distance;
                best_position = aim_position;
                best_player = player;
            }

            if (!player.humanoid.is_valid() || !player.main.is_valid()) continue;

            float current_health = get_player_health(player);
            if (current_health < 0.0f) continue;

            auto& tracker = health_trackers[player.main.address];

            if (!tracker.valid) {
                tracker.player_address = player.main.address;
                tracker.last_health = current_health;
                tracker.last_update = now;
                tracker.valid = true;
                continue;
            }

            tracker.last_update = now;

            if (current_health > tracker.last_health) {
                tracker.last_health = current_health;
                continue;
            }

            float damage = tracker.last_health - current_health;

            if (damage >= settings::health_threshold) {
                tracker.last_health = current_health;

                if (has_aim_point) {
                    tracer_cache.damage_detected = true;
                    tracer_cache.damage_position = aim_position;
                    tracer_cache.damage_time = now;
                }
            }
        }

        for (auto it = health_trackers.begin(); it != health_trackers.end();) {
            auto seconds_since_update = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_update).count();
            if (seconds_since_update > 10) {
                it = health_trackers.erase(it);
            }
            else {
                ++it;
            }
        }

        tracer_cache.target_player = best_player;
        tracer_cache.target_part_position = best_position;

        SHORT key_state = GetAsyncKeyState(VK_LBUTTON);
        bool mouse_held = (key_state & 0x8000) != 0;
        bool mouse_pressed = (key_state & 0x0001) != 0;

        tracer_cache.was_mouse_held = tracer_cache.mouse_held;
        tracer_cache.mouse_held = mouse_held;
        tracer_cache.mouse_pressed = (mouse_held && !tracer_cache.was_mouse_held) || mouse_pressed;

        if (mouse_held) {
            tracer_cache.last_click_time = now;
        }
    }

    void detect_shots() {
        if (!settings::enabled) return;

        std::lock_guard<std::mutex> lock(cache_mutex);

        auto now = std::chrono::steady_clock::now();

        bool clicked = tracer_cache.mouse_pressed;
        bool damaged = tracer_cache.damage_detected;
        tracer_cache.mouse_pressed = false;
        tracer_cache.damage_detected = false;

        if (!clicked && !damaged) return;

        auto since_last_shot = std::chrono::duration_cast<std::chrono::milliseconds>(now - tracer_cache.last_shot_time).count();
        if (since_last_shot < 90) return;

        auto since_last_click = std::chrono::duration_cast<std::chrono::milliseconds>(now - tracer_cache.last_click_time).count();
        if (!tracer_cache.mouse_held && since_last_click > 400) return;

        Vec3 start_position = get_muzzle_position();
        if (start_position.magnitude() < 1.0f) return;

        Vec3 end_position = Vec3::zero();

        if (damaged && tracer_cache.damage_position.magnitude() > 1.0f) {
            end_position = tracer_cache.damage_position;
        }
        else if (clicked && tracer_cache.target_player.is_valid() && tracer_cache.target_part_position.magnitude() > 1.0f) {
            end_position = tracer_cache.target_part_position;
        }
        else if (clicked) {
            end_position = get_aim_direction_point(80.0f);
        }

        if (end_position.magnitude() < 1.0f) return;
        if (functions::get_3d_distance(start_position, end_position) < 1.0f) return;

        add_tracer(start_position, end_position);
        tracer_cache.last_shot_time = now;
    }

    void add_tracer(const Vec3& start, const Vec3& end) {
        std::lock_guard<std::mutex> lock(tracer_mutex);


        BulletTracer tracer(start, end);
        tracer.duration = settings::tracer_duration;
        tracer.thickness = settings::tracer_thickness;

        if (settings::rainbow_tracers) {
            float time = ImGui::GetTime();
            ImVec4 rainbow = ImColor::HSV(fmod(time * 0.5f, 1.0f), 1.0f, 1.0f);
            tracer.color = ImGui::ColorConvertFloat4ToU32(rainbow);
        }
        else {
            tracer.color = ImGui::ColorConvertFloat4ToU32(ImVec4(
                settings::tracer_color[0],
                settings::tracer_color[1],
                settings::tracer_color[2],
                settings::tracer_color[3]
            ));
        }

        active_tracers.push_back(tracer);

        if (active_tracers.size() > 100) {
            active_tracers.erase(active_tracers.begin());
        }
    }

    void cleanup_expired_tracers() {
        std::lock_guard<std::mutex> lock(tracer_mutex);

        active_tracers.erase(
            std::remove_if(active_tracers.begin(), active_tracers.end(),
                [](const BulletTracer& tracer) { return tracer.is_expired(); }),
            active_tracers.end()
        );
    }

    void render_tracers() {
        if (!settings::enabled) return;
        cleanup_expired_tracers();

        std::lock_guard<std::mutex> lock(tracer_mutex);
        if (active_tracers.empty()) return;

        if (!is_valid_address(globals::visualengine.address)) return;

        auto draw_list = ImGui::GetBackgroundDrawList();
        Vec2 screen_dims = functions::get_screen_dimensions();
        if (screen_dims.x < 1.0f || screen_dims.y < 1.0f) return;

        Mat4 viewmatrix = read<Mat4>(globals::visualengine.address + offsets::viewmatrix);
        const float near_clip_w = 0.01f;

        for (auto& tracer : active_tracers) {
            if (!tracer.active) continue;

            Vec4 clip1 = world_to_clip(tracer.start_position, viewmatrix);
            Vec4 clip2 = world_to_clip(tracer.end_position, viewmatrix);

            bool original_endpoint_was_behind = clip2.w < near_clip_w;
            if (clip1.w < near_clip_w && clip2.w < near_clip_w) continue;

            if (clip1.w < near_clip_w || clip2.w < near_clip_w) {
                if (clip1.w < near_clip_w) std::swap(clip1, clip2);
                float t = (clip1.w - near_clip_w) / (clip1.w - clip2.w);
                clip2.x = clip1.x + t * (clip2.x - clip1.x);
                clip2.y = clip1.y + t * (clip2.y - clip1.y);
                clip2.z = clip1.z + t * (clip2.z - clip1.z);
                clip2.w = near_clip_w;
            }

            Vec2 p1, p2;
            p1.x = (clip1.x / clip1.w * 0.5f + 0.5f) * screen_dims.x;
            p1.y = (-clip1.y / clip1.w * 0.5f + 0.5f) * screen_dims.y;
            p2.x = (clip2.x / clip2.w * 0.5f + 0.5f) * screen_dims.x;
            p2.y = (-clip2.y / clip2.w * 0.5f + 0.5f) * screen_dims.y;

            ImVec2 original_p2 = ImVec2(p2.x, p2.y);

            OutCode outcode1 = compute_outcode(p1.x, p1.y, 0, 0, screen_dims.x, screen_dims.y);
            OutCode outcode2 = compute_outcode(p2.x, p2.y, 0, 0, screen_dims.x, screen_dims.y);
            bool accept = false;

            while (true) {
                if (!((int)outcode1 | (int)outcode2)) { accept = true; break; }
                else if ((int)outcode1 & (int)outcode2) { break; }
                else {
                    float x, y;
                    OutCode outcodeOut = outcode1 ? outcode1 : outcode2;
                    if ((int)outcodeOut & TOP) { x = p1.x + (p2.x - p1.x) * (screen_dims.y - p1.y) / (p2.y - p1.y); y = screen_dims.y; }
                    else if ((int)outcodeOut & BOTTOM) { x = p1.x + (p2.x - p1.x) * (0 - p1.y) / (p2.y - p1.y); y = 0; }
                    else if ((int)outcodeOut & RIGHT) { y = p1.y + (p2.y - p1.y) * (screen_dims.x - p1.x) / (p2.x - p1.x); x = screen_dims.x; }
                    else { y = p1.y + (p2.y - p1.y) * (0 - p1.x) / (p2.x - p1.x); x = 0; }
                    if (outcodeOut == outcode1) { p1.x = x; p1.y = y; outcode1 = compute_outcode(p1.x, p1.y, 0, 0, screen_dims.x, screen_dims.y); }
                    else { p2.x = x; p2.y = y; outcode2 = compute_outcode(p2.x, p2.y, 0, 0, screen_dims.x, screen_dims.y); }
                }
            }

            if (accept) {
                float alpha = settings::fade_out ? tracer.get_alpha() : 1.0f;
                ImVec4 color_vec = ImGui::ColorConvertU32ToFloat4(tracer.color);
                color_vec.w *= alpha;
                ImU32 render_color = ImGui::ColorConvertFloat4ToU32(color_vec);
                ImU32 outline_color = IM_COL32(0, 0, 0, 255 * alpha);

                draw_list->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), outline_color, tracer.thickness + 1.5f);
                draw_list->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), render_color, tracer.thickness);

                float cross_size = 4.2f;
                float crosshair_thickness = tracer.thickness > 1.0f ? tracer.thickness * 0.75f : 1.0f;

                draw_list->AddLine(ImVec2(original_p2.x - cross_size, original_p2.y), ImVec2(original_p2.x + cross_size, original_p2.y), outline_color, crosshair_thickness + 1.5f);
                draw_list->AddLine(ImVec2(original_p2.x, original_p2.y - cross_size), ImVec2(original_p2.x, original_p2.y + cross_size), outline_color, crosshair_thickness + 1.5f);
                draw_list->AddLine(ImVec2(original_p2.x - cross_size, original_p2.y), ImVec2(original_p2.x + cross_size, original_p2.y), render_color, crosshair_thickness);
                draw_list->AddLine(ImVec2(original_p2.x, original_p2.y - cross_size), ImVec2(original_p2.x, original_p2.y + cross_size), render_color, crosshair_thickness);
            }
        }
    }
}

void runs::runs_bullet_tracer_cache(bool firstrecieved) {
    bullet_tracer::initialize();

    while (firstrecieved) {
        if (!firstrecieved) return;

        if (!bullet_tracer::settings::enabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        bullet_tracer::update_cache();
        bullet_tracer::detect_shots();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    bullet_tracer::shutdown();
}