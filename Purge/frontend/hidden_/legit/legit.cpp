#include "legit.h"
#include "../../../backend/usermode/usermode.h"
#include "../../../includes.h"


void runs::runs_aimbot(bool firstrecieved) {
    bool found_target = false;
    bool was_keybind_active = false;

    // 1ms timer resolution so the smoothing sleep() below wakes on a steady
    // ~1ms cadence instead of the default 15.6ms tick, which made the glide
    // visibly step instead of flow.
    timeBeginPeriod(1);

    while (firstrecieved) {
        if (!firstrecieved) return;

        features::aimbot::aimbotbind.update();

        bool keybind_active = features::aimbot::enabled && features::aimbot::aimbotbind.enabled && !globals::menuactive;

        if (!keybind_active) {
            if (was_keybind_active) {
                std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
                aimbot_cache::current_target = {};
                found_target = false;
            }
            was_keybind_active = false;
            Sleep(50);
            continue;
        }

        if (!was_keybind_active) {
            std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
            aimbot_cache::current_target = {};
            found_target = false;
        }
        was_keybind_active = true;

        auto now = std::chrono::steady_clock::now();
        bool should_update_cache = std::chrono::duration_cast<std::chrono::milliseconds>(now - aimbot_cache::last_cache_update).count() >= aimbot_cache::cache_refresh_ms;

        if (should_update_cache) {
            aimbot_cache::cached_cursor_pos = functions::get_mouse_position();
            aimbot_cache::cached_camera = functions::get_current_camera();
            if (aimbot_cache::cached_camera.is_valid()) {
                aimbot_cache::cached_camera_pos = aimbot_cache::cached_camera.getPos();
            }
            aimbot_cache::cached_screen_dims = functions::get_screen_dimensions();
            aimbot_cache::last_cache_update = now;
        }

        if (!found_target || !features::aimbot::stickyaim) {
            roblox::player new_target = functions::get_target_closest_to_mouse_with_fov();

            if (functions::is_valid_target(new_target)) {
                std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
                aimbot_cache::last_target = aimbot_cache::current_target;
                aimbot_cache::current_target = new_target;
                found_target = true;
            }
            else {
                if (!features::aimbot::stickyaim) {
                    std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
                    aimbot_cache::current_target = {};
                    found_target = false;
                }
                Sleep(1);
                continue;
            }
        }

        roblox::player target;
        {
            std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
            target = aimbot_cache::current_target;
        }

        if (!functions::is_valid_target(target)) {
            std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
            aimbot_cache::current_target = {};
            found_target = false;
            continue;
        }

        roblox::instance target_part = functions::get_target_part(target, aimbot_cache::cached_cursor_pos);

        if (target_part.address == 0) {
            if (!features::aimbot::stickyaim) {
                std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
                aimbot_cache::current_target = {};
                found_target = false;
            }
            continue;
        }

        Vec3 target_pos = target_part.get_pos();
        //target_pos = functions::calculate_prediction_enhanced(target_pos, target_part);


        if (features::aimbot::predictions) {
            target_pos = functions::calculate_silent_prediction(target_pos, target);
        }

        Vec2 screen_coords = roblox::worldtoscreen(target_pos);
        if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) {
            if (!features::aimbot::stickyaim) {
                std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
                aimbot_cache::current_target = {};
                found_target = false;
            }
            continue;
        }

        if (!aimbot_cache::cached_camera.is_valid()) continue;

        if (features::aimbot::aimtype == 0) {
            functions::perform_camera_aimbot(target_pos, aimbot_cache::cached_camera_pos);
        }
        else {
            functions::perform_mouse_aimbot(screen_coords);
        }

        if (features::aimbot::smoothing || features::aimbot::aimtype == 1) {
            Sleep(1);
        }
    }

    std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
    aimbot_cache::current_target = {};
}

void runs::runs_triggerbot(bool firstrecieved) {
    auto last_fire_time = std::chrono::steady_clock::now();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    while (firstrecieved) {
        if (!firstrecieved) return;

        features::triggerbot::triggerbotbind.update();

        if (!features::triggerbot::enabled || !features::triggerbot::triggerbotbind.enabled || globals::menuactive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto player = functions::get_triggerbot_closest_target();
        if (!player.head.address) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        HWND hwnd = FindWindowA(0, "Roblox");
        if (hwnd == nullptr || !IsWindow(hwnd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        RECT window_rect;
        if (!GetWindowRect(hwnd, &window_rect)) continue;

        POINT point;
        if (!GetCursorPos(&point)) continue;

        if (point.x < window_rect.left || point.x > window_rect.right ||
            point.y < window_rect.top || point.y > window_rect.bottom) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (!ScreenToClient(hwnd, &point)) continue;

        Vec2 cursor_pos = { static_cast<float>(point.x), static_cast<float>(point.y) };
        float world_distance = 0.0f;

        bool target_found = functions::is_cursor_over_target_part(player, cursor_pos, world_distance);

        if (!target_found) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        if (features::triggerbot::rangecheck && world_distance > features::triggerbot::range) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto time_since_last_fire = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_fire_time).count();

        if (time_since_last_fire >= features::triggerbot::reactiontime) {
            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = 0;
            input.mi.dy = 0;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            input.mi.mouseData = 0;
            input.mi.dwExtraInfo = 0;
            input.mi.time = 0;

            SendInput(1, &input, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));

            last_fire_time = current_time;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}


void runs::runs_silentcache(bool firstrecieved) {
    bool found_target = false;
    roblox::player target;
    bool was_keybind_active = false;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HWND roblox_window = FindWindowA(0, "Roblox");

    while (firstrecieved) {
        if (!firstrecieved) return;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        features::silentaim::silentaimkeybind.update();

        bool keybind_active = features::silentaim::enabled && features::silentaim::silentaimkeybind.enabled && !globals::menuactive;

        if (!keybind_active) {
            if (was_keybind_active) {
                std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                silentaim_cache::data_ready = false;
                silentaim_cache::current_target = {};
                target = {};
                found_target = false;
                silentaim_cache::should_hit_target = true;
                silentaim_cache::was_enabled_last_frame = false;
            }
            was_keybind_active = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (!was_keybind_active) {
            std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
            silentaim_cache::current_target = {};
            found_target = false;
            silentaim_cache::was_enabled_last_frame = false;
        }
        was_keybind_active = true;

        auto now = std::chrono::steady_clock::now();
        bool should_update_cache = std::chrono::duration_cast<std::chrono::milliseconds>(now - silentaim_cache::last_cache_update).count() >= silentaim_cache::cache_refresh_ms;

        if (should_update_cache) {
            silentaim_cache::cached_cursor_pos = functions::get_mouse_position();
            silentaim_cache::cached_camera = functions::get_current_camera();
            if (silentaim_cache::cached_camera.is_valid()) {
                silentaim_cache::cached_camera_pos = silentaim_cache::cached_camera.getPos();
            }
            silentaim_cache::cached_screen_dims = functions::get_screen_dimensions();
            silentaim_cache::last_cache_update = now;
        }

        if (!found_target || !features::silentaim::stickyaim) {
            roblox::player new_target = functions::get_silent_target_closest_to_mouse(
                features::silentaim::drawfov,
                features::silentaim::fieldofview,
                features::silentaim::flags
            );

            if (new_target.head.address != 0) {
                std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                silentaim_cache::last_target = silentaim_cache::current_target;
                silentaim_cache::current_target = new_target;
                target = new_target;
                found_target = true;
                globals::cachedtarget = new_target;
            }
            else {
                if (!features::silentaim::stickyaim) {
                    found_target = false;
                }
                continue;
            }
        }

        {
            std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
            target = silentaim_cache::current_target;
        }

        if (found_target && target.head.address != 0) {
            if (features::silentaim::hitchance < 100.0f) {
                static std::mt19937 hc_gen(std::chrono::steady_clock::now().time_since_epoch().count());
                std::uniform_real_distribution<float> hc_distrib(0.01f, 99.99f);
                float hitchance_roll = hc_distrib(hc_gen);
                silentaim_cache::should_hit_target = hitchance_roll <= features::silentaim::hitchance;
            }
            else {
                silentaim_cache::should_hit_target = true;
            }

            if (features::silentaim::knockcheck) {
                auto body_effects = target.bodyeffects;
                if (body_effects.address && body_effects.findfirstchild("K.O").read_bool_value()) {
                    if (features::silentaim::autoswitch && !features::silentaim::stickyaim) {
                        found_target = false;
                        continue;
                    }
                }
            }

            roblox::instance target_part = functions::get_silent_part_from_player(target, silentaim_cache::cached_cursor_pos);

            if (target_part.address == 0) {
                if (!features::silentaim::stickyaim) {
                    found_target = false;
                }
                continue;
            }

            Vec3 target_pos = target_part.get_pos();

            if (features::aimbot::predictions) {
                target_pos = functions::calculate_silent_prediction(target_pos, target);
            }

            if (!silentaim_cache::should_hit_target && features::silentaim::hitchance < 100.0f) {
                static std::mt19937 miss_gen(std::chrono::steady_clock::now().time_since_epoch().count());
                std::uniform_real_distribution<float> miss_offset(-5.0f, 5.0f);
                target_pos.x += miss_offset(miss_gen);
                target_pos.y += miss_offset(miss_gen);
                target_pos.z += miss_offset(miss_gen);
            }

            if (globals::gamename != "Rivals") {
                Vec2 screen_coords = roblox::worldtoscreen(target_pos);
                if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) {
                    if (!features::silentaim::stickyaim) {
                        found_target = false;
                    }
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                    silentaim_cache::part_position = screen_coords;

                    Vec2 dimensions = silentaim_cache::cached_screen_dims;
                    POINT cursor_point;
                    GetCursorPos(&cursor_point);
                    ScreenToClient(roblox_window, &cursor_point);

                    silentaim_cache::cached_position_x = static_cast<uint64_t>(cursor_point.x);
                    silentaim_cache::cached_position_y = static_cast<uint64_t>(dimensions.y - std::abs(dimensions.y - (cursor_point.y)) - 58);
                    silentaim_cache::data_ready = true;
                    silentaim_cache::was_enabled_last_frame = true;
                }
            }
            else {
                {
                    std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                    silentaim_cache::cached_camera_pos = target_pos;
                    silentaim_cache::data_ready = true;
                    silentaim_cache::was_enabled_last_frame = true;
                }
            }
        }
        else {
            if (!features::silentaim::stickyaim) {
                std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                silentaim_cache::data_ready = false;
                found_target = false;
            }
        }
    }

    std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
    silentaim_cache::current_target = {};
}

void runs::runs_silentaimbot(bool firstrecieved) {
    roblox::instance mouse_service_instance;
    bool was_enabled = false;

    while (firstrecieved) {
        if (!firstrecieved) return;
        bool is_enabled1 = features::silentaim::enabled && features::silentaim::silentaimkeybind.enabled && !globals::menuactive;

        if (!is_enabled1) {

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        }
        if (globals::gamename != "Rivals") {
            bool is_enabled = features::silentaim::enabled && features::silentaim::silentaimkeybind.enabled && !globals::menuactive;


            if (!is_enabled) {
                if (was_enabled) {
                    for (int i = 0; i < 1; i++) {
                        globals::aim = globals::localplayer.findfirstchild("PlayerGui").findfirstchild("MainScreenGui").findfirstchild("Aim");
                        POINT cursor_point;
                        GetCursorPos(&cursor_point);
                        ScreenToClient(FindWindowA(0, "Roblox"), &cursor_point);
                        globals::aim.setFramePosX(static_cast<uint64_t>(cursor_point.x));
                        globals::aim.setFramePosY(static_cast<uint64_t>(cursor_point.y));
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                was_enabled = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            was_enabled = true;

            bool data_ready_local;
            Vector2 part_position_local;
            uint64_t cached_x_local, cached_y_local;
            roblox::player current_target_local;
            bool was_enabled_last_frame_local;

            {
                std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                data_ready_local = silentaim_cache::data_ready;
                part_position_local = silentaim_cache::part_position;
                cached_x_local = silentaim_cache::cached_position_x;
                cached_y_local = silentaim_cache::cached_position_y;
                current_target_local = silentaim_cache::current_target;
                was_enabled_last_frame_local = silentaim_cache::was_enabled_last_frame;
            }

            globals::aim = globals::localplayer.findfirstchild("PlayerGui").findfirstchild("MainScreenGui").findfirstchild("Aim");

            if (current_target_local.head.address != 0 && data_ready_local && was_enabled_last_frame_local) {
                if (features::silentaim::spoofmouse) {
                    globals::aim.setFramePosX(cached_x_local);
                    globals::aim.setFramePosY(cached_y_local);
                    mouse_service_instance.initialize_mouse_service(globals::mouseservice);
                    mouse_service_instance.write_mouse_position(globals::mouseservice, part_position_local.x, part_position_local.y);
                    globals::aim.setFramePosX(cached_x_local);
                    globals::aim.setFramePosY(cached_y_local);
                }
                else {
                    mouse_service_instance.initialize_mouse_service(globals::mouseservice);
                    mouse_service_instance.write_mouse_position(globals::mouseservice, part_position_local.x, part_position_local.y);
                }
            }
        }
        else {
            bool is_enabled = features::silentaim::enabled && features::silentaim::silentaimkeybind.enabled && !globals::menuactive;

            if (!is_enabled) {
                was_enabled = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            was_enabled = true;

            bool data_ready_local;
            roblox::player current_target_local;
            bool was_enabled_last_frame_local;
            Vec3 target_world_pos;

            {
                std::lock_guard<std::mutex> lock(silentaim_cache::target_mutex);
                data_ready_local = silentaim_cache::data_ready;
                target_world_pos = silentaim_cache::cached_camera_pos;
                current_target_local = silentaim_cache::current_target;
                was_enabled_last_frame_local = silentaim_cache::was_enabled_last_frame;
            }

            if (current_target_local.head.address != 0 && data_ready_local && was_enabled_last_frame_local) {
                roblox::camera current_camera = functions::get_current_camera();
                if (current_camera.is_valid()) {
                       
                //        functions::perform_camera_aimbot(target_world_pos, current_camera.getPos());
                     //   current_camera.writeRot(globals::lp.hrp.read_part_cframe());
                  
                }
            }
        }
    }
}




























































void cache::hook_maincache(bool firstreceived) {
    while (true) {
      // auto adornee = globals::localplayer.readAdornee();

       
   //    globals::lp.main.readAdornee().GetAttribute("Highlight");
        Sleep(5345345);
    }

}