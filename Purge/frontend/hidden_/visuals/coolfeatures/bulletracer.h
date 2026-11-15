#pragma once
#include <vector>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include "../../../../includes.h"
#include "../../../../backend/classes/classes.h"
#include "../../../../backend/imgui/imgui/imgui.h"

#define max
#undef max
#define min
#undef min

namespace bullet_tracer {
    struct BulletTracer {
        Vec3 start_position;
        Vec3 end_position;
        std::chrono::steady_clock::time_point creation_time;
        float duration = 2.0f;
        ImU32 color = IM_COL32(255, 255, 0, 255);
        float thickness = 2.0f;
        bool active = true;

        BulletTracer(const Vec3& start, const Vec3& end);
        bool is_expired() const;
        float get_alpha() const;
    };

    struct TracerCache {
        roblox::player target_player;
        Vec3 target_part_position;
        Vec3 camera_position;
        Vec3 damage_position;
        float target_health = 100.0f;
        bool mouse_held = false;
        bool was_mouse_held = false;
        bool mouse_pressed = false;
        bool damage_detected = false;
        float health_difference = 0.0f;
        std::chrono::steady_clock::time_point last_health_check;
        std::chrono::steady_clock::time_point last_shot_time;
        std::chrono::steady_clock::time_point last_click_time;
        std::chrono::steady_clock::time_point damage_time;

        TracerCache();
    };

    struct PlayerHealthTracker {
        uintptr_t player_address;
        float last_health;
        std::chrono::steady_clock::time_point last_update;
        bool valid;

        PlayerHealthTracker();
    };

    inline std::vector<BulletTracer> active_tracers;
    inline std::mutex tracer_mutex;
    inline TracerCache tracer_cache;
    inline std::mutex cache_mutex;
    inline std::unordered_map<uintptr_t, PlayerHealthTracker> health_trackers;
    inline std::mutex health_tracker_mutex;

    namespace settings {
        inline bool enabled = false;
        inline float tracer_duration = 5.0f;
        inline float tracer_thickness = 1.5f;
        inline float tracer_color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
        inline bool rainbow_tracers = false;
        inline bool fade_out = false;
        inline float detection_range = 1000.0f;
        inline float health_threshold = 1.0f;
    }

    void initialize();
    void shutdown();
    void update_cache();
    void detect_shots();
    void add_tracer(const Vec3& start, const Vec3& end);
    void cleanup_expired_tracers();
    void render_tracers();
}