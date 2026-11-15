#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <mutex>
#include "../../../../backend/classes/classes.h"
#include "../../../../backend/imgui/imgui/imgui.h"

enum class ResourceType {
    PHOSPHATE_NODE,
    METAL_NODE,
    STONE_NODE,
    CORN_PLANT,
    WOOL_PLANT,
    PUMPKIN_PLANT,
    TOMATO_PLANT,
    LEMON_PLANT,
    BLUEBERRY_PLANT,
    RASPBERRY_PLANT
};

enum class AnimalType {
    DEER,
    WILDBOAR,
    WOLF
};

struct ResourceRenderData {
    Vector3 position;
    Vector2 screen_position;
    Vector3 size;
    float distance;
    ResourceType type;
    std::string name;
    bool valid;
    bool on_screen;
    uint64_t address;

    ResourceRenderData() : distance(0.0f), type(ResourceType::PHOSPHATE_NODE),
        valid(false), on_screen(false), address(0) {
    }
};

struct AnimalRenderData {
    Vector3 position;
    Vector2 screen_position;
    Vector3 size;
    float distance;
    AnimalType type;
    std::string name;
    bool valid;
    bool on_screen;
    uint64_t address;

    AnimalRenderData() : distance(0.0f), type(AnimalType::DEER),
        valid(false), on_screen(false), address(0) {
    }
};

class ResourceESPCache {
private:
    static constexpr uint32_t UPDATE_INTERVAL_MS = 1;
    static constexpr float MAX_DISTANCE = 1000.0f;
    static constexpr size_t INITIAL_CAPACITY = 1000;
    static constexpr size_t ANIMAL_INITIAL_CAPACITY = 500;

    std::vector<ResourceRenderData> resources;
    std::vector<AnimalRenderData> animals;
    std::atomic<uint32_t> resource_count{ 0 };
    std::atomic<uint32_t> animal_count{ 0 };
    std::mutex cache_mutex;
    std::chrono::steady_clock::time_point last_update;
    std::unordered_map<uint64_t, uint32_t> address_to_index;
    std::unordered_map<uint64_t, uint32_t> animal_address_to_index;

    const std::vector<std::string> node_names;
    const std::vector<std::string> plant_names;
    const std::vector<std::string> animal_names;

    ResourceType get_resource_type(const std::string& name);
    AnimalType get_animal_type(const std::string& name);
    void scan_specific_nodes();
    void scan_specific_plants();
    void scan_animals();
    void update_screen_positions();
    void update_animal_screen_positions();
    void cleanup_invalid_resources();
    void cleanup_invalid_animals();
    void ensure_capacity(size_t needed_size);
    void ensure_animal_capacity(size_t needed_size);

public:
    ResourceESPCache();
    void update();
    std::vector<ResourceRenderData> get_render_data();
    std::vector<AnimalRenderData> get_animal_render_data();
    ImU32 get_resource_color(ResourceType type) const;
    ImU32 get_animal_color(AnimalType type) const;
    const char* get_resource_name(ResourceType type) const;
    const char* get_animal_name(AnimalType type) const;
};

extern ResourceESPCache g_resource_esp_cache;

namespace coolio {
    void render_resource_esp();
    void render_animal_esp();
}