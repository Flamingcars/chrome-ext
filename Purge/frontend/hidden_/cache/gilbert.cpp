#include <algorithm>
#include <cmath>
#include "visual2ndcachefile.h"
#include "../functions.h"

void SkeletonCache::calculate_r15_skeleton_lines(const roblox::player& player, SkeletonRenderData& skel_data) {
    try {
        ImU32 skeleton_color = IM_COL32(visuals::settings::skeletonscolor[0] * 255, visuals::settings::skeletonscolor[1] * 255, visuals::settings::skeletonscolor[2] * 255, visuals::settings::skeletonscolor[3] * 255);
        ImU32 outline_color = IM_COL32(0, 0, 0, 255);
        float thickness = 2.0f;

        auto dimensions = globals::visualengine.get_dimensions();
        auto is_on_screen = [&dimensions](const Vector2& pos) {
            return pos.x > 0 && pos.x < dimensions.x && pos.y > 0 && pos.y < dimensions.y;
            };

        skel_data.skeleton_lines.clear();
        skel_data.skeleton_lines.reserve(40);

        std::vector<Vector2> bone_screens(15);
        std::vector<bool> bone_valid(15, false);

        if (player.head.is_valid()) {
            bone_screens[0] = roblox::worldtoscreen(player.head.get_pos());
            bone_valid[0] = (bone_screens[0].x != -1 && bone_screens[0].y != -1);
        }

        if (player.uppertorso.is_valid()) {
            bone_screens[1] = roblox::worldtoscreen(player.uppertorso.get_pos());
            bone_valid[1] = (bone_screens[1].x != -1 && bone_screens[1].y != -1);
        }

        if (player.lowertorso.is_valid()) {
            bone_screens[2] = roblox::worldtoscreen(player.lowertorso.get_pos());
            bone_valid[2] = (bone_screens[2].x != -1 && bone_screens[2].y != -1);
        }

        if (player.leftupperarm.is_valid()) {
            bone_screens[3] = roblox::worldtoscreen(player.leftupperarm.get_pos());
            bone_valid[3] = (bone_screens[3].x != -1 && bone_screens[3].y != -1);
        }

        if (player.leftlowerarm.is_valid()) {
            bone_screens[4] = roblox::worldtoscreen(player.leftlowerarm.get_pos());
            bone_valid[4] = (bone_screens[4].x != -1 && bone_screens[4].y != -1);
        }

        if (player.rightupperarm.is_valid()) {
            bone_screens[5] = roblox::worldtoscreen(player.rightupperarm.get_pos());
            bone_valid[5] = (bone_screens[5].x != -1 && bone_screens[5].y != -1);
        }

        if (player.rightlowerarm.is_valid()) {
            bone_screens[6] = roblox::worldtoscreen(player.rightlowerarm.get_pos());
            bone_valid[6] = (bone_screens[6].x != -1 && bone_screens[6].y != -1);
        }

        if (player.leftupperleg.is_valid()) {
            bone_screens[7] = roblox::worldtoscreen(player.leftupperleg.get_pos());
            bone_valid[7] = (bone_screens[7].x != -1 && bone_screens[7].y != -1);
        }

        if (player.leftlowerleg.is_valid()) {
            bone_screens[8] = roblox::worldtoscreen(player.leftlowerleg.get_pos());
            bone_valid[8] = (bone_screens[8].x != -1 && bone_screens[8].y != -1);
        }

        if (player.rightupperleg.is_valid()) {
            bone_screens[9] = roblox::worldtoscreen(player.rightupperleg.get_pos());
            bone_valid[9] = (bone_screens[9].x != -1 && bone_screens[9].y != -1);
        }

        if (player.rightlowerleg.is_valid()) {
            bone_screens[10] = roblox::worldtoscreen(player.rightlowerleg.get_pos());
            bone_valid[10] = (bone_screens[10].x != -1 && bone_screens[10].y != -1);
        }

        if (player.leftfoot.is_valid()) {
            bone_screens[11] = roblox::worldtoscreen(player.leftfoot.get_pos());
            bone_valid[11] = (bone_screens[11].x != -1 && bone_screens[11].y != -1);
        }

        if (player.rightfoot.is_valid()) {
            bone_screens[12] = roblox::worldtoscreen(player.rightfoot.get_pos());
            bone_valid[12] = (bone_screens[12].x != -1 && bone_screens[12].y != -1);
        }

        if (player.lefthand.is_valid()) {
            bone_screens[13] = roblox::worldtoscreen(player.lefthand.get_pos());
            bone_valid[13] = (bone_screens[13].x != -1 && bone_screens[13].y != -1);
        }

        if (player.righthand.is_valid()) {
            bone_screens[14] = roblox::worldtoscreen(player.righthand.get_pos());
            bone_valid[14] = (bone_screens[14].x != -1 && bone_screens[14].y != -1);
        }


        auto add_outlined_line = [&](const Vector2& start, const Vector2& end) {
            if (is_on_screen(start) && is_on_screen(end)) {
  
                SkeletonLine outline;
                outline.start = ImVec2(start.x, start.y);
                outline.end = ImVec2(end.x, end.y);
                outline.color = outline_color;
                outline.thickness = thickness + 1.5f;
                outline.valid = true;
                skel_data.skeleton_lines.push_back(outline);

                SkeletonLine line;
                line.start = ImVec2(start.x, start.y);
                line.end = ImVec2(end.x, end.y);
                line.color = skeleton_color;
                line.thickness = thickness;
                line.valid = true;
                skel_data.skeleton_lines.push_back(line);
            }
            };


        struct Connection { int from, to; };
        std::vector<Connection> connections = {
            {0, 1},
            {1, 2},   
            {1, 3}, 
            {1, 5},  
            {3, 4},   
            {5, 6},   
            {4, 13},
            {6, 14}, 
            {2, 7}, 
            {2, 9},  
            {7, 8}, 
            {9, 10},  
            {8, 11},  
            {10, 12} 
        };

        for (const auto& conn : connections) {
            if (bone_valid[conn.from] && bone_valid[conn.to]) {
                add_outlined_line(bone_screens[conn.from], bone_screens[conn.to]);
            }
        }

        skel_data.valid = !skel_data.skeleton_lines.empty();

    }
    catch (...) {
        skel_data.valid = false;
    }
}

ChinaHatCache::ChinaHatCache() : last_update(std::chrono::steady_clock::now()) {}

void ChinaHatCache::calculate_china_hat_lines(const roblox::player& player, ChinaHatRenderData& hat_data) {
    try {
        if (!player.head.is_valid()) return;

        auto camera = functions::get_current_camera();
        if (!camera.is_valid()) return;

        Vec3 camera_pos = camera.getPos();
        Vec3 head_pos = player.head.get_pos();
        float distance = camera_pos.distance(head_pos);

        Vec3 head_size;
        try {
            head_size = player.head.get_part_size();
        }
        catch (...) {
            head_size = Vec3(2.0f, 2.0f, 2.0f);
        }

        float distance_scale = std::max(0.5f, std::min(1.5f, 75.0f / distance));
        float base_radius = std::max(head_size.x, head_size.z) * 0.75f * distance_scale; 

        Vector3 hat_tip = Vector3(head_pos.x, head_pos.y + head_size.y * 1.0f, head_pos.z); 
        Vector3 hat_base_center = Vector3(head_pos.x, head_pos.y + head_size.y * 0.3f, head_pos.z); 

        int segments = 12;
        float thickness = 1.0f;

        auto tip_screen = roblox::worldtoscreen(hat_tip);
        if (tip_screen.x == -1 || tip_screen.y == -1) return;

        std::vector<Vector3> base_points_3d;
        std::vector<ImVec2> base_points_screen;

        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * M_PI * i / segments;
            Vector3 point_3d = {
                hat_base_center.x + base_radius * cos(theta),
                hat_base_center.y,
                hat_base_center.z + base_radius * sin(theta)
            };
            base_points_3d.push_back(point_3d);

            auto screen_point = roblox::worldtoscreen(Vec3(point_3d.x, point_3d.y, point_3d.z));
            if (screen_point.x != -1 && screen_point.y != -1) {
                base_points_screen.push_back(ImVec2(screen_point.x, screen_point.y));
            }
        }

        if (base_points_screen.size() < 3) return;

        ImU32 brim_outline_color = IM_COL32(visuals::settings::chinahatcolor[0] * 255, visuals::settings::chinahatcolor[1] * 255, visuals::settings::chinahatcolor[2] * 255, visuals::settings::chinahatcolor[3] * 255);
        ImU32 line_color = IM_COL32(visuals::settings::chinahatcolor[0] * 255, visuals::settings::chinahatcolor[1] * 255, visuals::settings::chinahatcolor[2] * 255, visuals::settings::chinahatcolor[3] * 255);
        ImU32 black_outline = IM_COL32(0, 0, 0, 255);

        hat_data.brim_lines.clear();
        hat_data.brim_lines.reserve(base_points_screen.size() * 2);

        for (size_t i = 0; i < base_points_screen.size() - 1; i++) {
            ChinaHatLine outline_line;
            outline_line.start = base_points_screen[i];
            outline_line.end = base_points_screen[i + 1];
            outline_line.color = black_outline;
            outline_line.thickness = thickness + 1.5f;
            outline_line.valid = true;
            hat_data.brim_lines.push_back(outline_line);

            ChinaHatLine brim_line;
            brim_line.start = base_points_screen[i];
            brim_line.end = base_points_screen[i + 1];
            brim_line.color = brim_outline_color;
            brim_line.thickness = thickness;
            brim_line.valid = true;
            hat_data.brim_lines.push_back(brim_line);
        }

        hat_data.connecting_lines.clear();
        hat_data.connecting_lines.reserve(base_points_3d.size());
        hat_data.distance = distance_scale;
        for (int i = 0; i < base_points_3d.size(); i ++) {
            auto base_point = roblox::worldtoscreen(Vec3(base_points_3d[i].x, base_points_3d[i].y, base_points_3d[i].z));
            if (base_point.x != -1 && base_point.y != -1) {
        
                ChinaHatLine outline_connecting;
                outline_connecting.start = ImVec2(base_point.x, base_point.y);
                outline_connecting.end = ImVec2(tip_screen.x, tip_screen.y);
                outline_connecting.color = black_outline;
                outline_connecting.thickness = thickness + 1.0f;
                outline_connecting.valid = true;
                hat_data.connecting_lines.push_back(outline_connecting);


                ChinaHatLine connecting_line;
                connecting_line.start = ImVec2(base_point.x, base_point.y);
                connecting_line.end = ImVec2(tip_screen.x, tip_screen.y);
                connecting_line.color = line_color;
                connecting_line.thickness = thickness;
                connecting_line.valid = true;
                hat_data.connecting_lines.push_back(connecting_line);
            }
        }

        hat_data.valid = true;

    }
    catch (...) {
        hat_data.valid = false;
    }
}

void ChinaHatCache::update() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_update).count();

    if (elapsed < 4166) return;
    last_update = now;

    render_data.clear();

    try {
        if (!globals::localplayer.is_valid()) return;

        for (auto& player : globals::cachedplayers) {
            if (!player.is_valid() || !player.is_alive()) continue;
            if (visuals::settings::espteamcheck && globals::is_teammate(player) &&
                !visuals::settings::esp_teamcolor) continue;

            ChinaHatRenderData hat_data;
            hat_data.player_address = player.address;
            hat_data.teammate = globals::is_teammate(player);

            calculate_china_hat_lines(player, hat_data);

            if (hat_data.valid) {
                render_data.push_back(hat_data);
            }
        }
    }
    catch (...) {
    }
}

std::vector<ChinaHatRenderData> ChinaHatCache::get_render_data() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    return render_data;
}

SkeletonCache::SkeletonCache() : last_update(std::chrono::steady_clock::now()) {}

void SkeletonCache::calculate_r6_skeleton_lines(const roblox::player& player, SkeletonRenderData& skel_data) {
    try {
        ImU32 skeleton_color = IM_COL32(visuals::settings::skeletonscolor[0] * 255, visuals::settings::skeletonscolor[1] * 255, visuals::settings::skeletonscolor[2] * 255, visuals::settings::skeletonscolor[3] * 255);
        ImU32 outline_color = IM_COL32(0, 0, 0, 255);
        float thickness = 2.0f;

        auto dimensions = globals::visualengine.get_dimensions();
        auto is_on_screen = [&dimensions](const Vector2& pos) {
            return pos.x > 0 && pos.x < dimensions.x && pos.y > 0 && pos.y < dimensions.y;
            };

        skel_data.skeleton_lines.clear();
        skel_data.skeleton_lines.reserve(20);

        Vector2 head_screen, hrp_screen, left_arm_screen, right_arm_screen, left_leg_screen, right_leg_screen;
        bool head_valid = false, hrp_valid = false, left_arm_valid = false, right_arm_valid = false, left_leg_valid = false, right_leg_valid = false;

        if (player.head.is_valid()) {
            head_screen = roblox::worldtoscreen(player.head.get_pos());
            head_valid = (head_screen.x != -1 && head_screen.y != -1);
        }

        if (player.hrp.is_valid()) {
            hrp_screen = roblox::worldtoscreen(player.hrp.get_pos());
            hrp_valid = (hrp_screen.x != -1 && hrp_screen.y != -1);
        }

        if (player.lefthand.is_valid()) {
            left_arm_screen = roblox::worldtoscreen(player.lefthand.get_pos());
            left_arm_valid = (left_arm_screen.x != -1 && left_arm_screen.y != -1);
        }

        if (player.righthand.is_valid()) {
            right_arm_screen = roblox::worldtoscreen(player.righthand.get_pos());
            right_arm_valid = (right_arm_screen.x != -1 && right_arm_screen.y != -1);
        }

        if (player.leftfoot.is_valid()) {
            left_leg_screen = roblox::worldtoscreen(player.leftfoot.get_pos());
            left_leg_valid = (left_leg_screen.x != -1 && left_leg_screen.y != -1);
        }

        if (player.rightfoot.is_valid()) {
            right_leg_screen = roblox::worldtoscreen(player.rightfoot.get_pos());
            right_leg_valid = (right_leg_screen.x != -1 && right_leg_screen.y != -1);
        }

        auto add_outlined_line = [&](const Vector2& start, const Vector2& end) {
            if (is_on_screen(start) && is_on_screen(end)) {
                SkeletonLine outline;
                outline.start = ImVec2(start.x, start.y);
                outline.end = ImVec2(end.x, end.y);
                outline.color = outline_color;
                outline.thickness = thickness + 1.5f;
                outline.valid = true;
                skel_data.skeleton_lines.push_back(outline);

                SkeletonLine line;
                line.start = ImVec2(start.x, start.y);
                line.end = ImVec2(end.x, end.y);
                line.color = skeleton_color;
                line.thickness = thickness;
                line.valid = true;
                skel_data.skeleton_lines.push_back(line);
            }
            };

        if (head_valid && hrp_valid) {
            add_outlined_line(head_screen, hrp_screen);
        }

        if (hrp_valid && left_arm_valid) {
            add_outlined_line(hrp_screen, left_arm_screen);
        }

        if (hrp_valid && right_arm_valid) {
            add_outlined_line(hrp_screen, right_arm_screen);
        }

        if (hrp_valid && left_leg_valid) {
            add_outlined_line(hrp_screen, left_leg_screen);
        }

        if (hrp_valid && right_leg_valid) {
            add_outlined_line(hrp_screen, right_leg_screen);
        }

        skel_data.valid = !skel_data.skeleton_lines.empty();

    }
    catch (...) {
        skel_data.valid = false;
    }
}


void SkeletonCache::update() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_update).count();

    if (elapsed < 4166) return; 
    last_update = now;

    render_data.clear();

    try {
        if (!globals::localplayer.is_valid()) return;

        for (auto& player : globals::cachedplayers) {
            if (!player.is_valid() || !player.is_alive()) continue;
            if (visuals::settings::espteamcheck && globals::is_teammate(player) &&
                !visuals::settings::esp_teamcolor) continue;

            SkeletonRenderData skel_data;
            skel_data.player_address = player.address;
            skel_data.teammate = globals::is_teammate(player);

            if (player.is_r15()) {
                calculate_r15_skeleton_lines(player, skel_data);
            }
            else {
                calculate_r6_skeleton_lines(player, skel_data);
            }

            if (skel_data.valid) {
                render_data.push_back(skel_data);
            }
        }
    }
    catch (...) {
   
    }
}

std::vector<SkeletonRenderData> SkeletonCache::get_render_data() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    return render_data;
}


void update_china_hat_cache() {
    china_hat_cache.update();
}

void update_skeleton_cache() {
    skeleton_cache.update();
}

std::vector<ChinaHatRenderData> get_cached_china_hat_render_data() {
    return china_hat_cache.get_render_data();
}

std::vector<SkeletonRenderData> get_cached_skeleton_render_data() {
    return skeleton_cache.get_render_data();
}