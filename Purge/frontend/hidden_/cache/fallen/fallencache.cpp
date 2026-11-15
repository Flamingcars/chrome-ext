#include "fallencache.h"
#include "../../../../globals.h"
#include "../../modules.h"
#include "../../../../backend/imgui/imgui/imgui.h"

ResourceESPCache::ResourceESPCache() :
    last_update(std::chrono::steady_clock::now()),
    node_names({ "Phosphate_Node", "Metal_Node", "Stone_Node" }),
    plant_names({ "Corn Plant", "Wool Plant", "Pumpkin Plant", "Tomato Plant", "Lemon Plant", "Blueberry Plant", "Raspberry Plant" }),
    animal_names({ "PREFAB_ANIMAL_DEER", "PREFAB_ANIMAL_WILDBOAR", "PREFAB_ANIMAL_WOLF" }) {

    resources.reserve(INITIAL_CAPACITY);
    animals.reserve(ANIMAL_INITIAL_CAPACITY);
}

ResourceType ResourceESPCache::get_resource_type(const std::string& name) {
    if (name == "Phosphate_Node") return ResourceType::PHOSPHATE_NODE;
    if (name == "Metal_Node") return ResourceType::METAL_NODE;
    if (name == "Stone_Node") return ResourceType::STONE_NODE;
    if (name == "Corn Plant") return ResourceType::CORN_PLANT;
    if (name == "Wool Plant") return ResourceType::WOOL_PLANT;
    if (name == "Pumpkin Plant") return ResourceType::PUMPKIN_PLANT;
    if (name == "Tomato Plant") return ResourceType::TOMATO_PLANT;
    if (name == "Lemon Plant") return ResourceType::LEMON_PLANT;
    if (name == "Blueberry Plant") return ResourceType::BLUEBERRY_PLANT;
    if (name == "Raspberry Plant") return ResourceType::RASPBERRY_PLANT;
    return ResourceType::PHOSPHATE_NODE;
}

AnimalType ResourceESPCache::get_animal_type(const std::string& name) {
    if (name == "PREFAB_ANIMAL_DEER") return AnimalType::DEER;
    if (name == "PREFAB_ANIMAL_WILDBOAR") return AnimalType::WILDBOAR;
    if (name == "PREFAB_ANIMAL_WOLF") return AnimalType::WOLF;
    return AnimalType::DEER;
}

void ResourceESPCache::ensure_capacity(size_t needed_size) {
    if (resources.size() < needed_size) {
        size_t new_size = std::max(needed_size, resources.size() * 2);
        resources.resize(new_size);
    }
}

void ResourceESPCache::ensure_animal_capacity(size_t needed_size) {
    if (animals.size() < needed_size) {
        size_t new_size = std::max(needed_size, animals.size() * 2);
        animals.resize(new_size);
    }
}

void ResourceESPCache::scan_specific_nodes() {
    try {
        auto nodes_container = globals::workspace.findfirstchild("Nodes");
        if (!nodes_container.is_valid()) return;

        auto check_and_add_node = [&](const std::string& node_name, bool enabled) {
            if (!enabled) return;

            auto nodes = nodes_container.get_children();
            for (const auto& node : nodes) {
                if (!node.is_valid()) continue;

                auto node_name_obj = node.get_name();
                if (node_name_obj != node_name) continue;

                auto main_part = node.findfirstchild("Main");
                if (!main_part.is_valid()) continue;

                uint32_t current_count = resource_count.load(std::memory_order_acquire);

                try {
                    Vector3 pos = main_part.get_pos();
                    Vector3 size = main_part.get_part_size();
                    uint64_t addr = main_part.address;

                    auto addr_it = address_to_index.find(addr);
                    if (addr_it != address_to_index.end()) {
                        uint32_t idx = addr_it->second;
                        if (idx < current_count && idx < resources.size()) {
                            resources[idx].position = pos;
                            resources[idx].size = size;
                            resources[idx].valid = true;

                            if (globals::localplayer.is_valid()) {
                                Vector3 player_pos = globals::lp.hrp.get_pos();
                                Vector3 diff = pos - player_pos;
                                resources[idx].distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                            }
                            continue;
                        }
                    }

                    float distance = 0.0f;
                    if (globals::localplayer.is_valid()) {
                        Vector3 player_pos = globals::lp.hrp.get_pos();
                        Vector3 diff = pos - player_pos;
                        distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    }

                    if (distance <= MAX_DISTANCE) {
                        ensure_capacity(current_count + 1);

                        ResourceRenderData& resource = resources[current_count];
                        resource.position = pos;
                        resource.size = size;
                        resource.distance = distance;
                        resource.type = get_resource_type(node_name);
                        resource.name = node_name;
                        resource.valid = true;
                        resource.address = addr;

                        address_to_index[addr] = current_count;
                        resource_count.store(current_count + 1, std::memory_order_release);
                    }
                }
                catch (...) {}
            }
            };

        check_and_add_node("Phosphate_Node", features::visuals::phosphateesp);
        check_and_add_node("Metal_Node", features::visuals::metalesp);
        check_and_add_node("Stone_Node", features::visuals::stoneesp);
    }
    catch (...) {}
}

void ResourceESPCache::scan_specific_plants() {
    try {
        auto plants_container = globals::workspace.findfirstchild("Plants");
        if (!plants_container.is_valid()) return;

        auto check_and_add_plant = [&](const std::string& plant_name, bool enabled) {
            if (!enabled) return;

            auto plants = plants_container.get_children();
            for (const auto& plant : plants) {
                if (!plant.is_valid()) continue;

                auto plant_name_obj = plant.get_name();
                if (plant_name_obj != plant_name) continue;

                auto main_part = plant.findfirstchild("Main");
                if (!main_part.is_valid()) continue;

                uint32_t current_count = resource_count.load(std::memory_order_acquire);

                try {
                    Vector3 pos = main_part.get_pos();
                    Vector3 size = main_part.get_part_size();
                    uint64_t addr = main_part.address;

                    auto addr_it = address_to_index.find(addr);
                    if (addr_it != address_to_index.end()) {
                        uint32_t idx = addr_it->second;
                        if (idx < current_count && idx < resources.size()) {
                            resources[idx].position = pos;
                            resources[idx].size = size;
                            resources[idx].valid = true;

                            if (globals::localplayer.is_valid()) {
                                Vector3 player_pos = globals::lp.hrp.get_pos();
                                Vector3 diff = pos - player_pos;
                                resources[idx].distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                            }
                            continue;
                        }
                    }

                    float distance = 0.0f;
                    if (globals::localplayer.is_valid()) {
                        Vector3 player_pos = globals::lp.hrp.get_pos();
                        Vector3 diff = pos - player_pos;
                        distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    }

                    if (distance <= MAX_DISTANCE) {
                        ensure_capacity(current_count + 1);

                        ResourceRenderData& resource = resources[current_count];
                        resource.position = pos;
                        resource.size = size;
                        resource.distance = distance;
                        resource.type = get_resource_type(plant_name);
                        resource.name = plant_name;
                        resource.valid = true;
                        resource.address = addr;

                        address_to_index[addr] = current_count;
                        resource_count.store(current_count + 1, std::memory_order_release);
                    }
                }
                catch (...) {}
            }
            };

        check_and_add_plant("Corn Plant", features::visuals::cornesp);
        check_and_add_plant("Wool Plant", features::visuals::woolesp);
        check_and_add_plant("Pumpkin Plant", features::visuals::pumpkinesp);
        check_and_add_plant("Tomato Plant", features::visuals::tomatoesp);
        check_and_add_plant("Lemon Plant", features::visuals::lemonesp);
        check_and_add_plant("Blueberry Plant", features::visuals::blueberryesp);
        check_and_add_plant("Raspberry Plant", features::visuals::raspberryesp);
    }
    catch (...) {}
}

void ResourceESPCache::scan_animals() {
    try {
        auto animals_container = globals::workspace.findfirstchild("Animals");
        if (!animals_container.is_valid()) return;

        auto check_and_add_animal = [&](const std::string& animal_name, bool enabled) {
            if (!enabled) return;

            auto animals_list = animals_container.get_children();
            for (const auto& animal : animals_list) {
                if (!animal.is_valid()) continue;

                auto animal_name_obj = animal.get_name();
                if (animal_name_obj != animal_name) continue;

                auto main_part = animal.findfirstchild("HumanoidRootPart");
                if (!main_part.is_valid()) continue;

                uint32_t current_count = animal_count.load(std::memory_order_acquire);

                try {
                    Vector3 pos = main_part.get_pos();
                    Vector3 size = main_part.get_part_size();
                    uint64_t addr = main_part.address;

                    auto addr_it = animal_address_to_index.find(addr);
                    if (addr_it != animal_address_to_index.end()) {
                        uint32_t idx = addr_it->second;
                        if (idx < current_count && idx < animals.size()) {
                            animals[idx].position = pos;
                            animals[idx].size = size;
                            animals[idx].valid = true;

                            if (globals::localplayer.is_valid()) {
                                Vector3 player_pos = globals::lp.hrp.get_pos();
                                Vector3 diff = pos - player_pos;
                                animals[idx].distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                            }
                            continue;
                        }
                    }

                    float distance = 0.0f;
                    if (globals::localplayer.is_valid()) {
                        Vector3 player_pos = globals::lp.hrp.get_pos();
                        Vector3 diff = pos - player_pos;
                        distance = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    }

                    if (distance <= MAX_DISTANCE) {
                        ensure_animal_capacity(current_count + 1);

                        AnimalRenderData& animal_data = animals[current_count];
                        animal_data.position = pos;
                        animal_data.size = size;
                        animal_data.distance = distance;
                        animal_data.type = get_animal_type(animal_name);
                        animal_data.name = animal_name;
                        animal_data.valid = true;
                        animal_data.address = addr;

                        animal_address_to_index[addr] = current_count;
                        animal_count.store(current_count + 1, std::memory_order_release);
                    }
                }
                catch (...) {}
            }
            };

        check_and_add_animal("PREFAB_ANIMAL_DEER", features::visuals::deeresp);
        check_and_add_animal("PREFAB_ANIMAL_WILDBOAR", features::visuals::wildboaresp);
        check_and_add_animal("PREFAB_ANIMAL_WOLF", features::visuals::wolfesp);
    }
    catch (...) {}
}

void ResourceESPCache::update_screen_positions() {
    uint32_t count = resource_count.load(std::memory_order_acquire);
    auto dimensions = globals::visualengine.get_dimensions();

    for (uint32_t i = 0; i < count; ++i) {
        if (!resources[i].valid) continue;

        try {
            resources[i].screen_position = roblox::worldtoscreen(resources[i].position);
            resources[i].on_screen = (resources[i].screen_position.x != -1 &&
                resources[i].screen_position.y != -1 &&
                resources[i].screen_position.x > 0 &&
                resources[i].screen_position.x < dimensions.x &&
                resources[i].screen_position.y > 0 &&
                resources[i].screen_position.y < dimensions.y);
        }
        catch (...) {
            resources[i].on_screen = false;
        }
    }
}

void ResourceESPCache::update_animal_screen_positions() {
    uint32_t count = animal_count.load(std::memory_order_acquire);
    auto dimensions = globals::visualengine.get_dimensions();

    for (uint32_t i = 0; i < count; ++i) {
        if (!animals[i].valid) continue;

        try {
            animals[i].screen_position = roblox::worldtoscreen(animals[i].position);
            animals[i].on_screen = (animals[i].screen_position.x != -1 &&
                animals[i].screen_position.y != -1 &&
                animals[i].screen_position.x > 0 &&
                animals[i].screen_position.x < dimensions.x &&
                animals[i].screen_position.y > 0 &&
                animals[i].screen_position.y < dimensions.y);
        }
        catch (...) {
            animals[i].on_screen = false;
        }
    }
}

void ResourceESPCache::cleanup_invalid_resources() {
    uint32_t count = resource_count.load(std::memory_order_acquire);
    uint32_t write_index = 0;

    for (uint32_t read_index = 0; read_index < count; ++read_index) {
        if (resources[read_index].valid) {
            if (write_index != read_index) {
                resources[write_index] = resources[read_index];

                auto it = address_to_index.find(resources[write_index].address);
                if (it != address_to_index.end()) {
                    it->second = write_index;
                }
            }
            write_index++;
        }
        else {
            address_to_index.erase(resources[read_index].address);
        }
    }

    resource_count.store(write_index, std::memory_order_release);
}

void ResourceESPCache::cleanup_invalid_animals() {
    uint32_t count = animal_count.load(std::memory_order_acquire);
    uint32_t write_index = 0;

    for (uint32_t read_index = 0; read_index < count; ++read_index) {
        if (animals[read_index].valid) {
            if (write_index != read_index) {
                animals[write_index] = animals[read_index];

                auto it = animal_address_to_index.find(animals[write_index].address);
                if (it != animal_address_to_index.end()) {
                    it->second = write_index;
                }
            }
            write_index++;
        }
        else {
            animal_address_to_index.erase(animals[read_index].address);
        }
    }

    animal_count.store(write_index, std::memory_order_release);
}

void ResourceESPCache::update() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();

    if (elapsed < UPDATE_INTERVAL_MS) return;
    last_update = now;

    bool any_resource_esp_enabled = features::visuals::stoneesp ||
        features::visuals::phosphateesp ||
        features::visuals::metalesp ||
        features::visuals::cornesp ||
        features::visuals::woolesp ||
        features::visuals::pumpkinesp ||
        features::visuals::tomatoesp ||
        features::visuals::lemonesp ||
        features::visuals::blueberryesp ||
        features::visuals::raspberryesp;


    if (!any_resource_esp_enabled) {
        resource_count.store(0, std::memory_order_release);
        address_to_index.clear();
    }
    else {
        try {
            resource_count.store(0, std::memory_order_release);
            address_to_index.clear();

            scan_specific_nodes();
            scan_specific_plants();
            update_screen_positions();
            cleanup_invalid_resources();
        }
        catch (...) {}
    }

    bool any_animal_esp_enabled = features::visuals::deeresp ||
        features::visuals::wildboaresp ||
        features::visuals::wolfesp;

    if (!any_animal_esp_enabled) {
        animal_count.store(0, std::memory_order_release);
        animal_address_to_index.clear();
    }
    else {
        try {
            animal_count.store(0, std::memory_order_release);
            animal_address_to_index.clear();

            scan_animals();
            update_animal_screen_positions();
            cleanup_invalid_animals();
        }
        catch (...) {}
    }
}

std::vector<ResourceRenderData> ResourceESPCache::get_render_data() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    std::vector<ResourceRenderData> result;
    uint32_t count = resource_count.load(std::memory_order_acquire);
    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (resources[i].valid) {
            result.push_back(resources[i]);
        }
    }

    return result;
}

std::vector<AnimalRenderData> ResourceESPCache::get_animal_render_data() {
    std::lock_guard<std::mutex> lock(cache_mutex);

    std::vector<AnimalRenderData> result;
    uint32_t count = animal_count.load(std::memory_order_acquire);
    result.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (animals[i].valid) {
            result.push_back(animals[i]);
        }
    }

    return result;
}

ImU32 ResourceESPCache::get_resource_color(ResourceType type) const {
    switch (type) {
    case ResourceType::PHOSPHATE_NODE:
        return IM_COL32(
            static_cast<int>(visuals::settings::phosphateespcolor[0] * 255),
            static_cast<int>(visuals::settings::phosphateespcolor[1] * 255),
            static_cast<int>(visuals::settings::phosphateespcolor[2] * 255),
            static_cast<int>(visuals::settings::phosphateespcolor[3] * 255)
        );
    case ResourceType::METAL_NODE:
        return IM_COL32(
            static_cast<int>(visuals::settings::metalespcolor[0] * 255),
            static_cast<int>(visuals::settings::metalespcolor[1] * 255),
            static_cast<int>(visuals::settings::metalespcolor[2] * 255),
            static_cast<int>(visuals::settings::metalespcolor[3] * 255)
        );
    case ResourceType::STONE_NODE:
        return IM_COL32(
            static_cast<int>(visuals::settings::stoneespcolor[0] * 255),
            static_cast<int>(visuals::settings::stoneespcolor[1] * 255),
            static_cast<int>(visuals::settings::stoneespcolor[2] * 255),
            static_cast<int>(visuals::settings::stoneespcolor[3] * 255)
        );
    case ResourceType::CORN_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::cornespcolor[0] * 255),
            static_cast<int>(visuals::settings::cornespcolor[1] * 255),
            static_cast<int>(visuals::settings::cornespcolor[2] * 255),
            static_cast<int>(visuals::settings::cornespcolor[3] * 255)
        );
    case ResourceType::WOOL_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::woolespcolor[0] * 255),
            static_cast<int>(visuals::settings::woolespcolor[1] * 255),
            static_cast<int>(visuals::settings::woolespcolor[2] * 255),
            static_cast<int>(visuals::settings::woolespcolor[3] * 255)
        );
    case ResourceType::PUMPKIN_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::pumpkinespcolor[0] * 255),
            static_cast<int>(visuals::settings::pumpkinespcolor[1] * 255),
            static_cast<int>(visuals::settings::pumpkinespcolor[2] * 255),
            static_cast<int>(visuals::settings::pumpkinespcolor[3] * 255)
        );
    case ResourceType::TOMATO_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::tomatoespcolor[0] * 255),
            static_cast<int>(visuals::settings::tomatoespcolor[1] * 255),
            static_cast<int>(visuals::settings::tomatoespcolor[2] * 255),
            static_cast<int>(visuals::settings::tomatoespcolor[3] * 255)
        );
    case ResourceType::LEMON_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::lemonespcolor[0] * 255),
            static_cast<int>(visuals::settings::lemonespcolor[1] * 255),
            static_cast<int>(visuals::settings::lemonespcolor[2] * 255),
            static_cast<int>(visuals::settings::lemonespcolor[3] * 255)
        );
    case ResourceType::BLUEBERRY_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::blueberryespcolor[0] * 255),
            static_cast<int>(visuals::settings::blueberryespcolor[1] * 255),
            static_cast<int>(visuals::settings::blueberryespcolor[2] * 255),
            static_cast<int>(visuals::settings::blueberryespcolor[3] * 255)
        );
    case ResourceType::RASPBERRY_PLANT:
        return IM_COL32(
            static_cast<int>(visuals::settings::raspberryespcolor[0] * 255),
            static_cast<int>(visuals::settings::raspberryespcolor[1] * 255),
            static_cast<int>(visuals::settings::raspberryespcolor[2] * 255),
            static_cast<int>(visuals::settings::raspberryespcolor[3] * 255)
        );
    default: return IM_COL32(255, 0, 255, 255);
    }
}

ImU32 ResourceESPCache::get_animal_color(AnimalType type) const {
    switch (type) {
    case AnimalType::DEER:
        return IM_COL32(
            static_cast<int>(visuals::settings::deercolor[0] * 255),
            static_cast<int>(visuals::settings::deercolor[1] * 255),
            static_cast<int>(visuals::settings::deercolor[2] * 255),
            static_cast<int>(visuals::settings::deercolor[3] * 255)
        );
    case AnimalType::WILDBOAR:
        return IM_COL32(
            static_cast<int>(visuals::settings::wildboarcolor[0] * 255),
            static_cast<int>(visuals::settings::wildboarcolor[1] * 255),
            static_cast<int>(visuals::settings::wildboarcolor[2] * 255),
            static_cast<int>(visuals::settings::wildboarcolor[3] * 255)
        );
    case AnimalType::WOLF:
        return IM_COL32(
            static_cast<int>(visuals::settings::wolfcolor[0] * 255),
            static_cast<int>(visuals::settings::wolfcolor[1] * 255),
            static_cast<int>(visuals::settings::wolfcolor[2] * 255),
            static_cast<int>(visuals::settings::wolfcolor[3] * 255)
        );
    default: return IM_COL32(255, 0, 255, 255);
    }
}

const char* ResourceESPCache::get_resource_name(ResourceType type) const {
    switch (type) {
    case ResourceType::PHOSPHATE_NODE: return "Phosphate";
    case ResourceType::METAL_NODE: return "Metal";
    case ResourceType::STONE_NODE: return "Stone";
    case ResourceType::CORN_PLANT: return "Corn";
    case ResourceType::WOOL_PLANT: return "Wool";
    case ResourceType::PUMPKIN_PLANT: return "Pumpkin";
    case ResourceType::TOMATO_PLANT: return "Tomato";
    case ResourceType::LEMON_PLANT: return "Lemon";
    case ResourceType::BLUEBERRY_PLANT: return "Blueberry";
    case ResourceType::RASPBERRY_PLANT: return "Raspberry";
    default: return "Unknown";
    }
}

const char* ResourceESPCache::get_animal_name(AnimalType type) const {
    switch (type) {
    case AnimalType::DEER: return "Deer";
    case AnimalType::WILDBOAR: return "Wild Boar";
    case AnimalType::WOLF: return "Wolf";
    default: return "Unknown";
    }
}

ResourceESPCache g_resource_esp_cache;

void runs::runs_resource_esp_cache(bool should_run) {
    while (should_run) {
        try {
            g_resource_esp_cache.update();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        catch (...) {
        }
    }
}

namespace render {
    struct ResourceFadeAnimation {
        float current_alpha = 0.0f;
        float target_alpha = 0.0f;
        std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

        void update(bool enabled) {
            target_alpha = enabled ? 1.0f : 0.0f;

            auto now = std::chrono::steady_clock::now();
            float delta_seconds = std::chrono::duration<float>(now - last_update).count();
            last_update = now;

            if (abs(current_alpha - target_alpha) > 0.001f) {
                float lerp_speed = 8.0f * delta_seconds;
                current_alpha += (target_alpha - current_alpha) * lerp_speed;
                current_alpha = std::clamp(current_alpha, 0.0f, 1.0f);
            }
            else {
                current_alpha = target_alpha;
            }
        }

        ImU32 apply_fade(ImU32 color) {
            ImU32 alpha = static_cast<ImU32>(255 * current_alpha);
            return (color & 0x00FFFFFF) | (alpha << 24);
        }
    };

    static ResourceFadeAnimation resource_fade;
    static ResourceFadeAnimation animal_fade;

    void draw_text_outlined_freetype1(const std::string& text, const ImVec2& position, ImColor color, float font_size) {
        auto draw_list = ImGui::GetBackgroundDrawList();

        ImFont* font = nullptr;
        if (ImGui::GetIO().Fonts->Fonts.Size > 0) {
            font = ImGui::GetIO().Fonts->Fonts[0];
        }

        ImU32 outline_color = IM_COL32(0, 0, 0, (color >> 24) & 0xFF);

        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                if (x == 0 && y == 0) continue;
                ImVec2 outline_pos = ImVec2(position.x + x, position.y + y);
                draw_list->AddText(font, font_size, outline_pos, outline_color, text.c_str());
            }
        }

        draw_list->AddText(font, font_size, position, color, text.c_str());
    }
}

void coolio::render_resource_esp() {
    render::resource_fade.update(features::visuals::resource_esp_enabled);
    if (render::resource_fade.current_alpha <= 0.01f) return;

    std::vector<ResourceRenderData> cached_resources = g_resource_esp_cache.get_render_data();
    if (cached_resources.empty()) return;

    auto draw_list = ImGui::GetBackgroundDrawList();

    for (const auto& resource : cached_resources) {
        if (!resource.valid || !resource.on_screen) continue;

        bool should_render = false;
        switch (resource.type) {
        case ResourceType::PHOSPHATE_NODE:
            should_render = features::visuals::phosphateesp;
            break;
        case ResourceType::METAL_NODE:
            should_render = features::visuals::metalesp;
            break;
        case ResourceType::STONE_NODE:
            should_render = features::visuals::stoneesp;
            break;
        case ResourceType::CORN_PLANT:
            should_render = features::visuals::cornesp;
            break;
        case ResourceType::WOOL_PLANT:
            should_render = features::visuals::woolesp;
            break;
        case ResourceType::PUMPKIN_PLANT:
            should_render = features::visuals::pumpkinesp;
            break;
        case ResourceType::TOMATO_PLANT:
            should_render = features::visuals::tomatoesp;
            break;
        case ResourceType::LEMON_PLANT:
            should_render = features::visuals::lemonesp;
            break;
        case ResourceType::BLUEBERRY_PLANT:
            should_render = features::visuals::blueberryesp;
            break;
        case ResourceType::RASPBERRY_PLANT:
            should_render = features::visuals::raspberryesp;
            break;
        }

        if (!should_render) continue;

        ImU32 resource_color = render::resource_fade.apply_fade(g_resource_esp_cache.get_resource_color(resource.type));
        const char* resource_name = g_resource_esp_cache.get_resource_name(resource.type);

        std::string display_text = std::string(resource_name) + " [" + std::to_string(static_cast<int>(resource.distance)) + "m]";

        ImVec2 text_size = ImGui::CalcTextSize(display_text.c_str());
        text_size.x *= (12.0f / ImGui::GetFontSize());
        text_size.y *= (12.0f / ImGui::GetFontSize());

        ImVec2 text_position = ImVec2(
            resource.screen_position.x - text_size.x / 2.0f,
            resource.screen_position.y - text_size.y / 2.0f
        );

        render::draw_text_outlined_freetype1(display_text, text_position, resource_color, 15.0f);
    }
}

void coolio::render_animal_esp() {
    render::animal_fade.update(features::visuals::animal_esp_enabled);
    if (render::animal_fade.current_alpha <= 0.01f) return;

    std::vector<AnimalRenderData> cached_animals = g_resource_esp_cache.get_animal_render_data();
    if (cached_animals.empty()) return;

    auto draw_list = ImGui::GetBackgroundDrawList();

    for (const auto& animal : cached_animals) {
        if (!animal.valid || !animal.on_screen) continue;

        bool should_render = false;
        switch (animal.type) {
        case AnimalType::DEER:
            should_render = features::visuals::deeresp;
            break;
        case AnimalType::WILDBOAR:
            should_render = features::visuals::wildboaresp;
            break;
        case AnimalType::WOLF:
            should_render = features::visuals::wolfesp;
            break;
        }

        if (!should_render) continue;

        ImU32 animal_color = render::animal_fade.apply_fade(g_resource_esp_cache.get_animal_color(animal.type));
        const char* animal_name = g_resource_esp_cache.get_animal_name(animal.type);

        std::string display_text = std::string(animal_name) + " [" + std::to_string(static_cast<int>(animal.distance)) + "m]";

        ImVec2 text_size = ImGui::CalcTextSize(display_text.c_str());
        text_size.x *= (12.0f / ImGui::GetFontSize());
        text_size.y *= (12.0f / ImGui::GetFontSize());

        ImVec2 text_position = ImVec2(
            animal.screen_position.x - text_size.x / 2.0f,
            animal.screen_position.y - text_size.y / 2.0f
        );

        render::draw_text_outlined_freetype1(display_text, text_position, animal_color, 15.0f);
    }
}