#pragma once

#include <vector>
#include <chrono>
#include <algorithm>
#include <vector>

#include <chrono>

#include <algorithm>

#include <vector>

#include <mutex>
#include <string>
#include <unordered_map>

#include <chrono>

#include <algorithm>

#include "../../../../backend/classes/classes.h"

namespace player_trail {

    struct SimpleTrailPoint {
        Vec3 position;
        float age;

        SimpleTrailPoint(const Vec3& pos);
    };

    namespace settings {
        extern bool enabled;
        extern float trail_duration;
        extern float trail_thickness;
        extern float trail_color[4];
        extern bool rainbow_trail;
        extern bool fade_out;
        extern float point_distance;
        extern int max_points;
        extern bool visibility_check;

        // Trails for players marked Enemy in the Player List.
        extern bool enemy_enabled;
        extern float enemy_color[4];
    }

    extern std::vector<SimpleTrailPoint> trail_points;
    extern Vec3 last_position;
    extern bool initialized;

    void initialize();
    void shutdown();
    void update_cache();
    void render_trail();
    bool is_point_behind_player(const Vec3& trail_point, const Vec3& player_pos, const Vec3& camera_pos);
}

namespace runs {
    void runs_player_trail_cache(bool firstrecieved);
}