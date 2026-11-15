#include "../modules.h"
#include "../../../globals.h"
#include "../../../backend/usermode/usermode.h"
#include "../../../includes.h"
#include <immintrin.h>
#include <atomic>
#include <array>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#else
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#endif
#include "../visuals/visuals.h"

using namespace roblox;

namespace cache {
    static std::shared_mutex player_data_mutex;
    static std::shared_mutex esp_data_mutex;
    static std::mutex update_mutex;
    static std::atomic<bool> data_updating{ false };
}

class fast_allocator {
private:
    alignas(64) char* memory_base;
    size_t total_size;
    std::atomic<size_t> current_offset{ 0 };

public:
    fast_allocator(size_t arena_size) : total_size(arena_size) {
#ifdef _WIN32
        memory_base = static_cast<char*>(_aligned_malloc(arena_size, 64));
#else
        if (posix_memalign(reinterpret_cast<void**>(&memory_base), 64, arena_size) != 0) {
            memory_base = nullptr;
        }
#endif
    }

    ~fast_allocator() {
#ifdef _WIN32
        _aligned_free(memory_base);
#else
        free(memory_base);
#endif
    }

    template<typename T>
    T* allocate_aligned(size_t count = 1) {
        size_t bytes = sizeof(T) * count;
        size_t aligned_bytes = (bytes + 63) & ~63;
        size_t current_pos = current_offset.fetch_add(aligned_bytes, std::memory_order_acq_rel);

        if (current_pos + aligned_bytes > total_size) {
            current_offset.store(0, std::memory_order_release);
            current_pos = 0;
        }

        return reinterpret_cast<T*>(memory_base + current_pos);
    }

    void reset_allocator() {
        current_offset.store(0, std::memory_order_release);
    }

    bool is_valid() const { return memory_base != nullptr; }
};

struct alignas(64) player_data_cache {
    static constexpr size_t MAX_ENTITIES = 8192;
    static constexpr size_t GRID_SIZE = 256.0f;
    static constexpr size_t HASH_BUCKETS = 4096;

    alignas(32) std::array<float, MAX_ENTITIES> position_x;
    alignas(32) std::array<float, MAX_ENTITIES> position_y;
    alignas(32) std::array<float, MAX_ENTITIES> position_z;
    alignas(32) std::array<float, MAX_ENTITIES> head_x;
    alignas(32) std::array<float, MAX_ENTITIES> head_y;
    alignas(32) std::array<float, MAX_ENTITIES> head_z;
    alignas(32) std::array<float, MAX_ENTITIES> distance_squared;
    alignas(32) std::array<float, MAX_ENTITIES> health_ratio;
    alignas(32) std::array<uint32_t, MAX_ENTITIES> visibility_flags;
    alignas(32) std::array<uint16_t, MAX_ENTITIES> player_indices;
    alignas(32) std::array<uint16_t, MAX_ENTITIES> spatial_hashes;

    std::array<uint64_t, MAX_ENTITIES / 64 + 1> dirty_bits;
    std::array<std::vector<uint16_t>, HASH_BUCKETS> spatial_grid;

    std::atomic<uint32_t> entity_count{ 0 };
    std::atomic<uint64_t> frame_version{ 0 };
    std::atomic<bool> processing_complete{ false };

    player_data_cache() {
        memset(dirty_bits.data(), 0, dirty_bits.size() * sizeof(uint64_t));
        for (auto& bucket : spatial_grid) {
            bucket.reserve(32);
        }
    }

    void clear_cache() {
        entity_count.store(0, std::memory_order_release);
        frame_version.fetch_add(1, std::memory_order_acq_rel);
        memset(dirty_bits.data(), 0, dirty_bits.size() * sizeof(uint64_t));

        for (auto& bucket : spatial_grid) {
            bucket.clear();
        }

        processing_complete.store(false, std::memory_order_release);
    }

    __forceinline uint32_t compute_spatial_hash(float x, float z) const {
        uint32_t grid_x = static_cast<uint32_t>((x + 32768.0f) / GRID_SIZE);
        uint32_t grid_z = static_cast<uint32_t>((z + 32768.0f) / GRID_SIZE);
        return (grid_x * 73856093 ^ grid_z * 19349663) & (HASH_BUCKETS - 1);
    }

    void add_entity(const Vector3& pos, const Vector3& head_pos, float dist_sq,
        float health, uint32_t flags, uint16_t index) {
        uint32_t current_count = entity_count.load(std::memory_order_acquire);
        if (current_count >= MAX_ENTITIES) return;

        position_x[current_count] = pos.x;
        position_y[current_count] = pos.y;
        position_z[current_count] = pos.z;
        head_x[current_count] = head_pos.x;
        head_y[current_count] = head_pos.y;
        head_z[current_count] = head_pos.z;
        distance_squared[current_count] = dist_sq;
        health_ratio[current_count] = health;
        visibility_flags[current_count] = flags;
        player_indices[current_count] = index;

        uint32_t hash = compute_spatial_hash(pos.x, pos.z);
        spatial_hashes[current_count] = static_cast<uint16_t>(hash);
        spatial_grid[hash].push_back(current_count);

        entity_count.store(current_count + 1, std::memory_order_release);
    }

    void sort_by_distance() {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count <= 1) return;

        thread_local static std::vector<uint16_t> sort_indices;
        sort_indices.clear();
        sort_indices.reserve(count);

        for (uint16_t i = 0; i < count; ++i) {
            sort_indices.push_back(i);
        }

        std::sort(sort_indices.begin(), sort_indices.end(),
            [this](uint16_t a, uint16_t b) {
                return distance_squared[a] < distance_squared[b];
            });

        thread_local static std::array<float, MAX_ENTITIES> temp_pos_x;
        thread_local static std::array<float, MAX_ENTITIES> temp_pos_y;
        thread_local static std::array<float, MAX_ENTITIES> temp_pos_z;
        thread_local static std::array<float, MAX_ENTITIES> temp_head_x;
        thread_local static std::array<float, MAX_ENTITIES> temp_head_y;
        thread_local static std::array<float, MAX_ENTITIES> temp_head_z;
        thread_local static std::array<float, MAX_ENTITIES> temp_distance;
        thread_local static std::array<float, MAX_ENTITIES> temp_health;
        thread_local static std::array<uint32_t, MAX_ENTITIES> temp_flags;
        thread_local static std::array<uint16_t, MAX_ENTITIES> temp_indices;

        for (uint16_t i = 0; i < count; ++i) {
            uint16_t src_idx = sort_indices[i];
            temp_pos_x[i] = position_x[src_idx];
            temp_pos_y[i] = position_y[src_idx];
            temp_pos_z[i] = position_z[src_idx];
            temp_head_x[i] = head_x[src_idx];
            temp_head_y[i] = head_y[src_idx];
            temp_head_z[i] = head_z[src_idx];
            temp_distance[i] = distance_squared[src_idx];
            temp_health[i] = health_ratio[src_idx];
            temp_flags[i] = visibility_flags[src_idx];
            temp_indices[i] = player_indices[src_idx];
        }

        memcpy(position_x.data(), temp_pos_x.data(), count * sizeof(float));
        memcpy(position_y.data(), temp_pos_y.data(), count * sizeof(float));
        memcpy(position_z.data(), temp_pos_z.data(), count * sizeof(float));
        memcpy(head_x.data(), temp_head_x.data(), count * sizeof(float));
        memcpy(head_y.data(), temp_head_y.data(), count * sizeof(float));
        memcpy(head_z.data(), temp_head_z.data(), count * sizeof(float));
        memcpy(distance_squared.data(), temp_distance.data(), count * sizeof(float));
        memcpy(health_ratio.data(), temp_health.data(), count * sizeof(float));
        memcpy(visibility_flags.data(), temp_flags.data(), count * sizeof(uint32_t));
        memcpy(player_indices.data(), temp_indices.data(), count * sizeof(uint16_t));
    }

    void frustum_cull_simd(const Vector3& camera_pos, const Vector2& screen_dims) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        const __m256 cam_x = _mm256_set1_ps(camera_pos.x);
        const __m256 cam_y = _mm256_set1_ps(camera_pos.y);
        const __m256 cam_z = _mm256_set1_ps(camera_pos.z);
        const __m256 max_dist = _mm256_set1_ps(2500.0f * 2500.0f);
        const __m256 min_health = _mm256_set1_ps(0.01f);

        uint32_t visible_count = 0;

        uint32_t simd_limit = count - (count % 8);
        for (uint32_t i = 0; i < simd_limit; i += 8) {
            __m256 pos_x = _mm256_loadu_ps(&position_x[i]);
            __m256 pos_y = _mm256_loadu_ps(&position_y[i]);
            __m256 pos_z = _mm256_loadu_ps(&position_z[i]);
            __m256 health = _mm256_loadu_ps(&health_ratio[i]);

            __m256 dx = _mm256_sub_ps(pos_x, cam_x);
            __m256 dy = _mm256_sub_ps(pos_y, cam_y);
            __m256 dz = _mm256_sub_ps(pos_z, cam_z);

            __m256 dist_sq = _mm256_fmadd_ps(dx, dx, _mm256_fmadd_ps(dy, dy, _mm256_mul_ps(dz, dz)));

            __m256 distance_mask = _mm256_cmp_ps(dist_sq, max_dist, _CMP_LT_OQ);
            __m256 health_mask = _mm256_cmp_ps(health, min_health, _CMP_GE_OQ);
            __m256 final_mask = _mm256_and_ps(distance_mask, health_mask);

            int mask_bits = _mm256_movemask_ps(final_mask);

            for (int j = 0; j < 8; ++j) {
                if (mask_bits & (1 << j)) {
                    uint32_t src_idx = i + j;
                    if (visible_count != src_idx) {
                        position_x[visible_count] = position_x[src_idx];
                        position_y[visible_count] = position_y[src_idx];
                        position_z[visible_count] = position_z[src_idx];
                        head_x[visible_count] = head_x[src_idx];
                        head_y[visible_count] = head_y[src_idx];
                        head_z[visible_count] = head_z[src_idx];
                        distance_squared[visible_count] = distance_squared[src_idx];
                        health_ratio[visible_count] = health_ratio[src_idx];
                        visibility_flags[visible_count] = visibility_flags[src_idx];
                        player_indices[visible_count] = player_indices[src_idx];
                    }
                    visible_count++;
                }
            }
        }

        for (uint32_t i = simd_limit; i < count; ++i) {
            float dx = position_x[i] - camera_pos.x;
            float dy = position_y[i] - camera_pos.y;
            float dz = position_z[i] - camera_pos.z;
            float dist_sq = dx * dx + dy * dy + dz * dz;

            if (dist_sq < (2500.0f * 2500.0f) && health_ratio[i] >= 0.01f) {
                if (visible_count != i) {
                    position_x[visible_count] = position_x[i];
                    position_y[visible_count] = position_y[i];
                    position_z[visible_count] = position_z[i];
                    head_x[visible_count] = head_x[i];
                    head_y[visible_count] = head_y[i];
                    head_z[visible_count] = head_z[i];
                    distance_squared[visible_count] = distance_squared[i];
                    health_ratio[visible_count] = health_ratio[i];
                    visibility_flags[visible_count] = visibility_flags[i];
                    player_indices[visible_count] = player_indices[i];
                }
                visible_count++;
            }
        }

        entity_count.store(visible_count, std::memory_order_release);
        processing_complete.store(true, std::memory_order_release);
    }

    void batch_worldtoscreen(const Vector2& screen_dims) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        thread_local static std::array<Vector2, MAX_ENTITIES> screen_positions;
        thread_local static std::array<Vector2, MAX_ENTITIES> head_screen_positions;

        uint32_t simd_limit = count - (count % 8);

        for (uint32_t i = 0; i < simd_limit; i += 8) {
            __m256 pos_x = _mm256_loadu_ps(&position_x[i]);
            __m256 pos_y = _mm256_loadu_ps(&position_y[i]);
            __m256 pos_z = _mm256_loadu_ps(&position_z[i]);

            __m256 h_x = _mm256_loadu_ps(&head_x[i]);
            __m256 h_y = _mm256_loadu_ps(&head_y[i]);
            __m256 h_z = _mm256_loadu_ps(&head_z[i]);

            for (int j = 0; j < 8; ++j) {
                uint32_t idx = i + j;

                Vector3 world_pos = { pos_x.m256_f32[j], pos_y.m256_f32[j], pos_z.m256_f32[j] };
                Vector3 head_world_pos = { h_x.m256_f32[j], h_y.m256_f32[j], h_z.m256_f32[j] };

                screen_positions[idx] = roblox::worldtoscreen(world_pos);
                head_screen_positions[idx] = roblox::worldtoscreen(head_world_pos);

                const Vector2& screen = screen_positions[idx];
                if (screen.x > 0 && screen.y > 0 && screen.x < screen_dims.x && screen.y < screen_dims.y) {
                    visibility_flags[idx] |= 0x1;
                }
                else {
                    visibility_flags[idx] &= ~0x1;
                }
            }
        }

        for (uint32_t i = simd_limit; i < count; ++i) {
            Vector3 world_pos = { position_x[i], position_y[i], position_z[i] };
            Vector3 head_world_pos = { head_x[i], head_y[i], head_z[i] };

            screen_positions[i] = roblox::worldtoscreen(world_pos);
            head_screen_positions[i] = roblox::worldtoscreen(head_world_pos);

            const Vector2& screen = screen_positions[i];
            if (screen.x > 0 && screen.y > 0 && screen.x < screen_dims.x && screen.y < screen_dims.y) {
                visibility_flags[i] |= 0x1;
            }
            else {
                visibility_flags[i] &= ~0x1;
            }
        }
    }

    std::vector<uint16_t> query_spatial_area(const Vector3& center, float radius) const {
        std::vector<uint16_t> result;
        result.reserve(64);

        const float grid_radius = radius / GRID_SIZE + 1.0f;
        const int32_t start_x = static_cast<int32_t>((center.x - radius + 32768.0f) / GRID_SIZE);
        const int32_t end_x = static_cast<int32_t>((center.x + radius + 32768.0f) / GRID_SIZE);
        const int32_t start_z = static_cast<int32_t>((center.z - radius + 32768.0f) / GRID_SIZE);
        const int32_t end_z = static_cast<int32_t>((center.z + radius + 32768.0f) / GRID_SIZE);

        const float radius_sq = radius * radius;

        for (int32_t grid_x = start_x; grid_x <= end_x; ++grid_x) {
            for (int32_t grid_z = start_z; grid_z <= end_z; ++grid_z) {
                uint32_t hash = ((grid_x * 73856093) ^ (grid_z * 19349663)) & (HASH_BUCKETS - 1);

                for (uint16_t idx : spatial_grid[hash]) {
                    float dx = position_x[idx] - center.x;
                    float dy = position_y[idx] - center.y;
                    float dz = position_z[idx] - center.z;

                    if (dx * dx + dy * dy + dz * dz <= radius_sq) {
                        result.push_back(idx);
                    }
                }
            }
        }

        return result;
    }
};

struct alignas(64) screen_data_cache {
    static constexpr size_t SCREEN_ENTITIES = 2048;

    alignas(32) std::array<Vector2, SCREEN_ENTITIES> screen_positions;
    alignas(32) std::array<Vector2, SCREEN_ENTITIES> bounds_min;
    alignas(32) std::array<Vector2, SCREEN_ENTITIES> bounds_max;
    alignas(32) std::array<float, SCREEN_ENTITIES> box_margins;
    alignas(32) std::array<uint32_t, SCREEN_ENTITIES> screen_flags;

    std::atomic<uint32_t> screen_count{ 0 };
    std::atomic<uint64_t> screen_version{ 0 };

    void clear_screen() {
        screen_count.store(0, std::memory_order_release);
        screen_version.fetch_add(1, std::memory_order_acq_rel);
    }

    void add_screen_entity(const Vector2& screen_pos, const Vector2& b_min,
        const Vector2& b_max, float margin, uint32_t flags) {
        uint32_t current_count = screen_count.load(std::memory_order_acquire);
        if (current_count >= SCREEN_ENTITIES) return;

        screen_positions[current_count] = screen_pos;
        bounds_min[current_count] = b_min;
        bounds_max[current_count] = b_max;
        box_margins[current_count] = margin;
        screen_flags[current_count] = flags;

        screen_count.store(current_count + 1, std::memory_order_release);
    }

    void cull_screen_entities(const Vector2& screen_dims) {
        uint32_t count = screen_count.load(std::memory_order_acquire);
        if (count == 0) return;

        const __m256 screen_w = _mm256_set1_ps(screen_dims.x);
        const __m256 screen_h = _mm256_set1_ps(screen_dims.y);
        const __m256 zero = _mm256_setzero_ps();

        uint32_t visible_count = 0;
        uint32_t simd_limit = count - (count % 8);

        for (uint32_t i = 0; i < simd_limit; i += 8) {
            __m256 pos_x = _mm256_set_ps(
                screen_positions[i + 7].x, screen_positions[i + 6].x, screen_positions[i + 5].x,
                screen_positions[i + 4].x, screen_positions[i + 3].x, screen_positions[i + 2].x,
                screen_positions[i + 1].x, screen_positions[i + 0].x);

            __m256 pos_y = _mm256_set_ps(
                screen_positions[i + 7].y, screen_positions[i + 6].y, screen_positions[i + 5].y,
                screen_positions[i + 4].y, screen_positions[i + 3].y, screen_positions[i + 2].y,
                screen_positions[i + 1].y, screen_positions[i + 0].y);

            __m256 x_valid = _mm256_and_ps(_mm256_cmp_ps(pos_x, zero, _CMP_GE_OQ),
                _mm256_cmp_ps(pos_x, screen_w, _CMP_LT_OQ));

            __m256 y_valid = _mm256_and_ps(_mm256_cmp_ps(pos_y, zero, _CMP_GE_OQ),
                _mm256_cmp_ps(pos_y, screen_h, _CMP_LT_OQ));

            __m256 final_mask = _mm256_and_ps(x_valid, y_valid);

            int mask_bits = _mm256_movemask_ps(final_mask);

            for (int j = 0; j < 8; ++j) {
                uint32_t idx = i + j;
                if (mask_bits & (1 << j)) {
                    if (visible_count != idx) {
                        screen_positions[visible_count] = screen_positions[idx];
                        bounds_min[visible_count] = bounds_min[idx];
                        bounds_max[visible_count] = bounds_max[idx];
                        box_margins[visible_count] = box_margins[idx];
                        screen_flags[visible_count] = screen_flags[idx];
                    }
                    ++visible_count;
                }
            }
        }
        for (uint32_t i = simd_limit; i < count; ++i) {
            const Vector2& pos = screen_positions[i];
            if (pos.x >= 0 && pos.y >= 0 && pos.x < screen_dims.x && pos.y < screen_dims.y) {
                if (visible_count != i) {
                    screen_positions[visible_count] = screen_positions[i];
                    bounds_min[visible_count] = bounds_min[i];
                    bounds_max[visible_count] = bounds_max[i];
                    box_margins[visible_count] = box_margins[i];
                    screen_flags[visible_count] = screen_flags[i];
                }
                ++visible_count;
            }
        }

        screen_count.store(visible_count, std::memory_order_release);
    }
};

namespace optimization_helpers {
    alignas(32) static Vector2 cached_screen_dims{ -1.0f, -1.0f };
    static std::atomic<std::chrono::steady_clock::time_point> last_dims_update{ std::chrono::steady_clock::now() };

    __forceinline Vector2 get_screen_dimensions() {
        auto now = std::chrono::steady_clock::now();
        auto last = last_dims_update.load(std::memory_order_acquire);

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() > 100) {
            auto dims = globals::visualengine.get_dimensions();
            cached_screen_dims = dims;
            last_dims_update.store(now, std::memory_order_release);
            return dims;
        }

        return cached_screen_dims;
    }

    __forceinline bool is_on_screen(const Vector2& pos, const Vector2& dims) {
        return pos.x > 0 && pos.y > 0 && pos.x < dims.x && pos.y < dims.y;
    }

    bool calculate_player_bounds(const roblox::player& entity, Vector2& min_bounds,
        Vector2& max_bounds, float& margin) {
        if (!is_valid_address(entity.head.address)) return false;

        bool has_limbs = (is_valid_address(entity.lefthand.address) || is_valid_address(entity.leftupperarm.address)) &&
            (is_valid_address(entity.righthand.address) || is_valid_address(entity.rightupperarm.address)) &&
            (is_valid_address(entity.leftfoot.address) || is_valid_address(entity.leftupperleg.address)) &&
            (is_valid_address(entity.rightfoot.address) || is_valid_address(entity.rightupperleg.address));

        if (!has_limbs) return false;

        thread_local static std::array<Vector3, 6> world_points;
        thread_local static std::array<Vector2, 6> screen_points;

        world_points[0] = entity.head.get_pos();

        if (entity.rigtype == 1) {
            world_points[1] = entity.torso.address ? entity.torso.get_pos() : Vector3::zero();
        }
        else {
            world_points[1] = entity.uppertorso.address ? entity.uppertorso.get_pos() : Vector3::zero();
        }

        world_points[2] = entity.lefthand.address ? entity.lefthand.get_pos() : Vector3::zero();
        world_points[3] = entity.righthand.address ? entity.righthand.get_pos() : Vector3::zero();
        world_points[4] = entity.leftfoot.address ? entity.leftfoot.get_pos() : Vector3::zero();
        world_points[5] = entity.rightfoot.address ? entity.rightfoot.get_pos() : Vector3::zero();

        __m256 min_x = _mm256_set1_ps(FLT_MAX);
        __m256 min_y = _mm256_set1_ps(FLT_MAX);
        __m256 max_x = _mm256_set1_ps(-FLT_MAX);
        __m256 max_y = _mm256_set1_ps(-FLT_MAX);

        bool any_on_screen = false;
        for (size_t i = 0; i < 6; ++i) {
            screen_points[i] = roblox::worldtoscreen(world_points[i]);

            Vector2 dims = get_screen_dimensions();
            if (is_on_screen(screen_points[i], dims)) {
                any_on_screen = true;

                __m256 pos_x = _mm256_set1_ps(screen_points[i].x);
                __m256 pos_y = _mm256_set1_ps(screen_points[i].y);

                min_x = _mm256_min_ps(min_x, pos_x);
                min_y = _mm256_min_ps(min_y, pos_y);
                max_x = _mm256_max_ps(max_x, pos_x);
                max_y = _mm256_max_ps(max_y, pos_y);
            }
        }

        if (!any_on_screen) return false;

        float margin_calc = std::abs(screen_points[0].y - screen_points[1].y) * 0.45f;

        min_bounds.x = min_x.m256_f32[0] - margin_calc;
        min_bounds.y = min_y.m256_f32[0] - margin_calc;
        max_bounds.x = max_x.m256_f32[0] + margin_calc;
        max_bounds.y = max_y.m256_f32[0] + margin_calc;
        margin = margin_calc;

        return true;
    }
}

struct alignas(64) player_soa {
    static constexpr size_t CAPACITY = 4096;

    uintptr_t addresses[CAPACITY];
    instance mains[CAPACITY];
    instance humanoids[CAPACITY];
    instance heads[CAPACITY];
    instance hrps[CAPACITY];
    instance torsos[CAPACITY];
    instance uppertorsos[CAPACITY];
    instance lowertorsos[CAPACITY];
    instance lefthand[CAPACITY];
    instance righthand[CAPACITY];
    instance leftlowerarm[CAPACITY];
    instance rightlowerarm[CAPACITY];
    instance leftupperarm[CAPACITY];
    instance rightupperarm[CAPACITY];
    instance leftfoot[CAPACITY];
    instance rightfoot[CAPACITY];
    instance leftlowerleg[CAPACITY];
    instance rightlowerleg[CAPACITY];
    instance leftupperleg[CAPACITY];
    instance rightupperleg[CAPACITY];
    instance bodyeffects[CAPACITY];
    instance tools[CAPACITY];
    instance teams[CAPACITY];

    int healths[CAPACITY];
    int maxhealths[CAPACITY];
    int armors[CAPACITY];
    int rigtypes[CAPACITY];
    int statuses[CAPACITY];

    std::string names[CAPACITY];
    std::string displaynames[CAPACITY];
    std::string toolnames[CAPACITY];
    std::string flags[CAPACITY];

    size_t count;

    player_soa() : count(0) {
        memset(healths, 0, sizeof(healths));
        std::fill(std::begin(maxhealths), std::end(maxhealths), 100);
        memset(armors, 0, sizeof(armors));
        std::fill(std::begin(rigtypes), std::end(rigtypes), 1);
        memset(statuses, 0, sizeof(statuses));
    }
};

struct alignas(64) enhanced_player_cache {
    static constexpr size_t MAX_ENTITIES = 4096;
    static constexpr size_t GRID_SIZE = 256.0f;
    static constexpr size_t HASH_BUCKETS = 4096;

    alignas(32) std::array<float, MAX_ENTITIES> position_x;
    alignas(32) std::array<float, MAX_ENTITIES> position_y;
    alignas(32) std::array<float, MAX_ENTITIES> position_z;
    alignas(32) std::array<float, MAX_ENTITIES> head_x;
    alignas(32) std::array<float, MAX_ENTITIES> head_y;
    alignas(32) std::array<float, MAX_ENTITIES> head_z;
    alignas(32) std::array<float, MAX_ENTITIES> distance_squared;
    alignas(32) std::array<float, MAX_ENTITIES> health_ratio;
    alignas(32) std::array<uint32_t, MAX_ENTITIES> visibility_flags;

    alignas(32) std::array<float, MAX_ENTITIES> screen_x;
    alignas(32) std::array<float, MAX_ENTITIES> screen_y;
    alignas(32) std::array<float, MAX_ENTITIES> bounds_min_x;
    alignas(32) std::array<float, MAX_ENTITIES> bounds_min_y;
    alignas(32) std::array<float, MAX_ENTITIES> bounds_max_x;
    alignas(32) std::array<float, MAX_ENTITIES> bounds_max_y;
    alignas(32) std::array<float, MAX_ENTITIES> box_margins;

    std::array<uintptr_t, MAX_ENTITIES> player_addresses;
    std::array<std::string, MAX_ENTITIES> player_names;
    std::array<std::string, MAX_ENTITIES> tool_names;
    std::array<int, MAX_ENTITIES> player_healths;
    std::array<int, MAX_ENTITIES> player_max_healths;
    std::array<int, MAX_ENTITIES> player_armors;
    std::array<int, MAX_ENTITIES> rig_types;

    alignas(32) std::array<uint16_t, MAX_ENTITIES> spatial_hashes;
    std::array<std::vector<uint16_t>, HASH_BUCKETS> spatial_grid;

    std::atomic<uint32_t> entity_count{ 0 };
    std::atomic<uint64_t> frame_version{ 0 };
    std::atomic<bool> processing_complete{ false };

    enhanced_player_cache() {
        for (auto& bucket : spatial_grid) {
            bucket.reserve(32);
        }
    }

    void clear_cache() {
        entity_count.store(0, std::memory_order_release);
        frame_version.fetch_add(1, std::memory_order_acq_rel);

        for (auto& bucket : spatial_grid) {
            bucket.clear();
        }

        processing_complete.store(false, std::memory_order_release);
    }

    __forceinline uint32_t compute_spatial_hash(float x, float z) const {
        uint32_t grid_x = static_cast<uint32_t>((x + 32768.0f) / GRID_SIZE);
        uint32_t grid_z = static_cast<uint32_t>((z + 32768.0f) / GRID_SIZE);
        return (grid_x * 73856093 ^ grid_z * 19349663) & (HASH_BUCKETS - 1);
    }

    void add_entity(const roblox::player& source_player, const Vector3& pos,
        const Vector3& head_pos, float dist_sq, float health) {
        uint32_t current_count = entity_count.load(std::memory_order_acquire);
        if (current_count >= MAX_ENTITIES) return;

        position_x[current_count] = pos.x;
        position_y[current_count] = pos.y;
        position_z[current_count] = pos.z;
        head_x[current_count] = head_pos.x;
        head_y[current_count] = head_pos.y;
        head_z[current_count] = head_pos.z;
        distance_squared[current_count] = dist_sq;
        health_ratio[current_count] = health;

        player_addresses[current_count] = source_player.main.address;
        player_names[current_count] = source_player.name;
        tool_names[current_count] = source_player.toolname;
        player_healths[current_count] = source_player.health;
        player_max_healths[current_count] = source_player.maxhealth;
        player_armors[current_count] = source_player.armor;
        rig_types[current_count] = source_player.rigtype;

        uint32_t flags = 0;
        if (source_player.status == 1) flags |= 0x2;
        if (health > 0.5f) flags |= 0x4;
        if (dist_sq < 250000.0f) flags |= 0x8;
        if (globals::is_teammate(source_player)) flags |= 0x10;
        visibility_flags[current_count] = flags;



        uint32_t hash = compute_spatial_hash(pos.x, pos.z);
        spatial_hashes[current_count] = static_cast<uint16_t>(hash);
        spatial_grid[hash].push_back(current_count);

        
        entity_count.store(current_count + 1, std::memory_order_release);
    }

    void frustum_cull_and_compact(const Vector3& camera_pos, float max_distance_sq, float min_health) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        const __m256 cam_x = _mm256_set1_ps(camera_pos.x);
        const __m256 cam_y = _mm256_set1_ps(camera_pos.y);
        const __m256 cam_z = _mm256_set1_ps(camera_pos.z);
        const __m256 max_dist = _mm256_set1_ps(max_distance_sq);
        const __m256 min_health_vec = _mm256_set1_ps(min_health);

        uint32_t write_index = 0;

        uint32_t simd_limit = count - (count % 8);
        for (uint32_t i = 0; i < simd_limit; i += 8) {
            __m256 pos_x = _mm256_loadu_ps(&position_x[i]);
            __m256 pos_y = _mm256_loadu_ps(&position_y[i]);
            __m256 pos_z = _mm256_loadu_ps(&position_z[i]);
            __m256 health = _mm256_loadu_ps(&health_ratio[i]);

            __m256 dx = _mm256_sub_ps(pos_x, cam_x);
            __m256 dy = _mm256_sub_ps(pos_y, cam_y);
            __m256 dz = _mm256_sub_ps(pos_z, cam_z);

            __m256 dist_sq = _mm256_fmadd_ps(dx, dx, _mm256_fmadd_ps(dy, dy, _mm256_mul_ps(dz, dz)));

            __m256 distance_mask = _mm256_cmp_ps(dist_sq, max_dist, _CMP_LT_OQ);
            __m256 health_mask = _mm256_cmp_ps(health, min_health_vec, _CMP_GE_OQ);
            __m256 final_mask = _mm256_and_ps(distance_mask, health_mask);

            int mask_bits = _mm256_movemask_ps(final_mask);

            for (int j = 0; j < 8; ++j) {
                if (mask_bits & (1 << j)) {
                    uint32_t src_idx = i + j;
                    if (write_index != src_idx) {
                        position_x[write_index] = position_x[src_idx];
                        position_y[write_index] = position_y[src_idx];
                        position_z[write_index] = position_z[src_idx];
                        head_x[write_index] = head_x[src_idx];
                        head_y[write_index] = head_y[src_idx];
                        head_z[write_index] = head_z[src_idx];
                        distance_squared[write_index] = distance_squared[src_idx];
                        health_ratio[write_index] = health_ratio[src_idx];
                        visibility_flags[write_index] = visibility_flags[src_idx];

                        player_addresses[write_index] = player_addresses[src_idx];
                        player_names[write_index] = std::move(player_names[src_idx]);
                        tool_names[write_index] = std::move(tool_names[src_idx]);
                        player_healths[write_index] = player_healths[src_idx];
                        player_max_healths[write_index] = player_max_healths[src_idx];
                        player_armors[write_index] = player_armors[src_idx];
                        rig_types[write_index] = rig_types[src_idx];
                        spatial_hashes[write_index] = spatial_hashes[src_idx];
                    }
                    write_index++;
                }
            }
        }

        for (uint32_t i = simd_limit; i < count; ++i) {
            float dx = position_x[i] - camera_pos.x;
            float dy = position_y[i] - camera_pos.y;
            float dz = position_z[i] - camera_pos.z;
            float dist_sq = dx * dx + dy * dy + dz * dz;

            if (dist_sq < max_distance_sq && health_ratio[i] >= min_health) {
                if (write_index != i) {
                    position_x[write_index] = position_x[i];
                    position_y[write_index] = position_y[i];
                    position_z[write_index] = position_z[i];
                    head_x[write_index] = head_x[i];
                    head_y[write_index] = head_y[i];
                    head_z[write_index] = head_z[i];
                    distance_squared[write_index] = distance_squared[i];
                    health_ratio[write_index] = health_ratio[i];
                    visibility_flags[write_index] = visibility_flags[i];

                    player_addresses[write_index] = player_addresses[i];
                    player_names[write_index] = std::move(player_names[i]);
                    tool_names[write_index] = std::move(tool_names[i]);
                    player_healths[write_index] = player_healths[i];
                    player_max_healths[write_index] = player_max_healths[i];
                    player_armors[write_index] = player_armors[i];
                    rig_types[write_index] = rig_types[i];
                    spatial_hashes[write_index] = spatial_hashes[i];
                }
                write_index++;
            }
        }

        entity_count.store(write_index, std::memory_order_release);
    }

    void sort_by_distance() {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count <= 1) return;

        thread_local static std::vector<uint16_t> sort_indices;
        sort_indices.clear();
        sort_indices.reserve(count);

        for (uint16_t i = 0; i < count; ++i) {
            sort_indices.push_back(i);
        }

        std::sort(sort_indices.begin(), sort_indices.end(),
            [this](uint16_t a, uint16_t b) {
                return distance_squared[a] < distance_squared[b];
            });

        thread_local static std::array<float, MAX_ENTITIES> temp_pos_x, temp_pos_y, temp_pos_z;
        thread_local static std::array<float, MAX_ENTITIES> temp_head_x, temp_head_y, temp_head_z;
        thread_local static std::array<float, MAX_ENTITIES> temp_distance, temp_health;
        thread_local static std::array<uint32_t, MAX_ENTITIES> temp_flags;
        thread_local static std::array<uintptr_t, MAX_ENTITIES> temp_addresses;
        thread_local static std::array<std::string, MAX_ENTITIES> temp_names, temp_tools;
        thread_local static std::array<int, MAX_ENTITIES> temp_healths, temp_max_healths, temp_armors, temp_rigs;
        thread_local static std::array<uint16_t, MAX_ENTITIES> temp_hashes;

        for (uint16_t i = 0; i < count; ++i) {
            uint16_t src_idx = sort_indices[i];

            temp_pos_x[i] = position_x[src_idx];
            temp_pos_y[i] = position_y[src_idx];
            temp_pos_z[i] = position_z[src_idx];
            temp_head_x[i] = head_x[src_idx];
            temp_head_y[i] = head_y[src_idx];
            temp_head_z[i] = head_z[src_idx];
            temp_distance[i] = distance_squared[src_idx];
            temp_health[i] = health_ratio[src_idx];
            temp_flags[i] = visibility_flags[src_idx];

            temp_addresses[i] = player_addresses[src_idx];
            temp_names[i] = player_names[src_idx];
            temp_tools[i] = tool_names[src_idx];
            temp_healths[i] = player_healths[src_idx];
            temp_max_healths[i] = player_max_healths[src_idx];
            temp_armors[i] = player_armors[src_idx];
            temp_rigs[i] = rig_types[src_idx];
            temp_hashes[i] = spatial_hashes[src_idx];
        }

        memcpy(position_x.data(), temp_pos_x.data(), count * sizeof(float));
        memcpy(position_y.data(), temp_pos_y.data(), count * sizeof(float));
        memcpy(position_z.data(), temp_pos_z.data(), count * sizeof(float));
        memcpy(head_x.data(), temp_head_x.data(), count * sizeof(float));
        memcpy(head_y.data(), temp_head_y.data(), count * sizeof(float));
        memcpy(head_z.data(), temp_head_z.data(), count * sizeof(float));
        memcpy(distance_squared.data(), temp_distance.data(), count * sizeof(float));
        memcpy(health_ratio.data(), temp_health.data(), count * sizeof(float));
        memcpy(visibility_flags.data(), temp_flags.data(), count * sizeof(uint32_t));
        memcpy(player_addresses.data(), temp_addresses.data(), count * sizeof(uintptr_t));
        memcpy(player_healths.data(), temp_healths.data(), count * sizeof(int));
        memcpy(player_max_healths.data(), temp_max_healths.data(), count * sizeof(int));
        memcpy(player_armors.data(), temp_armors.data(), count * sizeof(int));
        memcpy(rig_types.data(), temp_rigs.data(), count * sizeof(int));
        memcpy(spatial_hashes.data(), temp_hashes.data(), count * sizeof(uint16_t));

        for (uint32_t i = 0; i < count; ++i) {
            player_names[i] = std::move(temp_names[i]);
            tool_names[i] = std::move(temp_tools[i]);
        }
    }

    void batch_worldtoscreen(const Vector2& screen_dims) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        for (uint32_t i = 0; i < count; ++i) {
            Vector3 world_pos = { position_x[i], position_y[i], position_z[i] };
            Vector3 head_world_pos = { head_x[i], head_y[i], head_z[i] };

            Vector2 screen_pos = roblox::worldtoscreen(world_pos);
            Vector2 head_screen_pos = roblox::worldtoscreen(head_world_pos);

            screen_x[i] = screen_pos.x;
            screen_y[i] = screen_pos.y;

            if (screen_pos.x > 0 && screen_pos.y > 0 &&
                screen_pos.x < screen_dims.x && screen_pos.y < screen_dims.y) {
                visibility_flags[i] |= 0x1;
            }
            else {
                visibility_flags[i] &= ~0x1;
            }

            bounds_min_x[i] = FLT_MAX;
            bounds_min_y[i] = FLT_MAX;
            bounds_max_x[i] = -FLT_MAX;
            bounds_max_y[i] = -FLT_MAX;
            box_margins[i] = 0.0f;
        }
    }

    void calculate_proper_bounds(const Vector2& screen_dims) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        std::shared_lock<std::shared_mutex> lock(cache::player_data_mutex);
        const auto& cached_players = globals::cachedplayers;

        for (uint32_t i = 0; i < count; ++i) {
            uintptr_t target_address = player_addresses[i];
            const roblox::player* source_player = nullptr;

            for (const auto& player : cached_players) {
                if (player.main.address == target_address) {
                    source_player = &player;
                    break;
                }
            }

            if (!source_player) continue;

            bool has_limbs = (is_valid_address(source_player->lefthand.address) || is_valid_address(source_player->leftupperarm.address)) &&
                (is_valid_address(source_player->righthand.address) || is_valid_address(source_player->rightupperarm.address)) &&
                (is_valid_address(source_player->leftfoot.address) || is_valid_address(source_player->leftupperleg.address)) &&
                (is_valid_address(source_player->rightfoot.address) || is_valid_address(source_player->rightupperleg.address));

            if (!has_limbs) {
                continue;
            }

            thread_local static std::array<Vector3, 6> world_points;
            thread_local static std::array<Vector2, 6> screen_points;

            world_points[0] = { head_x[i], head_y[i], head_z[i] };

            if (source_player->rigtype == 1) {
                world_points[1] = source_player->torso.address ? source_player->torso.get_pos() : Vector3::zero();
            }
            else {
                world_points[1] = source_player->uppertorso.address ? source_player->uppertorso.get_pos() : Vector3::zero();
            }

            world_points[2] = source_player->lefthand.address ? source_player->lefthand.get_pos() :
                (source_player->leftupperarm.address ? source_player->leftupperarm.get_pos() : Vector3::zero());
            world_points[3] = source_player->righthand.address ? source_player->righthand.get_pos() :
                (source_player->rightupperarm.address ? source_player->rightupperarm.get_pos() : Vector3::zero());
            world_points[4] = source_player->leftfoot.address ? source_player->leftfoot.get_pos() :
                (source_player->leftupperleg.address ? source_player->leftupperleg.get_pos() : Vector3::zero());
            world_points[5] = source_player->rightfoot.address ? source_player->rightfoot.get_pos() :
                (source_player->rightupperleg.address ? source_player->rightupperleg.get_pos() : Vector3::zero());

            float min_x = FLT_MAX, min_y = FLT_MAX;
            float max_x = -FLT_MAX, max_y = -FLT_MAX;
            bool any_on_screen = false;

            for (size_t j = 0; j < 6; ++j) {
                screen_points[j] = roblox::worldtoscreen(world_points[j]);

                if (optimization_helpers::is_on_screen(screen_points[j], screen_dims)) {
                    any_on_screen = true;
                    min_x = (std::min)(min_x, screen_points[j].x);
                    min_y = (std::min)(min_y, screen_points[j].y);
                    max_x = (std::max)(max_x, screen_points[j].x);
                    max_y = (std::max)(max_y, screen_points[j].y);
                }
            }

            if (!any_on_screen) {
                for (size_t j = 0; j < 6; ++j) {
                    min_x = (std::min)(min_x, screen_points[j].x);
                    min_y = (std::min)(min_y, screen_points[j].y);
                    max_x = (std::max)(max_x, screen_points[j].x);
                    max_y = (std::max)(max_y, screen_points[j].y);
                }
            }

            float margin_calc = std::abs(screen_points[0].y - screen_points[1].y) * 0.45f;

            bounds_min_x[i] = min_x - margin_calc;
            bounds_min_y[i] = min_y - margin_calc;
            bounds_max_x[i] = max_x + margin_calc;
            bounds_max_y[i] = max_y + margin_calc;
            box_margins[i] = margin_calc;
        }
    }

    void cull_offscreen_entities(const Vector2& screen_dims) {
        uint32_t count = entity_count.load(std::memory_order_acquire);
        if (count == 0) return;

        uint32_t write_index = 0;

        for (uint32_t i = 0; i < count; ++i) {
            bool on_screen = (bounds_min_x[i] < screen_dims.x && bounds_max_x[i] > 0 &&
                bounds_min_y[i] < screen_dims.y && bounds_max_y[i] > 0);

            if (on_screen) {
                if (write_index != i) {
                    position_x[write_index] = position_x[i];
                    position_y[write_index] = position_y[i];
                    position_z[write_index] = position_z[i];
                    head_x[write_index] = head_x[i];
                    head_y[write_index] = head_y[i];
                    head_z[write_index] = head_z[i];
                    distance_squared[write_index] = distance_squared[i];
                    health_ratio[write_index] = health_ratio[i];
                    visibility_flags[write_index] = visibility_flags[i];
                    screen_x[write_index] = screen_x[i];
                    screen_y[write_index] = screen_y[i];
                    bounds_min_x[write_index] = bounds_min_x[i];
                    bounds_min_y[write_index] = bounds_min_y[i];
                    bounds_max_x[write_index] = bounds_max_x[i];
                    bounds_max_y[write_index] = bounds_max_y[i];
                    box_margins[write_index] = box_margins[i];

                    player_addresses[write_index] = player_addresses[i];
                    player_names[write_index] = std::move(player_names[i]);
                    tool_names[write_index] = std::move(tool_names[i]);
                    player_healths[write_index] = player_healths[i];
                    player_max_healths[write_index] = player_max_healths[i];
                    player_armors[write_index] = player_armors[i];
                    rig_types[write_index] = rig_types[i];
                    spatial_hashes[write_index] = spatial_hashes[i];
                }
                write_index++;
            }
        }

        entity_count.store(write_index, std::memory_order_release);
        processing_complete.store(true, std::memory_order_release);
    }
};

namespace health_utils {
    inline bool safe_read_health(const instance& humanoid, int& health, int& maxhealth) {
        if (!humanoid.is_valid() || !is_valid_address(humanoid.address)) {
            return false;
        }

        try {
            float health_val = humanoid.read_health();
            float maxhealth_val = humanoid.read_maxhealth();

            if (health_val > 0.0f &&
                maxhealth_val >= 100.0f && maxhealth_val <= 10000.0f &&
                health_val <= maxhealth_val)
            {
                health = static_cast<int>(health_val);
                maxhealth = static_cast<int>(maxhealth_val);
                return true;
            }

            if (health_val == 0.0f &&
                maxhealth_val >= 100.0f && maxhealth_val <= 10000.0f)
            {
                health = 0;
                maxhealth = static_cast<int>(maxhealth_val);
                return true;
            }
        }
        catch (...) {}

        return false;
    }

    inline bool safe_read_arsenal_health(const instance& player_instance, int& health, int& maxhealth) {
        try {
            auto nrpbs = player_instance.findfirstchild("NRPBS");
            if (nrpbs.is_valid()) {
                auto health_obj = nrpbs.findfirstchild("Health");
                auto maxhealth_obj = nrpbs.findfirstchild("MaxHealth");

                if (health_obj.is_valid() && maxhealth_obj.is_valid()) {
                    double health_val = health_obj.read_double_value();
                    double maxhealth_val = maxhealth_obj.read_double_value();

                    if (health_val >= 0.0 && health_val <= 10000.0 &&
                        maxhealth_val > 0.0 && maxhealth_val <= 10000.0) {
                        health = static_cast<int>(health_val);
                        maxhealth = static_cast<int>(maxhealth_val);
                        return true;
                    }
                }
            }
        }
        catch (...) {
        }

        health = 0;
        maxhealth = 100;
        return false;
    }
}

namespace player_access {
    std::vector<player> get_cached_players() {
        std::shared_lock<std::shared_mutex> lock(cache::player_data_mutex);
        return globals::cachedplayers;
    }

    void set_cached_players(std::vector<player>&& players) {
        std::unique_lock<std::shared_mutex> lock(cache::player_data_mutex);
        globals::cachedplayers = std::move(players);
    }

    std::vector<ESPPlayerData> get_cached_esp_players() {
        std::shared_lock<std::shared_mutex> lock(cache::esp_data_mutex);
        return globals::cached_esp_players;
    }

    void set_cached_esp_players(std::vector<ESPPlayerData>&& esp_players) {
        std::unique_lock<std::shared_mutex> lock(cache::esp_data_mutex);
        globals::cached_esp_players = std::move(esp_players);
    }

    Vector3 get_cached_camera_position() {
        std::unique_lock<std::mutex> lock(cache::update_mutex);
        return globals::cached_camera_position;
    }
}


namespace team_utils {
    inline roblox::instance read_player_team(const roblox::instance& player_instance) {
        try {
            if (!player_instance.is_valid()) return {};

            const std::uintptr_t team_address = read<std::uintptr_t>(player_instance.address + offsets::Team);
            if (team_address < 0x10000 || team_address > 0x7FFFFFFFFFFF) return {};

            roblox::instance team(team_address);
            if (team.get_class_name() != "Team") return {};

            return team;
        }
        catch (...) {
            return {};
        }
    }
}

void cache::hook_cache(bool firstrecieved) {
    static player_soa players_data;
    static std::vector<player> final_players;
    final_players.reserve(4096);

    static const std::vector<std::string> bone_names = {
        "Head", "HumanoidRootPart", "Torso", "UpperTorso", "LowerTorso",
        "LeftHand", "RightHand", "LeftLowerArm", "RightLowerArm",
        "LeftUpperArm", "RightUpperArm", "LeftFoot", "RightFoot",
        "LeftLowerLeg", "RightLowerLeg", "LeftUpperLeg", "RightUpperLeg",
        "Left Arm", "Right Arm", "Left Leg", "Right Leg"
    };

    static const std::vector<std::string> bad_business_bone_names = {
        "LeftHand", "Abdomen", "RightLeg", "Hips", "RightFoot", "LeftArm",
        "LeftForearm", "RightForearm", "RightForeleg", "RightHand", "Chest",
        "RightArm", "Neck", "Head", "LeftForeleg", "LeftLeg", "LeftFoot"
    };

    static const std::vector<std::string> phantom_forces_bone_names = {
        "Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"
    };

    static const std::vector<std::string> military_locations = {
        "ConvoyFour", "ConvoyThree", "ConvoyTwo", "ConvoyOne", "Cargo Yard",
        "Labs", "Rocket Factory", "Military Airfield", "Industrial Port",
        "Military Base", "Military Barracks"
    };

    while (firstrecieved) {
        auto now = std::chrono::steady_clock::now();
        static auto last_full_update = std::chrono::steady_clock::now();
        auto time_since_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_full_update).count();

        if (time_since_update < 75) {
            std::this_thread::sleep_for(std::chrono::milliseconds(75));
            continue;
        }

        cache::data_updating.store(true, std::memory_order_release);

        players_data.count = 0;

        static bool previous_soldier_esp_state = false;
        if (previous_soldier_esp_state && !features::visuals::soldier_esp_enabled) {
            final_players.clear();
        }
        previous_soldier_esp_state = features::visuals::soldier_esp_enabled;

        if (!is_valid_address(globals::players.address)) {
            cache::data_updating.store(false, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            continue;
        }

        auto all_players = (globals::gamename == "Bad Business") ?
            globals::workspace.findfirstchild("Characters").get_children() :
            globals::players.get_children();

        if (globals::gamename == "PhantomForces") {
            std::vector<roblox::instance> playerlist1;
            for (roblox::instance folders : globals::datamodel.read_service("Workspace").findfirstchild("Players").get_children()) {
                playerlist1 = folders.get_children();
                for (roblox::instance fold : playerlist1) {
                    all_players.push_back(fold);
                }
            }
        }

        if (all_players.empty()) {
            cache::data_updating.store(false, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            continue;
        }
        const size_t player_count = (std::min)(all_players.size(), static_cast<size_t>(4096));

        thread_local static std::unordered_map<std::string, instance> children_cache;

        for (size_t i = 0; i < player_count; ++i) {
            auto& player_instance = all_players[i];

            if (!is_valid_address(player_instance.address)) {
                continue;
            }

            auto character = player_instance;
            if (globals::gamename == "Bad Business") {

            }
            else if (globals::gamename == "PhantomForces") {
                auto phantom_children = player_instance.get_children();
                bool found_head = false;
                for (auto& child : phantom_children) {
                    auto child_children = child.get_children();
                    for (auto& grandchild : child_children) {
                        if (grandchild.get_name() == "NameTagGui") {
                            character = child;
                            found_head = true;
                            break;
                        }
                    }
                    if (found_head) break;
                }
                if (!found_head) continue;
            }
            else {
                if (player_instance.get_class_name() != "Player") {
                    continue;
                }
                character = player_instance.model_instance();

                if (!is_valid_address(character.address)) {
                    continue;
                }
                if (character.get_name() != player_instance.get_name()) continue;
            }

            children_cache.clear();
            children_cache.reserve(25);

            if (globals::gamename == "Bad Business") {
                auto body = character.findfirstchild("Body");
                if (!body.is_valid()) {
                    continue;
                }

                auto root = character.findfirstchild("Root");
                if (!root.is_valid()) {
                    continue;
                }

                auto body_children = body.get_children();

                for (auto& body_child : body_children) {
                    if (body_child.is_valid()) {
                        try {
                            std::string bone_name = body_child.get_name();
                            if (!bone_name.empty() && bone_name.length() < 50) {
                                children_cache.emplace(bone_name, std::move(body_child));
                            }
                        }
                        catch (...) {
                            continue;
                        }
                    }
                }
            }
            else if (globals::gamename == "PhantomForces") {
                auto phantom_parent = character.read_parent();
                if (phantom_parent.is_valid()) {
                    auto phantom_siblings = phantom_parent.get_children();
                    for (auto& sibling : phantom_siblings) {
                        if (sibling.address != character.address) {
                            auto sibling_children = sibling.get_children();
                            bool has_spotlight = false;
                            for (auto& child : sibling_children) {
                                if (child.get_class_name() == "SpotLight") {
                                    has_spotlight = true;
                                    break;
                                }
                            }
                            if (has_spotlight) {
                                for (auto& child : sibling_children) {
                                    if (child.get_class_name() != "SpotLight") {
                                        children_cache.emplace(child.get_name(), std::move(child));
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else {
                auto children = character.get_children();
                for (auto& child : children) {
                    if (child.is_valid()) {
                        children_cache.emplace(child.get_name(), std::move(child));
                    }
                }
            }

            const size_t idx = players_data.count;
            players_data.addresses[idx] = player_instance.address;
            players_data.mains[idx] = player_instance;
            players_data.teams[idx] = team_utils::read_player_team(player_instance);
            if (globals::gamename == "Bad Business") {
                players_data.names[idx] = character.get_name();
                players_data.displaynames[idx] = character.get_name();
            }
            else if (globals::gamename == "PhantomForces") {
                players_data.names[idx] = "Unknown";
                players_data.displaynames[idx] = "Unknown";
            }
            else {
                players_data.names[idx] = player_instance.get_name();
                players_data.displaynames[idx] = player_instance.get_humdisplayname();
            }

            if (!character.address) continue;

            if (globals::gamename == "Bad Business") {
                auto root = character.findfirstchild("Root");
                if (!root.is_valid()) {
                    continue;
                }

                auto head_it = children_cache.find("Head");
                if (head_it != children_cache.end()) {
                    players_data.heads[idx] = head_it->second;
                }

                players_data.hrps[idx] = root;

                auto humanoid_it = character.findfirstchild("Humanoid");
                if (humanoid_it.is_valid()) {
                    players_data.humanoids[idx] = humanoid_it;
                }
                else {
                    players_data.humanoids[idx] = character;
                }

                players_data.rigtypes[idx] = 2;

                auto lefthand_it = children_cache.find("LeftHand");
                if (lefthand_it != children_cache.end()) {
                    players_data.lefthand[idx] = lefthand_it->second;
                }

                auto righthand_it = children_cache.find("RightHand");
                if (righthand_it != children_cache.end()) {
                    players_data.righthand[idx] = righthand_it->second;
                }

                auto leftforearm_it = children_cache.find("LeftForearm");
                if (leftforearm_it != children_cache.end()) players_data.leftlowerarm[idx] = leftforearm_it->second;

                auto rightforearm_it = children_cache.find("RightForearm");
                if (rightforearm_it != children_cache.end()) players_data.rightlowerarm[idx] = rightforearm_it->second;

                auto leftarm_it = children_cache.find("LeftArm");
                if (leftarm_it != children_cache.end()) {
                    players_data.leftupperarm[idx] = leftarm_it->second;
                }

                auto rightarm_it = children_cache.find("RightArm");
                if (rightarm_it != children_cache.end()) {
                    players_data.rightupperarm[idx] = rightarm_it->second;
                }

                auto leftfoot_it = children_cache.find("LeftFoot");
                if (leftfoot_it != children_cache.end()) {
                    players_data.leftfoot[idx] = leftfoot_it->second;
                }

                auto rightfoot_it = children_cache.find("RightFoot");
                if (rightfoot_it != children_cache.end()) {
                    players_data.rightfoot[idx] = rightfoot_it->second;
                }

                auto leftforeleg_it = children_cache.find("LeftForeleg");
                if (leftforeleg_it != children_cache.end()) players_data.leftlowerleg[idx] = leftforeleg_it->second;

                auto rightforeleg_it = children_cache.find("RightForeleg");
                if (rightforeleg_it != children_cache.end()) players_data.rightlowerleg[idx] = rightforeleg_it->second;

                auto leftleg_it = children_cache.find("LeftLeg");
                if (leftleg_it != children_cache.end()) {
                    players_data.leftupperleg[idx] = leftleg_it->second;
                }

                auto rightleg_it = children_cache.find("RightLeg");
                if (rightleg_it != children_cache.end()) {
                    players_data.rightupperleg[idx] = rightleg_it->second;
                }

                auto chest_it = children_cache.find("Chest");
                if (chest_it != children_cache.end()) players_data.uppertorsos[idx] = chest_it->second;

                auto abdomen_it = children_cache.find("Abdomen");
                if (abdomen_it != children_cache.end()) players_data.lowertorsos[idx] = abdomen_it->second;

                auto hips_it = children_cache.find("Hips");
                if (hips_it != children_cache.end()) players_data.torsos[idx] = hips_it->second;
            }
            else if (globals::gamename == "PhantomForces") {
                auto phantom_parent = character.read_parent();
                if (phantom_parent.is_valid()) {
                    auto all_parts = phantom_parent.get_children();

                    roblox::instance head_part;
                    roblox::instance hrp_torso;
                    std::vector<roblox::instance> other_parts;
                    std::string decrypted_name;

                    for (auto& part : all_parts) {
                        auto part_children = part.get_children();
                        bool is_head = false;
                        bool is_hrp_torso = false;

                        for (auto& child : part_children) {
                            if (child.get_name() == "NameTagGui") {
                                head_part = part;
                                is_head = true;

                                try {
                                    auto frame = read<roblox::instance>(child.address + 0x28);
                                    decrypted_name = frame.read_parent().read_parent().read_parent().get_name();
                                }
                                catch (...) {
                                    decrypted_name = "Unknown Player";
                                }
                                break;
                            }
                            else if (child.get_class_name() == "SpotLight") {
                                hrp_torso = part;
                                is_hrp_torso = true;
                                break;
                            }
                        }

                        if (!is_head && !is_hrp_torso) {
                            other_parts.push_back(part);
                        }
                    }

                    if (head_part.is_valid() && hrp_torso.is_valid() && other_parts.size() >= 4) {
                        players_data.heads[idx] = head_part;
                        players_data.hrps[idx] = hrp_torso;
                        players_data.torsos[idx] = hrp_torso;

                        Vector3 hrp_pos = hrp_torso.get_pos();
                        std::vector<std::pair<float, roblox::instance>> part_distances;

                        for (auto& part : other_parts) {
                            Vector3 part_pos = part.get_pos();
                            float dx = hrp_pos.x - part_pos.x;
                            float dy = hrp_pos.y - part_pos.y;
                            float dz = hrp_pos.z - part_pos.z;
                            float distance = dx * dx + dy * dy + dz * dz;
                            part_distances.push_back(std::make_pair(distance, part));
                        }

                        std::sort(part_distances.begin(), part_distances.end(), [](const std::pair<float, roblox::instance>& a, const std::pair<float, roblox::instance>& b) {
                            return a.first < b.first;
                            });

                        players_data.lefthand[idx] = part_distances[0].second;
                        players_data.righthand[idx] = part_distances[1].second;

                        players_data.leftfoot[idx] = part_distances[2].second;
                        players_data.rightfoot[idx] = part_distances[3].second;

                        if (!decrypted_name.empty()) {
                            players_data.names[idx] = decrypted_name;
                            players_data.displaynames[idx] = decrypted_name;
                        }
                    }
                    else {
                    }
                }

                players_data.humanoids[idx] = character;
                players_data.rigtypes[idx] = 1;
            }
            else {
                auto hrp_it = children_cache.find("HumanoidRootPart");
                if (hrp_it == children_cache.end() || !hrp_it->second.is_valid()) {
                    continue;
                }

                auto uppertorso_it = children_cache.find("UpperTorso");
                bool is_r15 = (uppertorso_it != children_cache.end() && uppertorso_it->second.is_valid());

                if (is_r15) {
                    auto rightupperleg_it = children_cache.find("RightUpperLeg");
                    auto rightupperarm_it = children_cache.find("RightUpperArm");

                    if (rightupperleg_it == children_cache.end() || !rightupperleg_it->second.is_valid() ||
                        rightupperarm_it == children_cache.end() || !rightupperarm_it->second.is_valid()) {
                        continue;
                    }
                }

                auto head_it = children_cache.find("Head");
                if (head_it != children_cache.end()) players_data.heads[idx] = head_it->second;

                players_data.hrps[idx] = hrp_it->second;

                auto humanoid_it = children_cache.find("Humanoid");
                if (humanoid_it != children_cache.end()) players_data.humanoids[idx] = humanoid_it->second;

                auto torso_it = children_cache.find("Torso");
                if (torso_it != children_cache.end()) players_data.torsos[idx] = torso_it->second;

                if (is_r15) {
                    players_data.rigtypes[idx] = 2;
                    players_data.uppertorsos[idx] = uppertorso_it->second;

                    auto lowertorso_it = children_cache.find("LowerTorso");
                    if (lowertorso_it != children_cache.end()) players_data.lowertorsos[idx] = lowertorso_it->second;

                    auto lefthand_it = children_cache.find("LeftHand");
                    if (lefthand_it != children_cache.end()) players_data.lefthand[idx] = lefthand_it->second;

                    auto righthand_it = children_cache.find("RightHand");
                    if (righthand_it != children_cache.end()) players_data.righthand[idx] = righthand_it->second;

                    auto leftlowerarm_it = children_cache.find("LeftLowerArm");
                    if (leftlowerarm_it != children_cache.end()) players_data.leftlowerarm[idx] = leftlowerarm_it->second;

                    auto rightlowerarm_it = children_cache.find("RightLowerArm");
                    if (rightlowerarm_it != children_cache.end()) players_data.rightlowerarm[idx] = rightlowerarm_it->second;

                    auto leftupperarm_it = children_cache.find("LeftUpperArm");
                    if (leftupperarm_it != children_cache.end()) players_data.leftupperarm[idx] = leftupperarm_it->second;

                    auto rightupperarm_it = children_cache.find("RightUpperArm");
                    if (rightupperarm_it != children_cache.end()) players_data.rightupperarm[idx] = rightupperarm_it->second;

                    auto leftfoot_it = children_cache.find("LeftFoot");
                    if (leftfoot_it != children_cache.end()) players_data.leftfoot[idx] = leftfoot_it->second;

                    auto leftlowerleg_it = children_cache.find("LeftLowerLeg");
                    if (leftlowerleg_it != children_cache.end()) players_data.leftlowerleg[idx] = leftlowerleg_it->second;

                    auto leftupperleg_it = children_cache.find("LeftUpperLeg");
                    if (leftupperleg_it != children_cache.end()) players_data.leftupperleg[idx] = leftupperleg_it->second;

                    auto rightlowerleg_it = children_cache.find("RightLowerLeg");
                    if (rightlowerleg_it != children_cache.end()) players_data.rightlowerleg[idx] = rightlowerleg_it->second;

                    auto rightfoot_it = children_cache.find("RightFoot");
                    if (rightfoot_it != children_cache.end()) players_data.rightfoot[idx] = rightfoot_it->second;

                    auto rightupperleg_it = children_cache.find("RightUpperLeg");
                    if (rightupperleg_it != children_cache.end()) players_data.rightupperleg[idx] = rightupperleg_it->second;
                }
                else {
                    players_data.rigtypes[idx] = 1;

                    auto leftarm_it = children_cache.find("Left Arm");
                    if (leftarm_it != children_cache.end()) players_data.lefthand[idx] = leftarm_it->second;

                    auto rightarm_it = children_cache.find("Right Arm");
                    if (rightarm_it != children_cache.end()) players_data.righthand[idx] = rightarm_it->second;

                    auto leftleg_it = children_cache.find("Left Leg");
                    if (leftleg_it != children_cache.end()) players_data.leftfoot[idx] = leftleg_it->second;

                    auto rightleg_it = children_cache.find("Right Leg");
                    if (rightleg_it != children_cache.end()) players_data.rightfoot[idx] = rightleg_it->second;
                }
            }

            if (globals::gamename != "Arsenal") {
                if (globals::gamename == "Bad Business") {
                    auto health_obj = character.findfirstchild("Health");
                    if (health_obj.is_valid()) {
                        try {
                            players_data.healths[idx] = static_cast<int>(health_obj.read_double_value());
                            players_data.maxhealths[idx] = 100;
                        }
                        catch (...) {
                            players_data.healths[idx] = 100;
                            players_data.maxhealths[idx] = 100;
                        }
                    }
                    else {
                        players_data.healths[idx] = 100;
                        players_data.maxhealths[idx] = 100;
                    }
                }
                else if (globals::gamename == "PhantomForces") {
                    players_data.healths[idx] = 100;
                    players_data.maxhealths[idx] = 100;
                }
                else {
                    int i = 0;
                    while (!health_utils::safe_read_health(players_data.humanoids[idx],
                        players_data.healths[idx],
                        players_data.maxhealths[idx])) {

                        i++;
                        if (i == 10) {
                            break;
                        }
                    }
                }
            }
            else {
                health_utils::safe_read_arsenal_health(player_instance,
                    players_data.healths[idx],
                    players_data.maxhealths[idx]);
            }

            auto bodyeffects = character.findfirstchild("BodyEffects");
            if (bodyeffects.is_valid()) {
                players_data.bodyeffects[idx] = bodyeffects;
                auto armor_obj = bodyeffects.findfirstchild("Armor");
                if (armor_obj.is_valid()) {
                    try {
                        double armor_val = armor_obj.read_double_value();
                        players_data.armors[idx] = (armor_val >= 0.0 && armor_val <= 10000.0) ?
                            static_cast<int>(armor_val) : 0;
                    }
                    catch (...) {
                        players_data.armors[idx] = 0;
                    }
                }
                else {
                    players_data.armors[idx] = 0;
                }
            }
            else {
                players_data.armors[idx] = 0;
            }

            if (players_data.displaynames[idx].empty() && players_data.humanoids[idx].is_valid()) {
                try {
                    players_data.displaynames[idx] = players_data.humanoids[idx].get_humdisplayname();
                }
                catch (...) {
                    players_data.displaynames[idx] = players_data.names[idx];
                }
            }
            const std::vector<std::string> weaponnames = {
                "Boulder",
                "Bow",
                "Wooden Spear",
                "Stone Spear",
                "Machete",
                "Saw Bat",
                "Nail gun",
                "Military AA12",
                "Salvaged Break Action",
                "Crossbow",
                "P250",
                "Salvaged Pump Action",
                "Salvaged Pipe Rifle",
                "Salvaged Scorpion",
                "Salvaged Python",
                "Salvaged M14",
                "Military M4A1",
                "Military Barret",
                "AK74U",
                "Swift Rocket",
                "Combustive Rocket",
                "Wooden Bow",
                "Salvaged P250",
                "Military USP",
                "Salvaged SMG",
                "Salvaged AK74u",
                "Salvaged AK47",
                "Military PKM",
                "Salvaged RPG",
                "Regular Rocket",
                "Dynamite Stick",
                "Dynamite Bundle",
                "Timed Charge",
                "Grenades",
                "Stone Hatchet",
                "Iron Shard Hatchet",
                "Steel Axe",
                "Stone Pickaxe",
                "Iron Shard Pickaxe",
                "Steel Pickaxe",
                "Salvaged Shovel",
                "Steel Shovel",
                "Blueprint",
                "Hammer",
                "Lighter",
                "Bone Tool",
                "Drill",
                "Chainsaw",
                "Pumpkin Launcher",
                "Cursed Pumpkins"
            };

            auto tool = character.read_service("Tool");
            if (tool.is_valid()) {
                players_data.tools[idx] = tool;
                try {
                    players_data.toolnames[idx] = tool.get_name();
                }
                catch (...) {
                    players_data.toolnames[idx] = "[No Tool]";
                }
            }
            else {
                if (globals::gamename == "Bad Business") {
                    auto character_children = character.get_children();
                    bool weapon_found = false;
                    for (auto& child : character_children) {
                        if (child.is_valid() && child.get_class_name() == "Tool") {
                            players_data.toolnames[idx] = child.get_name();
                            weapon_found = true;
                            break;
                        }
                    }
                    if (!weapon_found) {
                        players_data.toolnames[idx] = "[No Tool]";
                    }
                }
                else if (globals::gamename == "PhantomForces") {
                    players_data.toolnames[idx] = "[PF Weapon]";
                }
                else if (globals::gamename == "Fallen") {
                    bool weapon_found = false;
                    for (const auto& name : weaponnames) {
                        auto tool_it = children_cache.find(name);
                        if (tool_it != children_cache.end() && tool_it->second.address) {
                            players_data.toolnames[idx] = "[" + name + "]";
                            weapon_found = true;
                            break;
                        }
                    }
                    if (!weapon_found) {
                        players_data.toolnames[idx] = "[No Tool]";
                    }
                }
                else {
                    players_data.toolnames[idx] = "[No Tool]";
                }
            }

            players_data.statuses[idx] = (players_data.healths[idx] > 0 && players_data.hrps[idx].is_valid()) ? 1 : 0;

            if (globals::gamename == "Bad Business") {
                players_data.flags[idx] = "US";

                auto clothes_check = character.findfirstchild("Clothes");
                if (!clothes_check.is_valid()) {
                    players_data.statuses[idx] = 3;
                }
            }
            else if (globals::gamename == "PhantomForces") {
                players_data.flags[idx] = "US";
            }
            else {
                auto country = players_data.mains[idx].findfirstchild("LocaleId");
                if (country.is_valid()) {
                    try {
                        players_data.flags[idx] = country.address;
                    }
                    catch (...) {
                        players_data.flags[idx] = "US";
                    }
                }
                else {
                    players_data.flags[idx] = "US";
                }
            }

            players_data.count++;

            if (players_data.count % 32 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        size_t regular_players_count = players_data.count;

        //    std::cout << regular_players_count;

        if (features::visuals::soldier_esp_enabled && globals::gamename == "Fallen") {
            try {
                auto military_container = globals::workspace.findfirstchild("Military");
                if (military_container.is_valid()) {
                    for (const auto& location_name : military_locations) {
                        auto location = military_container.findfirstchild(location_name);
                        if (!location.is_valid()) continue;

                        auto location_children = location.get_children();
                        for (auto& child : location_children) {
                            if (!child.is_valid()) continue;

                            std::string child_name = child.get_name();
                            if (child_name.find("Soldier") == std::string::npos) continue;

                            auto soldier_hrp = child.findfirstchild("HumanoidRootPart");
                            if (!soldier_hrp.is_valid()) continue;

                            Vector3 soldier_position = soldier_hrp.get_pos();
                            Vector3 camera_position = globals::camera.getPos();
                            float dx = camera_position.x - soldier_position.x;
                            float dy = camera_position.y - soldier_position.y;
                            float dz = camera_position.z - soldier_position.z;
                            float distance_sq = dx * dx + dy * dy + dz * dz;

                            if (distance_sq > 90000.0f) continue;

                            if (players_data.count >= 4096) break;

                            children_cache.clear();
                            children_cache.reserve(25);

                            auto soldier_children = child.get_children();
                            for (auto& soldier_child : soldier_children) {
                                if (soldier_child.is_valid()) {
                                    children_cache.emplace(soldier_child.get_name(), std::move(soldier_child));
                                }
                            }

                            auto soldier_hrp_it = children_cache.find("HumanoidRootPart");
                            if (soldier_hrp_it == children_cache.end() || !soldier_hrp_it->second.is_valid()) {
                                continue;
                            }

                            auto soldier_uppertorso_it = children_cache.find("UpperTorso");
                            bool soldier_is_r15 = (soldier_uppertorso_it != children_cache.end() && soldier_uppertorso_it->second.is_valid());

                            if (soldier_is_r15) {
                                auto soldier_rightupperleg_it = children_cache.find("RightUpperLeg");
                                auto soldier_rightupperarm_it = children_cache.find("RightUpperArm");

                                if (soldier_rightupperleg_it == children_cache.end() || !soldier_rightupperleg_it->second.is_valid() ||
                                    soldier_rightupperarm_it == children_cache.end() || !soldier_rightupperarm_it->second.is_valid()) {
                                    continue;
                                }
                            }

                            const size_t idx = players_data.count;
                            players_data.addresses[idx] = child.address;
                            players_data.mains[idx] = child;
                            players_data.teams[idx] = team_utils::read_player_team(child);
                            players_data.names[idx] = child_name + " (" + location_name + ")";
                            players_data.displaynames[idx] = child_name;

                            auto soldier_head_it = children_cache.find("Head");
                            if (soldier_head_it != children_cache.end()) players_data.heads[idx] = soldier_head_it->second;

                            players_data.hrps[idx] = soldier_hrp_it->second;

                            auto soldier_humanoid_it = children_cache.find("Humanoid");
                            if (soldier_humanoid_it != children_cache.end()) players_data.humanoids[idx] = soldier_humanoid_it->second;

                            auto soldier_torso_it = children_cache.find("Torso");
                            if (soldier_torso_it != children_cache.end()) players_data.torsos[idx] = soldier_torso_it->second;

                            if (soldier_is_r15) {
                                players_data.rigtypes[idx] = 2;
                                players_data.uppertorsos[idx] = soldier_uppertorso_it->second;

                                auto soldier_lowertorso_it = children_cache.find("LowerTorso");
                                if (soldier_lowertorso_it != children_cache.end()) players_data.lowertorsos[idx] = soldier_lowertorso_it->second;

                                auto soldier_lefthand_it = children_cache.find("LeftHand");
                                if (soldier_lefthand_it != children_cache.end()) players_data.lefthand[idx] = soldier_lefthand_it->second;

                                auto soldier_righthand_it = children_cache.find("RightHand");
                                if (soldier_righthand_it != children_cache.end()) players_data.righthand[idx] = soldier_righthand_it->second;

                                auto soldier_leftlowerarm_it = children_cache.find("LeftLowerArm");
                                if (soldier_leftlowerarm_it != children_cache.end()) players_data.leftlowerarm[idx] = soldier_leftlowerarm_it->second;

                                auto soldier_rightlowerarm_it = children_cache.find("RightLowerArm");
                                if (soldier_rightlowerarm_it != children_cache.end()) players_data.rightlowerarm[idx] = soldier_rightlowerarm_it->second;

                                auto soldier_leftupperarm_it = children_cache.find("LeftUpperArm");
                                if (soldier_leftupperarm_it != children_cache.end()) players_data.leftupperarm[idx] = soldier_leftupperarm_it->second;

                                auto soldier_rightupperarm_it = children_cache.find("RightUpperArm");
                                if (soldier_rightupperarm_it != children_cache.end()) players_data.rightupperarm[idx] = soldier_rightupperarm_it->second;

                                auto soldier_leftfoot_it = children_cache.find("LeftFoot");
                                if (soldier_leftfoot_it != children_cache.end()) players_data.leftfoot[idx] = soldier_leftfoot_it->second;

                                auto soldier_leftlowerleg_it = children_cache.find("LeftLowerLeg");
                                if (soldier_leftlowerleg_it != children_cache.end()) players_data.leftlowerleg[idx] = soldier_leftlowerleg_it->second;

                                auto soldier_leftupperleg_it = children_cache.find("LeftUpperLeg");
                                if (soldier_leftupperleg_it != children_cache.end()) players_data.leftupperleg[idx] = soldier_leftupperleg_it->second;

                                auto soldier_rightlowerleg_it = children_cache.find("RightLowerLeg");
                                if (soldier_rightlowerleg_it != children_cache.end()) players_data.rightlowerleg[idx] = soldier_rightlowerleg_it->second;

                                auto soldier_rightfoot_it = children_cache.find("RightFoot");
                                if (soldier_rightfoot_it != children_cache.end()) players_data.rightfoot[idx] = soldier_rightfoot_it->second;

                                auto soldier_rightupperleg_it = children_cache.find("RightUpperLeg");
                                if (soldier_rightupperleg_it != children_cache.end()) players_data.rightupperleg[idx] = soldier_rightupperleg_it->second;
                            }
                            else {
                                players_data.rigtypes[idx] = 1;

                                auto soldier_leftarm_it = children_cache.find("Left Arm");
                                if (soldier_leftarm_it != children_cache.end()) players_data.lefthand[idx] = soldier_leftarm_it->second;

                                auto soldier_rightarm_it = children_cache.find("Right Arm");
                                if (soldier_rightarm_it != children_cache.end()) players_data.righthand[idx] = soldier_rightarm_it->second;

                                auto soldier_leftleg_it = children_cache.find("Left Leg");
                                if (soldier_leftleg_it != children_cache.end()) players_data.leftfoot[idx] = soldier_leftleg_it->second;

                                auto soldier_rightleg_it = children_cache.find("Right Leg");
                                if (soldier_rightleg_it != children_cache.end()) players_data.rightfoot[idx] = soldier_rightleg_it->second;
                            }

                            if (globals::gamename != "Arsenal") {
                                int i = 0;
                                while (!health_utils::safe_read_health(players_data.humanoids[idx],
                                    players_data.healths[idx],
                                    players_data.maxhealths[idx])) {

                                    i++;
                                    if (i == 10) {
                                        break;
                                    }
                                }
                            }

                            players_data.armors[idx] = 0;
                            players_data.toolnames[idx] = "[Soldier Weapon]";
                            players_data.statuses[idx] = (players_data.healths[idx] > 0 && players_data.hrps[idx].is_valid()) ? 1 : 0;
                            players_data.flags[idx] = "US";

                            players_data.count++;
                        }

                        if (players_data.count >= 4096) break;
                    }
                }
            }
            catch (...) {}
        }

        final_players.clear();
        final_players.reserve(players_data.count);

        size_t final_count = features::visuals::soldier_esp_enabled ? players_data.count : regular_players_count;

        for (size_t i = 0; i < final_count; ++i) {
            player entity(players_data.addresses[i]);
            entity.main = players_data.mains[i];
            entity.name = std::move(players_data.names[i]);
            entity.displayname = std::move(players_data.displaynames[i]);
            entity.head = players_data.heads[i];
            entity.hrp = players_data.hrps[i];
            entity.humanoid = players_data.humanoids[i];
            entity.torso = players_data.torsos[i];
            entity.uppertorso = players_data.uppertorsos[i];
            entity.lowertorso = players_data.lowertorsos[i];
            entity.lefthand = players_data.lefthand[i];
            entity.righthand = players_data.righthand[i];
            entity.leftlowerarm = players_data.leftlowerarm[i];
            entity.rightlowerarm = players_data.rightlowerarm[i];
            entity.leftupperarm = players_data.leftupperarm[i];
            entity.rightupperarm = players_data.rightupperarm[i];
            entity.leftfoot = players_data.leftfoot[i];
            entity.rightfoot = players_data.rightfoot[i];
            entity.leftlowerleg = players_data.leftlowerleg[i];
            entity.rightlowerleg = players_data.rightlowerleg[i];
            entity.leftupperleg = players_data.leftupperleg[i];
            entity.rightupperleg = players_data.rightupperleg[i];
            entity.bodyeffects = players_data.bodyeffects[i];
            entity.tool = players_data.tools[i];
            entity.team = players_data.teams[i];
            entity.health = players_data.healths[i];
            entity.maxhealth = players_data.maxhealths[i];
            entity.armor = players_data.armors[i];
            entity.rigtype = players_data.rigtypes[i];
            entity.status = players_data.statuses[i];
            entity.toolname = std::move(players_data.toolnames[i]);
            entity.flag = std::move(players_data.flags[i]);
            entity.pictureDownloaded = false;

            // The UserId child was never filled in, so every avatar lookup had no id to work with.
            // Same read as instance::read_userid, done inline because roblox::player is not an
            // instance and has no such member.
            entity.userid = read<roblox::instance>(entity.address + offsets::UserId);

            entity.bones.clear();
            entity.bones.reserve(20);

            const std::vector<std::string>& current_bone_names = (globals::gamename == "Bad Business") ? bad_business_bone_names :
                (globals::gamename == "PhantomForces") ? phantom_forces_bone_names : bone_names;

            for (const auto& bone_name : current_bone_names) {
                auto it = children_cache.find(bone_name);
                if (it != children_cache.end() && it->second.is_valid()) {
                    entity.bones.push_back(it->second);
                }
            }

            if (globals::gamename == "Bad Business") {
                if (players_data.statuses[i] == 3) {
                    if (entity.name == globals::localplayer.get_name()) {
                        globals::lp = entity;
                    }
                }
            }
            else {
                if (entity.name == globals::localplayer.get_name()) {
                    globals::lp = entity;
                }
            }
            if (!visuals::settings::localesp && entity.name == globals::lp.name)continue;
            final_players.emplace_back(std::move(entity));
        }

        player_access::set_cached_players(std::move(final_players));

        cache::data_updating.store(false, std::memory_order_release);
        last_full_update = now;
        std::this_thread::sleep_for(std::chrono::milliseconds(75));
    }

}

void cache::hook_espcache(bool firstrecieved) {
    static enhanced_player_cache player_cache;
    static fast_allocator memory_allocator(128 * 1024 * 1024);

    static auto last_frame_time = std::chrono::steady_clock::now();
    static Vector3 previous_camera_pos = Vector3::zero();
    static uint32_t frame_counter = 0;

#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(GetCurrentThread(), 0x2);
#else
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif

    while (firstrecieved) {
        auto current_frame_time = std::chrono::steady_clock::now();
        auto frame_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_frame_time - last_frame_time).count();

        if (frame_delta < 8333) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }

        if (cache::data_updating.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            continue;
        }

        if (!is_valid_address(globals::localplayer.address) || !is_valid_address(globals::camera.address)) {
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            continue;
        }

        Vector3 camera_position = globals::camera.getPos();
        Vector2 screen_dimensions = optimization_helpers::get_screen_dimensions();

        player_cache.clear_cache();
        memory_allocator.reset_allocator();

        std::vector<player> cached_players = player_access::get_cached_players();
        const size_t player_count = cached_players.size();

        if (player_count == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(2000));
        }

        uint32_t processed_players = 0;
        const uint32_t max_players_per_frame = (std::min)(
            static_cast<uint32_t>(player_count),
            static_cast<uint32_t>(enhanced_player_cache::MAX_ENTITIES)
            );

        for (size_t i = 0; i < player_count && processed_players < max_players_per_frame; ++i) {
            auto player = cached_players[i];

            if (!features::visuals::soldier_esp_enabled &&
                player.name.find("Soldier (") != std::string::npos) {
                continue;
            }

            if (visuals::settings::espteamcheck && globals::is_teammate(player) &&
                !visuals::settings::esp_teamcolor) {
                continue;
            }

            if (!player.hrp.is_valid() || !player.head.is_valid() || player.status != 1) {
                continue;
            }

            int validated_health = 0;
            int validated_maxhealth = 0;

            if (globals::gamename == "Bad Business") {
                validated_health = player.health;
                validated_maxhealth = player.maxhealth;
            }
            else {
                if (!health_utils::safe_read_health(player.humanoid, validated_health, validated_maxhealth)) {
                    validated_health = player.health;
                    validated_maxhealth = player.maxhealth;
                }
            }

            player.health = validated_health;
            player.maxhealth = validated_maxhealth;

            Vector3 player_position = player.hrp.get_pos();
            Vector3 head_position = player.head.get_pos();

            float dx = camera_position.x - player_position.x;
            float dy = camera_position.y - player_position.y;
            float dz = camera_position.z - player_position.z;
            float distance_sq = dx * dx + dy * dy + dz * dz;

            if (distance_sq > 6250000.0f) continue;

            float health_normalized = 0.0f;
            if (player.maxhealth > 0) {
                health_normalized = (std::max)(0.0f, (std::min)(1.0f,
                    static_cast<float>(player.health) / static_cast<float>(player.maxhealth)));
            }

            player_cache.add_entity(player, player_position, head_position, distance_sq, health_normalized);
            processed_players++;

            if (processed_players % 64 == 0) {
                std::this_thread::yield();
            }
        }

        if (processed_players == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(5000));
        }

        player_cache.frustum_cull_and_compact(camera_position, 2500.0f * 2500.0f, 0.01f);

        uint32_t visible_count = player_cache.entity_count.load(std::memory_order_acquire);

        if (visible_count > 1 && visible_count != 0) {
            player_cache.sort_by_distance();
        }

        player_cache.batch_worldtoscreen(screen_dimensions);
        player_cache.calculate_proper_bounds(screen_dimensions);
        player_cache.cull_offscreen_entities(screen_dimensions);

        thread_local static std::vector<ESPPlayerData> final_output;
        final_output.clear();

        uint32_t final_count = player_cache.entity_count.load(std::memory_order_acquire);
        final_output.reserve(final_count);

        for (uint32_t i = 0; i < final_count; ++i) {
            ESPPlayerData esp_entity;

            esp_entity.player_address = player_cache.player_addresses[i];
            esp_entity.position = { player_cache.position_x[i], player_cache.position_y[i], player_cache.position_z[i] };
            esp_entity.head_position = { player_cache.head_x[i], player_cache.head_y[i], player_cache.head_z[i] };
            esp_entity.screen_position = { player_cache.screen_x[i], player_cache.screen_y[i] };
            esp_entity.top_left = { player_cache.bounds_min_x[i], player_cache.bounds_min_y[i] };
            esp_entity.bottom_right = { player_cache.bounds_max_x[i], player_cache.bounds_max_y[i] };
            esp_entity.distance = std::sqrt(player_cache.distance_squared[i]);
            esp_entity.health = player_cache.health_ratio[i];
            esp_entity.margin = player_cache.box_margins[i];
            esp_entity.valid = true;
            esp_entity.on_screen = (player_cache.visibility_flags[i] & 0x1) != 0;
            esp_entity.teammate = (player_cache.visibility_flags[i] & 0x10) != 0;
            esp_entity.name = player_cache.player_names[i];
            esp_entity.tool_name = player_cache.tool_names[i];

            esp_entity.player_ref.main.address = player_cache.player_addresses[i];
            esp_entity.player_ref.name = player_cache.player_names[i];
            esp_entity.player_ref.toolname = player_cache.tool_names[i];

            esp_entity.player_ref.health = player_cache.player_healths[i];
            esp_entity.player_ref.maxhealth = player_cache.player_max_healths[i];
            esp_entity.player_ref.armor = player_cache.player_armors[i];
            esp_entity.player_ref.rigtype = player_cache.rig_types[i];

            final_output.emplace_back(std::move(esp_entity));
        }

        player_access::set_cached_esp_players(std::move(final_output));

        {
            std::unique_lock<std::mutex> lock(cache::update_mutex);
            globals::cached_camera_position = camera_position;
        }

        last_frame_time = current_frame_time;
        previous_camera_pos = camera_position;
        frame_counter++;

        if (frame_counter % 240 == 0) {
            memory_allocator.reset_allocator();
        }

        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}











struct alignas(32) BonePositionCache {
    Vector3 position;
    bool valid;
    std::chrono::steady_clock::time_point last_update;

    BonePositionCache() : valid(false), last_update(std::chrono::steady_clock::now()) {}
};

struct alignas(64) PlayerBonesCache {
    static const size_t MAX_BONES = 20;

    std::array<BonePositionCache, MAX_BONES> bone_positions;
    uintptr_t player_address;
    std::atomic<uint64_t> position_version{ 0 };

    PlayerBonesCache() : player_address(0) {}

    PlayerBonesCache(const PlayerBonesCache& other)
        : bone_positions(other.bone_positions), player_address(other.player_address),
        position_version(other.position_version.load()) {
    }

    PlayerBonesCache& operator=(const PlayerBonesCache& other) {
        if (this != &other) {
            bone_positions = other.bone_positions;
            player_address = other.player_address;
            position_version.store(other.position_version.load());
        }
        return *this;
    }

    PlayerBonesCache(PlayerBonesCache&& other) noexcept
        : bone_positions(std::move(other.bone_positions)), player_address(other.player_address),
        position_version(other.position_version.load()) {
    }

    PlayerBonesCache& operator=(PlayerBonesCache&& other) noexcept {
        if (this != &other) {
            bone_positions = std::move(other.bone_positions);
            player_address = other.player_address;
            position_version.store(other.position_version.load());
        }
        return *this;
    }

    void update_bone_position(size_t bone_index, const Vector3& position) {
        if (bone_index >= MAX_BONES) return;
        bone_positions[bone_index].position = position;
        bone_positions[bone_index].valid = true;
        bone_positions[bone_index].last_update = std::chrono::steady_clock::now();
        position_version.fetch_add(1, std::memory_order_acq_rel);
    }

    Vector3 get_bone_position(size_t bone_index) const {
        if (bone_index >= MAX_BONES || !bone_positions[bone_index].valid) {
            return Vector3{ 0, 0, 0 };
        }
        return bone_positions[bone_index].position;
    }

    bool is_bone_valid(size_t bone_index) const {
        return bone_index < MAX_BONES && bone_positions[bone_index].valid;
    }
};

struct alignas(32) ChamsCacheData {
    Vector3 position;
    Vector3 size;
    Matrix3 rotation;
    uintptr_t address;
    float distance_sq;
    int userid;
    bool valid;
    std::chrono::steady_clock::time_point last_rotation_update;
    std::chrono::steady_clock::time_point last_size_update;

    ChamsCacheData() : address(0), userid(0), distance_sq(0.0f), valid(false),
        last_rotation_update(std::chrono::steady_clock::now()),
        last_size_update(std::chrono::steady_clock::now()) {
    }
};

struct Polygon_t {
    std::vector<ImVec2> vertices;
    float depth;
    float distance;
    int player_address;

    Polygon_t() { vertices.reserve(8); }
    Polygon_t(std::vector<ImVec2>&& verts, float d, float dist)
        : vertices(std::move(verts)), depth(d), distance(dist) {
    }
};

struct alignas(64) PlayerChamsData {
    static const size_t MAX_PARTS = 16;
    int userid;
    std::array<ChamsCacheData, MAX_PARTS> parts;
    PlayerBonesCache bones_cache;
    uintptr_t player_address;
    std::atomic<uint32_t> part_count{ 0 };
    float last_update_time;
    bool needs_update;

    PlayerChamsData() : player_address(0), last_update_time(0.0f), needs_update(false) {}

    PlayerChamsData(const PlayerChamsData& other)
        : parts(other.parts), userid(0), bones_cache(other.bones_cache), player_address(other.player_address),
        part_count(other.part_count.load()), last_update_time(other.last_update_time),
        needs_update(other.needs_update) {
    }

    PlayerChamsData& operator=(const PlayerChamsData& other) {
        if (this != &other) {
            parts = other.parts;
            bones_cache = other.bones_cache;
            player_address = other.player_address;
            part_count.store(other.part_count.load());
            last_update_time = other.last_update_time;
            needs_update = other.needs_update;
        }
        return *this;
    }

    PlayerChamsData(PlayerChamsData&& other) noexcept
        : parts(std::move(other.parts)), bones_cache(std::move(other.bones_cache)),
        player_address(other.player_address), part_count(other.part_count.load()),
        last_update_time(other.last_update_time), needs_update(other.needs_update) {
    }

    PlayerChamsData& operator=(PlayerChamsData&& other) noexcept {
        if (this != &other) {
            parts = std::move(other.parts);
            bones_cache = std::move(other.bones_cache);
            player_address = other.player_address;
            part_count.store(other.part_count.load());
            last_update_time = other.last_update_time;
            needs_update = other.needs_update;
        }
        return *this;
    }

    void clear_parts() {
        part_count.store(0, std::memory_order_release);
    }

    bool add_part(const ChamsCacheData& part_data) {
        uint32_t current_count = part_count.load(std::memory_order_acquire);
        if (current_count >= MAX_PARTS) return false;

        parts[current_count] = part_data;
        part_count.store(current_count + 1, std::memory_order_release);
        return true;
    }

    uint32_t get_part_count() const {
        return part_count.load(std::memory_order_acquire);
    }
};

float cross_product_2d(const ImVec2& p0, const ImVec2& a, const ImVec2& b) {
    return (a.x - p0.x) * (b.y - p0.y) - (a.y - p0.y) * (b.x - p0.x);
}

std::vector<ImVec2> convexHull(std::vector<ImVec2>& points) {
    int n = points.size();
    if (n <= 3) return points;
    int bottommost_index = 0;
    for (int i = 1; i < n; ++i) {
        if (points[i].y < points[bottommost_index].y || (points[i].y == points[bottommost_index].y && points[i].x < points[bottommost_index].x)) {
            bottommost_index = i;
        }
    }
    std::swap(points[0], points[bottommost_index]);
    ImVec2 p0 = points[0];
    std::sort(points.begin() + 1, points.end(), [&](const ImVec2& a, const ImVec2& b) {
        float cross = cross_product_2d(p0, a, b);
        if (cross == 0) {
            float distSqA = (p0.x - a.x) * (p0.x - a.x) + (p0.y - a.y) * (p0.y - a.y);
            float distSqB = (p0.x - b.x) * (p0.x - b.x) + (p0.y - b.y) * (p0.y - b.y);
            return distSqA < distSqB;
        }
        return cross > 0;
        });
    std::vector<ImVec2> hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);
    for (int i = 2; i < n; ++i) {
        while (hull.size() > 1 && cross_product_2d(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }
    return hull;
}

class alignas(64) enhanced_chams_cache {
private:
    static const size_t MAX_PLAYERS = 256;
    static const size_t MAX_POLYGONS = 2048;

    std::array<PlayerChamsData, MAX_PLAYERS> player_data;
    std::array<Polygon_t, MAX_POLYGONS> polygons;

    std::atomic<uint32_t> player_count{ 0 };
    std::atomic<uint32_t> polygon_count{ 0 };
    std::atomic<uint64_t> frame_version{ 0 };

public:
    enhanced_chams_cache() = default;

    void clear_cache() {
        player_count.store(0, std::memory_order_release);
        polygon_count.store(0, std::memory_order_release);
        frame_version.fetch_add(1, std::memory_order_acq_rel);
    }

    bool add_player_data(const PlayerChamsData& data) {
        uint32_t current_count = player_count.load(std::memory_order_acquire);
        if (current_count >= MAX_PLAYERS) return false;

        player_data[current_count] = data;
        player_count.store(current_count + 1, std::memory_order_release);
        return true;
    }

    void process_polygons() {
        uint32_t p_count = player_count.load(std::memory_order_acquire);
        if (p_count == 0) return;

        uint32_t poly_write_index = 0;

        for (uint32_t i = 0; i < p_count; ++i) {
            const auto& player = player_data[i];
            uint32_t part_count = player.get_part_count();

            for (uint32_t j = 0; j < part_count; ++j) {
                const auto& part = player.parts[j];
                if (!part.valid || poly_write_index >= MAX_POLYGONS) continue;

                if (process_part_polygon(part, poly_write_index, std::sqrt(player.parts[j].distance_sq), player.parts[j].userid)) {
                    poly_write_index++;
                }
            }
        }

        polygon_count.store(poly_write_index, std::memory_order_release);

        if (poly_write_index > 1) {
            std::sort(polygons.begin(), polygons.begin() + poly_write_index,
                [](const Polygon_t& a, const Polygon_t& b) {
                    return a.depth > b.depth;
                });
        }
    }

    std::vector<Polygon_t> get_polygons_copy() const {
        uint32_t count = polygon_count.load(std::memory_order_acquire);
        std::vector<Polygon_t> result;
        result.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            result.push_back(polygons[i]);
        }

        return result;
    }

    uint32_t get_player_count() const {
        return player_count.load(std::memory_order_acquire);
    }

    uint32_t get_polygon_count() const {
        return polygon_count.load(std::memory_order_acquire);
    }

private:
    bool process_part_polygon(const ChamsCacheData& part, uint32_t write_index, float distance, int userid) {
        const float hx = part.size.x * 0.6f;
        const float hy = part.size.y * 0.6f;
        const float hz = part.size.z * 0.6f;

        Vector3 corners[8] = {
            {-hx, -hy, -hz}, {hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz},
            {-hx, -hy, hz}, {hx, -hy, hz}, {-hx, hy, hz}, {hx, hy, hz}
        };

        std::vector<Vector3> cube_vertices;
        cube_vertices.reserve(8);

        for (int i = 0; i < 8; ++i) {
            Vector3& corner = corners[i];
            Vector3 rotated = {
                corner.x * part.rotation.data[0] + corner.y * part.rotation.data[1] + corner.z * part.rotation.data[2],
                corner.x * part.rotation.data[3] + corner.y * part.rotation.data[4] + corner.z * part.rotation.data[5],
                corner.x * part.rotation.data[6] + corner.y * part.rotation.data[7] + corner.z * part.rotation.data[8]
            };
            cube_vertices.emplace_back(
                rotated.x + part.position.x,
                rotated.y + part.position.y,
                rotated.z + part.position.z
            );
        }

        std::vector<ImVec2> screen_vertices;
        screen_vertices.reserve(8);
        float total_depth = 0.0f;
        int visible_count = 0;

        for (const auto& vertex : cube_vertices) {
            Vector2 screen_pos = roblox::worldtoscreen(vertex);
            if (screen_pos.y <= 0 || screen_pos.x <= 0) continue;

            screen_vertices.emplace_back(screen_pos.x, screen_pos.y);
            total_depth += vertex.z;
            visible_count++;
        }

        if (visible_count < 3) return false;

        std::vector<ImVec2> hull = convexHull(screen_vertices);
        if (hull.size() < 3) return false;

        polygons[write_index] = Polygon_t(std::move(hull), total_depth * 0.125f, distance);
        polygons[write_index].player_address = userid;
        return true;
    }
};

namespace cache {
    static std::shared_mutex chams_data_mutex;
    static std::shared_mutex bones_cache_mutex;
    static enhanced_chams_cache chams_cache;
    static std::unordered_map<uintptr_t, PlayerBonesCache> global_bones_cache;
}

void update_player_bones_cache(const roblox::player& player) {
    std::unique_lock<std::shared_mutex> lock(cache::bones_cache_mutex);

    auto& bones_cache = cache::global_bones_cache[player.main.address];
    bones_cache.player_address = player.main.address;

    const std::vector<const roblox::instance*> bone_parts = {
        &player.head, &player.hrp, &player.torso, &player.uppertorso, &player.lowertorso,
        &player.lefthand, &player.righthand, &player.leftlowerarm, &player.rightlowerarm,
        &player.leftupperarm, &player.rightupperarm, &player.leftfoot, &player.rightfoot,
        &player.leftlowerleg, &player.rightlowerleg, &player.leftupperleg, &player.rightupperleg,
        &player.leftupperarm, &player.rightupperarm
    };

    for (size_t i = 0; i < bone_parts.size() && i < PlayerBonesCache::MAX_BONES; ++i) {
        if (bone_parts[i]->is_valid()) {
            try {
                Vector3 bone_pos = bone_parts[i]->get_pos();
                bones_cache.update_bone_position(i, bone_pos);
            }
            catch (...) {
                continue;
            }
        }
    }
}

void cache::hook_chamscache(bool firstrecieved) {
    static std::vector<PlayerChamsData> temp_players;
    temp_players.reserve(256);

    static auto last_frame_time = std::chrono::steady_clock::now();
    static auto last_bone_update = std::chrono::steady_clock::now();
    static const uint64_t chams_frame_time = 1;
    static const uint64_t bone_update_time = 1;
    static const uint64_t rotation_update_time = 1;
    static const uint64_t size_update_time = 1;
    static const float max_distance_sq = 2500.0f * 2500.0f;

#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif

    while (firstrecieved) {
        auto current_time = std::chrono::steady_clock::now();
        auto frame_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_frame_time).count();
        auto bone_delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_bone_update).count();

        if ((!visuals::settings::chams && !visuals::settings::chamsoutline && !visuals::settings::chamsglow && !visuals::settings::skeletons)) {
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            continue;
        }
        //if (cache::data_updating.load(std::memory_order_acquire)) {
        //    std::this_thread::sleep_for(std::chrono::microseconds(1));
        //    continue;
        //}

        auto cached_players = player_access::get_cached_players();

        //if (cached_players.empty()) {
        //    std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //    continue;
        //}

        if (bone_delta >= bone_update_time) {

            std::this_thread::sleep_for(std::chrono::microseconds(5));
            last_bone_update = current_time;
        }

        if (frame_delta < chams_frame_time) {
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            continue;
        }

        Vector3 camera_pos = player_access::get_cached_camera_position();
        temp_players.clear();

        for (const auto& player : cached_players) {
            if (!player.main.address || player.status != 1) continue;
            if (visuals::settings::espteamcheck && globals::is_teammate(player)) continue;
   
            Vector3 player_pos = player.hrp.get_pos();
            float dx = camera_pos.x - player_pos.x;
            float dy = camera_pos.y - player_pos.y;
            float dz = camera_pos.z - player_pos.z;
            float dist_sq = dx * dx + dy * dy + dz * dz;

            if (dist_sq > max_distance_sq) continue;

            PlayerChamsData player_data;
            player_data.player_address = player.main.address;
            player_data.last_update_time = 0.0f;
            player_data.needs_update = true;
            player_data.clear_parts();

            std::vector<const roblox::instance*> parts_to_process;
            if (player.rigtype == 2) {
                parts_to_process = {
                    &player.head, &player.uppertorso, &player.lowertorso,
                    &player.leftupperarm, &player.rightupperarm,
                    &player.leftlowerarm, &player.rightlowerarm,
                    &player.lefthand, &player.righthand,
                    &player.leftupperleg, &player.rightupperleg,
                    &player.leftlowerleg, &player.rightlowerleg,
                    &player.leftfoot, &player.rightfoot
                };
            }
            else if (player.rigtype == 1) {
                parts_to_process = {
                    &player.head, &player.torso,
                    &player.lefthand, &player.righthand,
                    &player.leftfoot, &player.rightfoot
                };
            }
            else {
                continue;
            }

            for (const auto* part : parts_to_process) {
                if (!part->is_valid()) continue;

                try {
                    ChamsCacheData cache_data;
                    CFrame cframe = part->read_cframe();
                    cache_data.address = part->address;
                    cache_data.userid = player.userid.address;
                    cache_data.position = cframe.position;
                    cache_data.size = part->get_part_size();
                    cache_data.rotation = cframe.rotation_matrix;
                    cache_data.distance_sq = dist_sq;
                    cache_data.last_rotation_update = current_time;
                    cache_data.last_size_update = current_time;
                    
                    cache_data.valid = (cache_data.size.x >= 0.2f &&
                        cache_data.size.y >= 0.2f &&
                        cache_data.size.z >= 0.2f);

                    if (cache_data.valid) {
                        player_data.add_part(cache_data);
                    }
                }
                catch (...) {
                    continue;
                }
            }

            if (player_data.get_part_count() > 0) {
                temp_players.emplace_back(std::move(player_data));
            }
        }

        {
            std::unique_lock<std::shared_mutex> lock(cache::chams_data_mutex);
            cache::chams_cache.clear_cache();

            for (const auto& player_data : temp_players) {
                cache::chams_cache.add_player_data(player_data);
            }

            cache::chams_cache.process_polygons();
        }
       
        last_frame_time = current_time;
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

namespace player_access {
    std::vector<Polygon_t> get_cached_chams_polygons() {
        std::shared_lock<std::shared_mutex> lock(cache::chams_data_mutex);
        return cache::chams_cache.get_polygons_copy();
    }

    Vector3 get_cached_bone_position(uintptr_t player_address, size_t bone_index) {
        std::shared_lock<std::shared_mutex> lock(cache::bones_cache_mutex);
        auto it = cache::global_bones_cache.find(player_address);
        if (it != cache::global_bones_cache.end() && bone_index < PlayerBonesCache::MAX_BONES) {
            return it->second.get_bone_position(bone_index);
        }
        return Vector3{ 0, 0, 0 };
    }

    bool is_cached_bone_valid(uintptr_t player_address, size_t bone_index) {
        std::shared_lock<std::shared_mutex> lock(cache::bones_cache_mutex);
        auto it = cache::global_bones_cache.find(player_address);
        if (it != cache::global_bones_cache.end() && bone_index < PlayerBonesCache::MAX_BONES) {
            return it->second.is_bone_valid(bone_index);
        }
        return false;
    }
}
