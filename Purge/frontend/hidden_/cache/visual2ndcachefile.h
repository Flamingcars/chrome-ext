#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include "../../../imgui/imgui.h"
#include "../visuals/visuals.h"


namespace roblox {
    class player;
}

struct ChinaHatLine {
    ImVec2 start;
    ImVec2 end;
    ImU32 color;
    float thickness;
    bool valid;

 //   ChinaHatLine();
};

struct ChinaHatRenderData {
    std::vector<ChinaHatLine> brim_lines;
    std::vector<ChinaHatLine> connecting_lines;
    uintptr_t player_address;
    bool valid;
    bool teammate = false;
    float distance;

 //   ChinaHatRenderData();
};

struct SkeletonLine {
    ImVec2 start;
    ImVec2 end;
    ImU32 color;
    float thickness;
    bool valid;

    //SkeletonLine();
};

struct SkeletonRenderData {
    std::vector<SkeletonLine> skeleton_lines;
    uintptr_t player_address;
    bool valid;
    bool teammate = false;

   // SkeletonRenderData();
};

class ChinaHatCache {
private:
    std::vector<ChinaHatRenderData> render_data;
    std::mutex cache_mutex;
    std::chrono::steady_clock::time_point last_update;

    void calculate_china_hat_lines(const roblox::player& player, ChinaHatRenderData& hat_data);

public:
    ChinaHatCache();
    void update();
    std::vector<ChinaHatRenderData> get_render_data();
};

class SkeletonCache {
private:
    std::vector<SkeletonRenderData> render_data;
    std::mutex cache_mutex;
    std::chrono::steady_clock::time_point last_update;

    void calculate_r6_skeleton_lines(const roblox::player& player, SkeletonRenderData& skel_data);
    void calculate_r15_skeleton_lines(const roblox::player& player, SkeletonRenderData& skel_data);

public:
    SkeletonCache();
    void update();
    std::vector<SkeletonRenderData> get_render_data();
};

inline ChinaHatCache china_hat_cache;
inline SkeletonCache skeleton_cache;

void update_china_hat_cache();
void update_skeleton_cache();

std::vector<ChinaHatRenderData> get_cached_china_hat_render_data();
std::vector<SkeletonRenderData> get_cached_skeleton_render_data();