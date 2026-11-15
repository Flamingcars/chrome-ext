#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <atomic>
#include <shared_mutex>
#include <array>
#include "../../../globals.h"


#define max
#undef max
#define min
#undef min
namespace wallcheck {

    struct SpatialGrid {
        static constexpr int GRID_SIZE = 64;
        static constexpr float CELL_SIZE = 100.0f;
        std::array<std::array<std::array<std::vector<size_t>, GRID_SIZE>, GRID_SIZE>, GRID_SIZE> grid;

        int world_to_grid(float coord) const {
            int result = static_cast<int>((coord + CELL_SIZE * GRID_SIZE * 0.5f) / CELL_SIZE);
            return std::clamp(result, 0, GRID_SIZE - 1);
        }

        void clear() {
            for (auto& layer : grid) {
                for (auto& row : layer) {
                    for (auto& cell : row) {
                        cell.clear();
                    }
                }
            }
        }
    };

    struct AABB {
        Vector3 min, max;

        AABB() : min{ 1e6f, 1e6f, 1e6f }, max{ -1e6f, -1e6f, -1e6f } {}

        AABB(const Vector3& center, const Vector3& size) {
            Vector3 half_size = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
            min = { center.x - half_size.x, center.y - half_size.y, center.z - half_size.z };
            max = { center.x + half_size.x, center.y + half_size.y, center.z + half_size.z };
        }

        bool intersects_ray(const Vector3& origin, const Vector3& dir, float t_max) const {
            float t1 = (min.x - origin.x) / dir.x;
            float t2 = (max.x - origin.x) / dir.x;
            if (t1 > t2) std::swap(t1, t2);

            float tmin = t1;
            float tmax = t2;

            t1 = (min.y - origin.y) / dir.y;
            t2 = (max.y - origin.y) / dir.y;
            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            if (tmin > tmax) return false;

            t1 = (min.z - origin.z) / dir.z;
            t2 = (max.z - origin.z) / dir.z;
            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            return tmin <= tmax && tmax >= 0.0f && tmin <= t_max;
        }
    };

    struct OptimizedWall {
        Vector3 position;
        Vector3 size;
        Matrix3 rotation;
        AABB aabb;
        float distance_sq_from_origin;

        OptimizedWall(const roblox::wall& wall) : position(wall.position), size(wall.size), rotation(wall.rotation) {
            aabb = AABB(position, size);
            distance_sq_from_origin = position.x * position.x + position.y * position.y + position.z * position.z;
        }
    };

    struct VisibilityResult {
        std::vector<roblox::player> visible_players;
        std::chrono::steady_clock::time_point timestamp;
    };

    class ThreadSafeCache {
    public:
        ThreadSafeCache(size_t max_size = 50000, std::chrono::milliseconds lifetime = std::chrono::milliseconds(300))
            : max_size_(max_size), lifetime_(lifetime) {
        }

        uint64_t hash_ray(const Vector3& from, const Vector3& to) const {
            auto quantize = [](float f) -> uint32_t {
                return static_cast<uint32_t>(std::round(f * 50.0f));
                };

            uint64_t h1 = (static_cast<uint64_t>(quantize(from.x)) << 32) | quantize(from.y);
            uint64_t h2 = (static_cast<uint64_t>(quantize(from.z)) << 32) | quantize(to.x);
            uint64_t h3 = (static_cast<uint64_t>(quantize(to.y)) << 32) | quantize(to.z);

            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }

        bool get(const Vector3& from, const Vector3& to, bool& result) {
            std::shared_lock<std::shared_mutex> lock(mutex_);

            uint64_t key = hash_ray(from, to);
            auto it = cache_.find(key);

            if (it != cache_.end()) {
                auto now = std::chrono::steady_clock::now();
                if (now - it->second.timestamp < lifetime_) {
                    result = it->second.is_obstructed;
                    return true;
                }
            }

            return false;
        }

        void put(const Vector3& from, const Vector3& to, bool result) {
            std::unique_lock<std::shared_mutex> lock(mutex_);

            if (cache_.size() >= max_size_) {
                auto now = std::chrono::steady_clock::now();
                for (auto it = cache_.begin(); it != cache_.end();) {
                    if (now - it->second.timestamp >= lifetime_) {
                        it = cache_.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                if (cache_.size() >= max_size_) {
                    auto oldest = std::min_element(cache_.begin(), cache_.end(),
                        [](const auto& a, const auto& b) {
                            return a.second.timestamp < b.second.timestamp;
                        });
                    cache_.erase(oldest);
                }
            }

            uint64_t key = hash_ray(from, to);
            cache_[key] = { result, std::chrono::steady_clock::now() };
        }

        void clear() {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            cache_.clear();
        }

    private:
        struct CacheEntry {
            bool is_obstructed;
            std::chrono::steady_clock::time_point timestamp;
        };

        std::unordered_map<uint64_t, CacheEntry> cache_;
        mutable std::shared_mutex mutex_;
        size_t max_size_;
        std::chrono::milliseconds lifetime_;
    };

    class WallChecker {
    public:
        WallChecker();
        ~WallChecker();

        void start();
        void stop();
        bool can_see(const Vector3& from, const Vector3& to);
        std::vector<roblox::player> get_visible_players();

    private:
        void wall_scanning_thread();
        void visibility_checking_thread();
        bool ray_intersects_aabb(const Vector3& origin, const Vector3& target, const OptimizedWall& wall);
        bool ray_intersects_obb(const Vector3& origin, const Vector3& target, const OptimizedWall& wall);
        void update_spatial_grid();
        std::vector<size_t> get_nearby_walls(const Vector3& from, const Vector3& to);

        std::unique_ptr<ThreadSafeCache> cache_;
        std::vector<OptimizedWall> walls_;
        std::unique_ptr<SpatialGrid> spatial_grid_;
        VisibilityResult visibility_result_;

        std::thread wall_scanner_thread_;
        std::thread visibility_thread_;

        std::atomic<bool> should_stop_;
        std::atomic<bool> is_running_;
        std::atomic<bool> walls_updated_;

        mutable std::shared_mutex walls_mutex_;
        mutable std::shared_mutex visibility_mutex_;

        static constexpr float MIN_DISTANCE_THRESHOLD = 0.1f;
        static constexpr float MAX_WALL_DISTANCE = 1000.0f;
        static constexpr int MAX_WALLS_TO_CHECK = 50;
    };

    extern std::unique_ptr<WallChecker> g_wall_checker;

    inline bool is_valid_vector(const Vector3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
            !std::isnan(v.x) && !std::isnan(v.y) && !std::isnan(v.z) &&
            std::abs(v.x) < 1e5f && std::abs(v.y) < 1e5f && std::abs(v.z) < 1e5f;
    }

    inline bool is_valid_matrix(const Matrix3& m) {
        for (int i = 0; i < 9; ++i) {
            float val = m.data[i];
            if (!std::isfinite(val) || std::isnan(val) || std::abs(val) > 5.0f) {
                return false;
            }
        }
        return true;
    }

    constexpr bool is_valid_address(uintptr_t address) {
        return address > 0x1000 && address < 0x7FFFFFFFFFFF;
    }

    constexpr float distance_squared(const Vector3& a, const Vector3& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dz = b.z - a.z;
        return dx * dx + dy * dy + dz * dz;
    }

    void initialize();
    void shutdown();
    bool can_see(const Vector3& from, const Vector3& to);
    std::vector<roblox::player> get_visible_players();
}