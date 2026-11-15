#pragma once
#include "../modules.h"
#include "../../../globals.h"
#include <random>

namespace silentaim_cache {
    inline roblox::player current_target = {};
    inline roblox::player last_target = {};
    inline std::mutex target_mutex;
    inline Vec3 cached_camera_pos = {};
    inline Vec2 cached_cursor_pos = {};
    inline Vec2 cached_screen_dims = {};
    inline roblox::camera cached_camera = {};
    inline std::chrono::steady_clock::time_point last_cache_update = std::chrono::steady_clock::now();
    inline const int cache_refresh_ms = 16;
    inline Vector2 part_position = {};
    inline uint64_t cached_position_x = 0;
    inline uint64_t cached_position_y = 0;
    inline bool data_ready = false;
    inline bool should_hit_target = true;
    inline bool was_enabled_last_frame = false;
}
