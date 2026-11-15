#include "../../modules.h"
#include "../../../../globals.h"
#include "../../../../backend/usermode/usermode.h"
#include "../../functions.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include "trail.h"

namespace player_access {
    std::vector<roblox::ESPPlayerData> get_cached_esp_players();
}

namespace player_trail {

    SimpleTrailPoint::SimpleTrailPoint(const Vec3& pos) : position(pos), age(0.0f) {}

    namespace settings {
        bool enabled = false;
        float trail_duration = 5.0f;
        float trail_thickness = 1.2f;
        float trail_color[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
        bool rainbow_trail = false;
        bool fade_out = true;
        float point_distance = 0.2f;
        int max_points = 50;

        bool enemy_enabled = false;
        float enemy_color[4] = { 0.95f, 0.25f, 0.25f, 1.0f };
    }

    std::vector<SimpleTrailPoint> trail_points;
    Vec3 last_position = Vec3::zero();
    bool initialized = false;

    void initialize() {
        trail_points.clear();
        trail_points.reserve(settings::max_points);
        last_position = Vec3::zero();
        initialized = true;
    }

    void shutdown() {
        trail_points.clear();
        initialized = false;
    }
    // ---------- enemy trails ----------
    namespace {
        struct EnemyTrail {
            std::vector<SimpleTrailPoint> points;
            Vec3 last_position = Vec3::zero();
        };

        std::unordered_map<std::string, EnemyTrail> enemy_trails;
        std::mutex enemy_mutex;
    }

    // Shared by the local trail and every enemy trail so both look identical apart from colour.
    void draw_trail(const std::vector<SimpleTrailPoint>& points, const float* color, bool rainbow) {
        try {
            ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
            if (!draw_list) return;

            const Vec2 screen_dims = functions::get_screen_dimensions();
            if (screen_dims.x <= 0 || screen_dims.y <= 0) return;

            for (size_t i = 0; i + 1 < points.size(); ++i) {
                const SimpleTrailPoint& current_point = points[i];
                const SimpleTrailPoint& next_point = points[i + 1];

                const Vec2 screen_pos1 = roblox::worldtoscreen(current_point.position);
                const Vec2 screen_pos2 = roblox::worldtoscreen(next_point.position);

                if (screen_pos1.x == -1 || screen_pos1.y == -1 ||
                    screen_pos2.x == -1 || screen_pos2.y == -1) continue;

                if (screen_pos1.x < -100 || screen_pos1.x > screen_dims.x + 100 ||
                    screen_pos1.y < -100 || screen_pos1.y > screen_dims.y + 100) continue;
                if (screen_pos2.x < -100 || screen_pos2.x > screen_dims.x + 100 ||
                    screen_pos2.y < -100 || screen_pos2.y > screen_dims.y + 100) continue;

                float alpha1 = 1.0f;
                float alpha2 = 1.0f;

                if (settings::fade_out) {
                    alpha1 = (current_point.age < settings::trail_duration) ?
                        (1.0f - (current_point.age / settings::trail_duration)) : 0.0f;
                    alpha2 = (next_point.age < settings::trail_duration) ?
                        (1.0f - (next_point.age / settings::trail_duration)) : 0.0f;

                    if (alpha1 < 0.0f) alpha1 = 0.0f;
                    if (alpha2 < 0.0f) alpha2 = 0.0f;
                }

                const float avg_alpha = (alpha1 + alpha2) * 0.5f;
                if (avg_alpha <= 0.01f) continue;

                ImVec4 color_vec;
                if (rainbow) {
                    const float time_offset = static_cast<float>(i) / static_cast<float>(points.size());
                    color_vec = ImColor::HSV(fmod(ImGui::GetTime() * 0.3f + time_offset, 1.0f), 0.8f, 1.0f);
                }
                else {
                    color_vec = ImVec4(color[0], color[1], color[2], color[3]);
                }

                color_vec.w *= avg_alpha;

                const ImU32 render_color = ImGui::ColorConvertFloat4ToU32(color_vec);
                const ImU32 outline_color = IM_COL32(0, 0, 0, static_cast<int>(255 * avg_alpha));
                const float thickness = settings::trail_thickness * (0.5f + 0.5f * avg_alpha);

                if (thickness > 1.5f) {
                    ImVec4 glow_color = color_vec;
                    glow_color.w *= 0.3f;

                    draw_list->AddLine(ImVec2(screen_pos1.x, screen_pos1.y), ImVec2(screen_pos2.x, screen_pos2.y),
                        ImGui::ColorConvertFloat4ToU32(glow_color), thickness * 1.8f);
                }

                draw_list->AddLine(ImVec2(screen_pos1.x, screen_pos1.y), ImVec2(screen_pos2.x, screen_pos2.y),
                    outline_color, thickness + 1.5f);
                draw_list->AddLine(ImVec2(screen_pos1.x, screen_pos1.y), ImVec2(screen_pos2.x, screen_pos2.y),
                    render_color, thickness);
            }
        }
        catch (...) {
            // Ignore rendering errors
        }
    }

    void update_enemy_trails() {
        if (!settings::enemy_enabled) {
            std::lock_guard<std::mutex> lock(enemy_mutex);
            enemy_trails.clear();
            return;
        }

        const std::vector<roblox::ESPPlayerData> players = player_access::get_cached_esp_players();
        const float dt = 0.016f;

        std::lock_guard<std::mutex> lock(enemy_mutex);

        // Drop trails for players that left, or that are no longer marked enemy.
        for (auto it = enemy_trails.begin(); it != enemy_trails.end();) {
            const auto found = std::find_if(players.begin(), players.end(),
                [&](const roblox::ESPPlayerData& p) { return p.name == it->first; });

            const bool keep = found != players.end() && player_relations::get(it->first) == 2;

            if (!keep) it = enemy_trails.erase(it);
            else ++it;
        }

        for (const auto& player : players) {
            if (player.name.empty() || !player.valid) continue;
            if (player_relations::get(player.name) != 2) continue;

            // The ESP cache copies the player position directly; player_ref carries only the
            // instance address, so its hrp is always invalid and used to skip every player.
            const Vec3 current_pos = player.position;

            if (current_pos.magnitude() < 1.0f) continue;

            EnemyTrail& trail = enemy_trails[player.name];

            if (trail.last_position.magnitude() > 1.0f) {
                const float distance = trail.last_position.distance(current_pos);

                if (distance >= settings::point_distance && distance < 60.0f) {
                    if (trail.points.size() >= static_cast<size_t>(settings::max_points))
                        trail.points.erase(trail.points.begin());

                    trail.points.emplace_back(current_pos);
                    trail.last_position = current_pos;
                }
            }
            else {
                trail.last_position = current_pos;
            }

            for (auto& point : trail.points)
                point.age += dt;

            trail.points.erase(
                std::remove_if(trail.points.begin(), trail.points.end(),
                    [](const SimpleTrailPoint& p) { return p.age >= settings::trail_duration; }),
                trail.points.end());
        }
    }

    void render_enemy_trails() {
        if (!settings::enemy_enabled) return;

        std::vector<std::vector<SimpleTrailPoint>> snapshot;

        {
            std::lock_guard<std::mutex> lock(enemy_mutex);

            for (const auto& entry : enemy_trails) {
                if (entry.second.points.size() >= 2)
                    snapshot.push_back(entry.second.points);
            }
        }

        for (const auto& points : snapshot)
            draw_trail(points, settings::enemy_color, false);
    }

    void update_cache() {
        if (!initialized) return;

        update_enemy_trails();

        if (!settings::enabled) return;

        try {
            if (!globals::localplayer.is_valid()) return;

            auto character = globals::lp.main;
            if (!character.is_valid()) return;

            auto hrp = globals::lp.hrp;
            if (!hrp.is_valid()) return;

            Vec3 current_pos = hrp.get_pos();
            if (current_pos.magnitude() < 1.0f) return;

            if (last_position.magnitude() > 1.0f) {
                float distance = last_position.distance(current_pos);
                if (distance >= settings::point_distance && distance < 50.0f) {
                    if (trail_points.size() >= (size_t)settings::max_points) {
                        trail_points.erase(trail_points.begin());
                    }
                    trail_points.emplace_back(current_pos);
                    last_position = current_pos;
                }
            }
            else {
                last_position = current_pos;
            }

            float dt = 0.016f;
            for (size_t i = 0; i < trail_points.size(); ++i) {
                trail_points[i].age += dt;
            }

            auto it = trail_points.begin();
            while (it != trail_points.end()) {
                if (it->age >= settings::trail_duration) {
                    it = trail_points.erase(it);
                }
                else {
                    ++it;
                }
            }

        }
        catch (...) {
            // Ignore errors
        }
    }

    void render_trail() {
        if (!initialized) return;

        render_enemy_trails();

        if (!settings::enabled || trail_points.size() < 2) return;

        draw_trail(trail_points, settings::trail_color, settings::rainbow_trail);
    }
}

void runs::runs_player_trail_cache(bool firstrecieved) {
    player_trail::initialize();

    while (firstrecieved) {
        if (!firstrecieved) break;

        player_trail::update_cache();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    player_trail::shutdown();
}