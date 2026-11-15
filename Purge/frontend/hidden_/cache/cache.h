#pragma once

#include "../modules.h"
#include "../../../globals.h"
#include "../../../backend/usermode/usermode.h"
#include <atomic>
#include <shared_mutex>
#include <memory_resource>

#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#else
#include <cstdlib>
#endif

using namespace roblox;

class lockfree_arena_allocator {
private:
    alignas(64) char* memory;
    alignas(64) std::atomic<size_t> position;
    size_t size;

public:
    lockfree_arena_allocator(size_t arena_size);
    ~lockfree_arena_allocator();

    template<typename T>
    T* allocate(size_t count = 1) noexcept;
    void reset() noexcept;
    bool is_valid() const noexcept;
};

struct alignas(64) lockfree_player_cache {
    static constexpr size_t CAPACITY = 2048;
    static constexpr size_t CACHE_LINE = 64;

    alignas(CACHE_LINE) std::atomic<uintptr_t> addresses[CAPACITY];
    alignas(CACHE_LINE) std::atomic<uint64_t> address_versions[CAPACITY];
    alignas(CACHE_LINE) instance instances[CAPACITY];
    alignas(CACHE_LINE) Vector3 positions[CAPACITY];
    alignas(CACHE_LINE) Vector3 head_positions[CAPACITY];
    alignas(CACHE_LINE) std::atomic<int> healths[CAPACITY];
    alignas(CACHE_LINE) std::atomic<int> max_healths[CAPACITY];
    alignas(CACHE_LINE) std::atomic<int> armors[CAPACITY];
    alignas(CACHE_LINE) std::atomic<int> rig_types[CAPACITY];
    alignas(CACHE_LINE) std::atomic<bool> valid_flags[CAPACITY];
    alignas(CACHE_LINE) std::atomic<uint64_t> last_updates[CAPACITY];

    alignas(CACHE_LINE) std::atomic<size_t> count;
    alignas(CACHE_LINE) std::atomic<uint64_t> global_version;
    alignas(CACHE_LINE) std::atomic<bool> updating;

    std::string names[CAPACITY];
    std::string display_names[CAPACITY];
    std::string tool_names[CAPACITY];
    std::string flags[CAPACITY];

    std::shared_mutex data_mutex;

    lockfree_player_cache();
    void mark_invalid(size_t index) noexcept;
    bool is_valid_index(size_t index) const noexcept;
    void increment_version() noexcept;
};

struct alignas(64) lockfree_esp_cache {
    static constexpr size_t MAX_PLAYERS = 1024;
    static constexpr size_t CACHE_LINE = 64;

    alignas(CACHE_LINE) std::atomic<uintptr_t> addresses[MAX_PLAYERS];
    alignas(CACHE_LINE) Vector3 positions[MAX_PLAYERS];
    alignas(CACHE_LINE) Vector3 head_positions[MAX_PLAYERS];
    alignas(CACHE_LINE) Vector2 screen_positions[MAX_PLAYERS];
    alignas(CACHE_LINE) Vector2 top_lefts[MAX_PLAYERS];
    alignas(CACHE_LINE) Vector2 bottom_rights[MAX_PLAYERS];
    alignas(CACHE_LINE) float distances[MAX_PLAYERS];
    alignas(CACHE_LINE) float healths[MAX_PLAYERS];
    alignas(CACHE_LINE) float margins[MAX_PLAYERS];
    alignas(CACHE_LINE) std::atomic<bool> validities[MAX_PLAYERS];
    alignas(CACHE_LINE) std::atomic<bool> on_screens[MAX_PLAYERS];
    alignas(CACHE_LINE) std::atomic<uint64_t> update_times[MAX_PLAYERS];

    std::string names[MAX_PLAYERS];
    std::string tool_names[MAX_PLAYERS];

    alignas(CACHE_LINE) std::atomic<size_t> count;
    alignas(CACHE_LINE) std::atomic<uint64_t> version;
    alignas(CACHE_LINE) std::atomic<bool> updating;

    std::shared_mutex cache_mutex;

    lockfree_esp_cache();
    void clear_safe() noexcept;
    bool add_player_safe(const ESPPlayerData& data) noexcept;
};

namespace safe_cache_helpers {
    extern thread_local Vector2 cached_dimensions;
    extern thread_local uint64_t last_dimension_check;
    extern thread_local std::vector<Vector2> screen_points;
    extern thread_local bool points_initialized;
    extern thread_local std::vector<std::string> bone_names;
    extern thread_local std::unordered_map<std::string, instance> children_cache;

    bool is_on_screen_safe(const Vector2& pos) noexcept;
    bool calculate_bounds_safe(const roblox::player& player, Vector2& top_left, Vector2& bottom_right, float& margin) noexcept;
    bool is_valid_player_safe(const roblox::player& player) noexcept;
    bool is_valid_instance_safe(const instance& inst) noexcept;
    Vector3 get_position_safe(const instance& inst) noexcept;
    std::string get_name_safe(const instance& inst) noexcept;
    int get_health_safe(const instance& humanoid) noexcept;
    int get_max_health_safe(const instance& humanoid) noexcept;
}

namespace cache {
    void hook_cache_optimized(bool first_received);
    void hook_esp_cache_optimized(bool first_received);
    void hook_chams_cache_optimized(bool first_received);
}

extern std::unique_ptr<lockfree_arena_allocator> global_arena;
extern std::atomic<bool> cache_shutdown_flag;