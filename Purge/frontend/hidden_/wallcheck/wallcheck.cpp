#include "wallcheck.h"
#include "../../../includes.h"
#include "../../../backend/usermode/usermode.h"

#define max
#undef max
#define min
#undef min

namespace wallcheck {

    std::unique_ptr<WallChecker> g_wall_checker = nullptr;

    WallChecker::WallChecker()
        : should_stop_(false), is_running_(false), walls_updated_(false) {
        cache_ = std::make_unique<ThreadSafeCache>();
        spatial_grid_ = std::make_unique<SpatialGrid>();
        walls_.reserve(10000);
    }

    WallChecker::~WallChecker() {
        stop();
    }

    void WallChecker::start() {
        if (is_running_.load()) return;

        should_stop_ = false;
        is_running_ = true;

        wall_scanner_thread_ = std::thread([this]() { wall_scanning_thread(); });
        visibility_thread_ = std::thread([this]() { visibility_checking_thread(); });
    }

    void WallChecker::stop() {
        if (!is_running_.load()) return;

        should_stop_ = true;

        if (wall_scanner_thread_.joinable()) {
            wall_scanner_thread_.join();
        }

        if (visibility_thread_.joinable()) {
            visibility_thread_.join();
        }

        is_running_ = false;
    }

    bool WallChecker::can_see(const Vector3& from, const Vector3& to) {
        if (!is_valid_vector(from) || !is_valid_vector(to)) {
            return true;
        }

        float distance_sq = distance_squared(from, to);
        if (distance_sq < MIN_DISTANCE_THRESHOLD * MIN_DISTANCE_THRESHOLD) {
            return true;
        }

        bool cached_result;
        if (cache_->get(from, to, cached_result)) {
            return !cached_result;
        }

        std::vector<size_t> nearby_walls = get_nearby_walls(from, to);

        std::shared_lock<std::shared_mutex> lock(walls_mutex_);

        for (size_t wall_idx : nearby_walls) {
            if (wall_idx >= walls_.size()) continue;

            const OptimizedWall& wall = walls_[wall_idx];

            if (ray_intersects_aabb(from, to, wall)) {
                if (ray_intersects_obb(from, to, wall)) {
                    cache_->put(from, to, true);
                    return false;
                }
            }
        }

        cache_->put(from, to, false);
        return true;
    }

    std::vector<roblox::player> WallChecker::get_visible_players() {
        std::shared_lock<std::shared_mutex> lock(visibility_mutex_);
        return visibility_result_.visible_players;
    }

    void WallChecker::wall_scanning_thread() {
        std::unordered_set<uintptr_t> known_primitives;
        std::vector<OptimizedWall> wall_buffer;
        bool first_scan = true;

        while (!should_stop_) {
            if (!globals::workspace.address) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (!first_scan) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }

            if (should_stop_) break;

            known_primitives.clear();
            wall_buffer.clear();
            wall_buffer.reserve(10000);

            if (!is_valid_address(globals::workspace.address)) {
                continue;
            }

            for (const roblox::player& player : globals::cachedplayers) {
                if (!is_valid_address(player.head.address)) continue;

                try {
                    auto parent_children = player.head.read_parent().get_children();
                    auto head_children = player.head.get_children();

                    auto process_bones = [&](const std::vector<roblox::instance>& bones) {
                        for (const auto& bone : bones) {
                            if (!is_valid_address(bone.address)) continue;

                            uintptr_t primitive = read<uintptr_t>(bone.address + offsets::Primitive);
                            if (is_valid_address(primitive)) {
                                known_primitives.insert(primitive);
                            }
                        }
                        };

                    process_bones(parent_children);
                    process_bones(head_children);
                }
                catch (...) {
                    continue;
                }
            }

            try {
                uintptr_t walls1_addr = read<uintptr_t>(globals::workspace.address + offsets::PrimitivesPointer1);
                if (!is_valid_address(walls1_addr)) continue;

                uintptr_t walls2_addr = read<uintptr_t>(walls1_addr + offsets::PrimitivesPointer2);
                if (!is_valid_address(walls2_addr)) continue;

                uintptr_t primitive_base = walls2_addr;

                int max_count = 0xffffff;
                int valid_wall_count = 0;
                int invalid_address_count = 0;

                for (int i = 0; i < max_count && valid_wall_count < 50000; i += sizeof(uintptr_t)) {
                    if (should_stop_) break;

                    uintptr_t current_primitive = 0;

                    try {
                        current_primitive = read<uintptr_t>(primitive_base + i);
                    }
                    catch (...) {
                        break;
                    }

                    if (!is_valid_address(current_primitive)) {
                        invalid_address_count++;
                        if (invalid_address_count > 1000) break;
                        continue;
                    }

                    invalid_address_count = 0;

                    try {
                        int validate_value = read<int>(current_primitive + 0x8);
                        if (validate_value != 0x16) continue;

                        if (known_primitives.find(current_primitive) != known_primitives.end()) continue;

                        Vector3 size = read<Vector3>(current_primitive + offsets::PartSize);
                        bool anchored = read<bool>(current_primitive + offsets::Anchored);
                        float transparency = read<float>(current_primitive + offsets::Transparency);
                        bool can_collide = read<bool>(current_primitive + offsets::CanCollide);

                        if (!can_collide || !anchored || transparency >= 1.0f) continue;

                        if (!is_valid_vector(size)) continue;

                        if (size.x > 500.0f || size.y > 500.0f || size.z > 500.0f ||
                            size.x < 0.5f || size.y < 0.3f || size.z < 0.5f) continue;

                        CFrame cframe = read<CFrame>(current_primitive + offsets::CFrame);
                        if (!is_valid_vector(cframe.position)) continue;

                        if (cframe.position.x == 0.0f && cframe.position.y == 0.0f && cframe.position.z == 0.0f) continue;

                        float distance_from_origin = std::sqrt(cframe.position.x * cframe.position.x +
                            cframe.position.y * cframe.position.y +
                            cframe.position.z * cframe.position.z);
                        if (distance_from_origin < 3.0f) continue;

                        if (!is_valid_matrix(cframe.rotation_matrix)) continue;

                        roblox::wall temp_wall;
                        temp_wall.size = size;
                        temp_wall.position = cframe.position;
                        temp_wall.rotation = cframe.rotation_matrix;
                        temp_wall.cframe = cframe;

                        wall_buffer.emplace_back(temp_wall);
                        valid_wall_count++;
                    }
                    catch (...) {
                        continue;
                    }
                }

                std::sort(wall_buffer.begin(), wall_buffer.end(),
                    [](const OptimizedWall& a, const OptimizedWall& b) {
                        return a.distance_sq_from_origin < b.distance_sq_from_origin;
                    });

                {
                    std::unique_lock<std::shared_mutex> lock(walls_mutex_);
                    walls_ = std::move(wall_buffer);
                    walls_updated_ = true;
                }

                update_spatial_grid();

                first_scan = false;
            }
            catch (...) {
                continue;
            }
        }
    }

    void WallChecker::visibility_checking_thread() {
        while (!should_stop_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            if (globals::cachedplayers.empty()) continue;

            std::vector<roblox::player> visible_players;
            visible_players.reserve(globals::cachedplayers.size());

            for (const auto& player : globals::cachedplayers) {
                if (!is_valid_address(player.head.address)) continue;

                try {
                    Vector3 local_position = globals::lp.head.get_pos();
                    Vector3 target_position = player.head.get_pos();

                    if (can_see(local_position, target_position)) {
                        visible_players.push_back(player);
                    }
                }
                catch (...) {
                    continue;
                }
            }

            {
                std::unique_lock<std::shared_mutex> lock(visibility_mutex_);
                visibility_result_.visible_players = std::move(visible_players);
                visibility_result_.timestamp = std::chrono::steady_clock::now();
            }
        }
    }

    bool WallChecker::ray_intersects_aabb(const Vector3& origin, const Vector3& target, const OptimizedWall& wall) {
        Vector3 direction = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

        if (length < 1e-6f) return false;

        direction.x /= length;
        direction.y /= length;
        direction.z /= length;

        return wall.aabb.intersects_ray(origin, direction, length);
    }

    bool WallChecker::ray_intersects_obb(const Vector3& origin, const Vector3& target, const OptimizedWall& wall) {
        Vector3 direction = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
        float segment_length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

        if (segment_length < 1e-6f) return false;

        direction.x /= segment_length;
        direction.y /= segment_length;
        direction.z /= segment_length;

        Matrix3 inverse_rotation = wall.rotation.transpose();
        Vector3 relative_origin = { origin.x - wall.position.x, origin.y - wall.position.y, origin.z - wall.position.z };
        Vector3 local_origin = inverse_rotation.multiply_vector(relative_origin);
        Vector3 local_dir = inverse_rotation.multiply_vector(direction);

        if (!is_valid_vector(local_origin) || !is_valid_vector(local_dir)) {
            return false;
        }

        constexpr float epsilon = 1e-6f;
        Vector3 half_size = { wall.size.x * 0.5f - epsilon, wall.size.y * 0.5f - epsilon, wall.size.z * 0.5f - epsilon };

        float tmin = 0.0f;
        float tmax = segment_length;

        auto check_axis = [&](float origin_comp, float dir_comp, float half_size_comp) -> bool {
            if (std::abs(dir_comp) < epsilon) {
                return !(origin_comp < -half_size_comp || origin_comp > half_size_comp);
            }

            float t1 = (-half_size_comp - origin_comp) / dir_comp;
            float t2 = (half_size_comp - origin_comp) / dir_comp;

            if (!std::isfinite(t1) || !std::isfinite(t2)) return false;

            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            return tmin <= tmax + epsilon;
            };

        return check_axis(local_origin.x, local_dir.x, half_size.x) &&
            check_axis(local_origin.y, local_dir.y, half_size.y) &&
            check_axis(local_origin.z, local_dir.z, half_size.z) &&
            tmin <= segment_length + epsilon;
    }

    void WallChecker::update_spatial_grid() {
        if (!walls_updated_.load()) return;

        spatial_grid_->clear();

        std::shared_lock<std::shared_mutex> lock(walls_mutex_);

        for (size_t i = 0; i < walls_.size(); ++i) {
            const OptimizedWall& wall = walls_[i];

            int gx = spatial_grid_->world_to_grid(wall.position.x);
            int gy = spatial_grid_->world_to_grid(wall.position.y);
            int gz = spatial_grid_->world_to_grid(wall.position.z);

            spatial_grid_->grid[gx][gy][gz].push_back(i);
        }

        walls_updated_ = false;
    }

    std::vector<size_t> WallChecker::get_nearby_walls(const Vector3& from, const Vector3& to) {
        std::vector<size_t> nearby_walls;
        nearby_walls.reserve(MAX_WALLS_TO_CHECK);

        Vector3 mid = { (from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f, (from.z + to.z) * 0.5f };

        int gx = spatial_grid_->world_to_grid(mid.x);
        int gy = spatial_grid_->world_to_grid(mid.y);
        int gz = spatial_grid_->world_to_grid(mid.z);

        std::unordered_set<size_t> added_walls;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    int nx = gx + dx;
                    int ny = gy + dy;
                    int nz = gz + dz;

                    if (nx >= 0 && nx < SpatialGrid::GRID_SIZE &&
                        ny >= 0 && ny < SpatialGrid::GRID_SIZE &&
                        nz >= 0 && nz < SpatialGrid::GRID_SIZE) {

                        for (size_t wall_idx : spatial_grid_->grid[nx][ny][nz]) {
                            if (added_walls.find(wall_idx) == added_walls.end()) {
                                added_walls.insert(wall_idx);
                                nearby_walls.push_back(wall_idx);

                                if (nearby_walls.size() >= MAX_WALLS_TO_CHECK) {
                                    return nearby_walls;
                                }
                            }
                        }
                    }
                }
            }
        }

        return nearby_walls;
    }

    void initialize() {
        if (!g_wall_checker) {
            g_wall_checker = std::make_unique<WallChecker>();
            g_wall_checker->start();
        }
    }

    void shutdown() {
        if (g_wall_checker) {
            g_wall_checker->stop();
            g_wall_checker.reset();
        }
    }

    bool can_see(const Vector3& from, const Vector3& to) {
        if (!g_wall_checker) {
            initialize();
        }
        return g_wall_checker->can_see(from, to);
    }

    std::vector<roblox::player> get_visible_players() {
        if (!g_wall_checker) {
            return {};
        }
        return g_wall_checker->get_visible_players();
    }
}