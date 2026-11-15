#pragma once
#include <Windows.h>
#include <vector>
#include <cmath>
#include <chrono>
#include "../../backend/classes/classes.h"
#include "../../globals.h"
#include "wallcheck/wallcheck.h"
#include <regex>
#include "../overlay/overlay.h"
#define max
#undef max
#define min
#undef min

class functions {
public:
    static inline Vec2 get_mouse_position() {
        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        ScreenToClient(GetForegroundWindow(), &cursor_pos);
        return Vec2(static_cast<float>(cursor_pos.x), static_cast<float>(cursor_pos.y));
    }

    static inline Vec2 get_screen_dimensions() {
        RECT rect;
        GetClientRect(GetForegroundWindow(), &rect);
        return Vec2(static_cast<float>(rect.right), static_cast<float>(rect.bottom));
    }

    static inline roblox::camera get_current_camera() {
        return roblox::camera(globals::camera.address);
    }

    static inline std::vector<roblox::player> get_all_players() {
        return globals::cachedplayers;
    }

    static inline Mat3 lerp_rotation(const Mat3& from, const Mat3& to, float t) {
        Mat3 result;
        for (int i = 0; i < 9; i++) {
            result.data[i] = from.data[i] + (to.data[i] - from.data[i]) * t;
        }
        return result;
    }

    static inline float vec3_distance(const Vec3& a, const Vec3& b) {
        return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2) + pow(a.z - b.z, 2));
    }

    static inline float vec2_distance(const Vec2& a, const Vec2& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return sqrt(dx * dx + dy * dy);
    }

    static inline bool is_on_screen(const Vec2& screen_pos, const Vec2& screen_dims) {
        return screen_pos.x >= 0 && screen_pos.x <= screen_dims.x &&
            screen_pos.y >= 0 && screen_pos.y <= screen_dims.y;
    }

    static inline float lerp_float(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static inline Vec3 lerp_vec3(const Vec3& a, const Vec3& b, float t) {
        return Vec3(
            lerp_float(a.x, b.x, t),
            lerp_float(a.y, b.y, t),
            lerp_float(a.z, b.z, t)
        );
    }

    static inline float normalize_angle(float angle) {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

    static inline float get_3d_distance(const Vec3& a, const Vec3& b) {
        return (a - b).magnitude();
    }

    static inline float get_fov_to_target(const Vec3& camera_pos, const Mat3& camera_rot, const Vec3& target_pos) {
        Vec3 direction = (target_pos - camera_pos).normalize();
        Vec3 forward = Vec3(camera_rot.data[6], camera_rot.data[7], camera_rot.data[8]);
        float dot_product = direction.dot(forward);
        return acos(dot_product) * (180.0f / 3.14159f);
    }

    static inline bool is_valid_target(const roblox::player& player) {
        return player.is_valid() && player.is_alive() && player.head.is_valid();
    }

    static inline float smooth_step(float t) {
        return t * t * (3.0f - 2.0f * t);
    }

    // Follow rate in 1/seconds for a given smoothing amount: higher amount = slower,
    // smoother follow. 0 means "snap".
    static inline float smoothing_rate(float amount) {
        if (amount <= 0.001f) return 0.0f;
        return 300.0f / amount;
    }

    // Exponential approach coefficient for this frame. Using the real frame time
    // keeps the glide the same speed no matter how fast the aim thread loops.
    static inline float smoothing_alpha(float amount, float dt) {
        const float rate = smoothing_rate(amount);
        if (rate <= 0.0f) return 1.0f;
        return 1.0f - expf(-rate * std::clamp(dt, 0.0001f, 0.1f));
    }

    static inline Vec3 matrix_forward(const Mat3& matrix) {
        return Vec3(matrix.data[6], matrix.data[7], matrix.data[8]).normalize();
    }

    // Interpolates the view direction along the shortest arc between the two
    // vectors, so the camera turns the short way through the crosshair instead of
    // spinning around at the 180 degree wrap.
    static inline Vec3 slerp_direction(const Vec3& from, const Vec3& to, float t) {
        const Vec3 a = from.normalize();
        const Vec3 b = to.normalize();
        if (a.magnitude_squared() < 1e-6f) return b;
        if (b.magnitude_squared() < 1e-6f) return a;

        const float dot = std::clamp(a.dot(b), -1.0f, 1.0f);

        // Already pointing there.
        if (dot > 0.999999f) return b;

        const float theta = acosf(dot);

        // Exactly opposite: the great-circle interpolation collapses to a zero
        // vector, so rotate around a perpendicular axis instead.
        if (dot < -0.999999f) {
            Vec3 axis = a.cross(Vec3(0.0f, 1.0f, 0.0f));
            if (axis.magnitude_squared() < 1e-6f) axis = a.cross(Vec3(1.0f, 0.0f, 0.0f));
            axis = axis.normalize();

            const float angle = theta * t;
            const float cos_angle = cosf(angle);
            const float sin_angle = sinf(angle);
            const Vec3 rotated = a * cos_angle + axis.cross(a) * sin_angle +
                axis * (axis.dot(a) * (1.0f - cos_angle));

            return rotated.normalize();
        }

        const float sin_theta = sinf(theta);
        const float weight_a = sinf((1.0f - t) * theta) / sin_theta;
        const float weight_b = sinf(t * theta) / sin_theta;

        return Vec3(a * weight_a + b * weight_b).normalize();
    }

    static inline float ease_in_out(float t) {
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }

    static inline Mat3 direction_to_rotation(const Vec3& direction) {
        Vec3 forward = direction.normalize();
        Vec3 right = Vec3(0, 1, 0).cross(forward).normalize();
        Vec3 up = forward.cross(right).normalize();

        Mat3 rotation;
        rotation.data[0] = right.x; rotation.data[1] = right.y; rotation.data[2] = right.z;
        rotation.data[3] = up.x;    rotation.data[4] = up.y;    rotation.data[5] = up.z;
        rotation.data[6] = forward.x; rotation.data[7] = forward.y; rotation.data[8] = forward.z;

        return rotation;
    }

    static inline Vector3 normaltheaimbot(const Vector3& vec) {
        float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        return (length != 0) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : vec;
    }

    static inline Vector3 crosstheproduct(const Vector3& vec1, const Vector3& vec2) {
        return {
            vec1.y * vec2.z - vec1.z * vec2.y,
            vec1.z * vec2.x - vec1.x * vec2.z,
            vec1.x * vec2.y - vec1.y * vec2.x
        };
    }

    static inline Mat3 look_at_to_matrix(const Vec3& camera_position, const Vec3& target_position) {
        Vector3 forward = normaltheaimbot(Vector3{ (target_position.x - camera_position.x), (target_position.y - camera_position.y), (target_position.z - camera_position.z) });
        Vector3 right = normaltheaimbot(crosstheproduct({ 0, 1, 0 }, forward));
        Vector3 up = crosstheproduct(forward, right);

        Matrix3 look_at_matrix{};
        look_at_matrix.data[0] = -right.x;  look_at_matrix.data[1] = up.x;  look_at_matrix.data[2] = -forward.x;
        look_at_matrix.data[3] = right.y;  look_at_matrix.data[4] = up.y;  look_at_matrix.data[5] = -forward.y;
        look_at_matrix.data[6] = -right.z;  look_at_matrix.data[7] = up.z;  look_at_matrix.data[8] = -forward.z;

        return look_at_matrix;
    }

    static inline void perform_camera_aimbot(const Vec3& target_pos, const Vec3& camera_pos) {
        roblox::camera camera = get_current_camera();
        if (!camera.address) return;

        static auto last_tick = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last_tick).count();
        last_tick = now;

        const Mat3 target_matrix = look_at_to_matrix(camera_pos, target_pos);

        if (!features::aimbot::smoothing || smoothing_rate(features::aimbot::smoothingamount) <= 0.0f) {
            camera.writeRot(target_matrix);
            return;
        }

        const Vec3 from = matrix_forward(camera.getRot());
        const Vec3 to = matrix_forward(target_matrix);

        if (from.magnitude_squared() < 1e-6f || to.magnitude_squared() < 1e-6f) {
            camera.writeRot(target_matrix);
            return;
        }

        // Land exactly once we are on target instead of creeping towards it forever.
        const float remaining = acosf(std::clamp(from.dot(to), -1.0f, 1.0f)) * (180.0f / 3.14159f);
        if (remaining < 0.05f) {
            camera.writeRot(target_matrix);
            return;
        }

        const float t = smoothing_alpha(features::aimbot::smoothingamount, dt);
        const Vec3 aim_direction = slerp_direction(from, to, t);

        if (aim_direction.magnitude_squared() < 1e-6f) {
            camera.writeRot(target_matrix);
            return;
        }

        camera.writeRot(look_at_to_matrix(camera_pos, camera_pos + aim_direction));
    }

    static inline roblox::player get_current_aimbot_target() {
        std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
        return aimbot_cache::current_target;
    }

    static inline roblox::player get_last_aimbot_target() {
        std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
        return aimbot_cache::last_target;
    }

    static inline bool has_aimbot_target() {
        std::lock_guard<std::mutex> lock(aimbot_cache::target_mutex);
        return aimbot_cache::current_target.address != 0;
    }

    static inline void perform_mouse_aimbot(const Vec2& screen_coords) {
        // Re-resolve every call: a handle cached while Roblox was not up yet stays
        // null forever and the mouse aim silently does nothing.
        HWND roblox_window = FindWindowA(nullptr, "Roblox");
        if (!roblox_window || !IsWindow(roblox_window)) return;

        POINT cursor_pos;
        if (!GetCursorPos(&cursor_pos)) return;
        ScreenToClient(roblox_window, &cursor_pos);

        float delta_x = screen_coords.x - static_cast<float>(cursor_pos.x);
        float delta_y = screen_coords.y - static_cast<float>(cursor_pos.y);

        // Leftover sub-pixel movement from the previous step. Without this the
        // fractional part is truncated away every frame and the glide comes out as
        // uneven stuttering steps.
        static float carry_x = 0.0f;
        static float carry_y = 0.0f;

        if (features::aimbot::smoothing) {
            static auto last_tick = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - last_tick).count();
            last_tick = now;

            const float alpha = smoothing_alpha(features::aimbot::smoothingamount, dt);
            delta_x *= alpha;
            delta_y *= alpha;
        }
        else {
            carry_x = 0.0f;
            carry_y = 0.0f;
        }

        float move_x = delta_x + carry_x;
        float move_y = delta_y + carry_y;

        if (!std::isfinite(move_x) || !std::isfinite(move_y)) {
            carry_x = 0.0f;
            carry_y = 0.0f;
            return;
        }

        // Already on target: settle and drop the remainder.
        if (fabsf(move_x) < 0.5f && fabsf(move_y) < 0.5f) {
            carry_x = 0.0f;
            carry_y = 0.0f;
            return;
        }

        const LONG step_x = static_cast<LONG>(move_x);
        const LONG step_y = static_cast<LONG>(move_y);
        carry_x = move_x - static_cast<float>(step_x);
        carry_y = move_y - static_cast<float>(step_y);

        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = step_x;
        input.mi.dy = step_y;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    static inline roblox::instance get_closest_part_to_cursor(const roblox::player& player, const Vec2& cursor_pos) {
        std::vector<std::pair<roblox::instance, std::string>> parts = {
            {player.head, "Head"},
            {player.uppertorso, "UpperTorso"},
            {player.lowertorso, "LowerTorso"},
            {player.hrp, "HumanoidRootPart"}
        };

        roblox::instance closest_part = player.head;
        float closest_distance = 9e18f;

        for (auto& [part, name] : parts) {
            if (part.address == 0) continue;

            Vec2 screen_coords = roblox::worldtoscreen(part.get_pos());
            if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) continue;

            float dx = cursor_pos.x - screen_coords.x;
            float dy = cursor_pos.y - screen_coords.y;
            float distance = dx * dx + dy * dy;

            if (distance < closest_distance) {
                closest_distance = distance;
                closest_part = part;
            }
        }

        return closest_part;
    }

    static inline roblox::instance get_target_part(const roblox::player& player, const Vec2& cursor_pos) {
        switch (features::aimbot::hitpart) {
        case 0: return player.head;
        case 1: return player.hrp;
        case 2: return player.uppertorso;
        case 3: return get_closest_part_to_cursor(player, cursor_pos);
        default: return player.head;
        }
    }

    static inline Vec3 calculate_prediction_enhanced(const Vec3& target_pos, const roblox::instance& target_part) {
        if (!features::aimbot::predictions) return target_pos;

        Vec3 velocity = target_part.get_velocity();
        Vec3 prediction_factor = Vec3(
            features::aimbot::predictionsx,
            features::aimbot::predictionsy,
            features::aimbot::predictionsx
        );

        return Vec3(
            target_pos.x + (velocity.x / prediction_factor.x),
            target_pos.y + (velocity.y / prediction_factor.y),
            target_pos.z + (velocity.z / prediction_factor.z)
        );
    }

    static inline roblox::player get_target_closest_to_mouse_with_fov() {
        Vec2 mouse_pos = get_mouse_position();
        const auto& players = get_all_players();
        if (players.empty()) return {};
        Vec3 camera_pos = get_current_camera().getPos();
        Vec2 screen_dims = get_screen_dimensions();
        roblox::player closest = {};
        float closest_distance_squared = 9e18f;
        const float fov_size = features::aimbot::fieldofviewsize;
        const float fov_size_squared = fov_size * fov_size;
        for (const auto& player : players) {
            if (!is_valid_target(player)) continue;
            if (player.name == globals::lp.name)continue;
            if (features::aimbot::teamcheck && globals::is_teammate(player)) continue;
            roblox::instance target_part = get_target_part(player, mouse_pos);
            if (target_part.address == 0) continue;
            Vec3 part_pos = target_part.get_pos();
            Vec2 screen_coords = roblox::worldtoscreen(part_pos);
            if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) continue;
            if (!is_on_screen(screen_coords, screen_dims)) continue;
            const float dx = mouse_pos.x - screen_coords.x;
            const float dy = mouse_pos.y - screen_coords.y;
            const float distance_squared = dx * dx + dy * dy;
            if (features::aimbot::drawfov && distance_squared > fov_size_squared) continue;
            if (player.health <= 0) continue;
            if (distance_squared < closest_distance_squared) {
                closest_distance_squared = distance_squared;
                closest = player;
            }
        }
        return closest;
    }

    static inline bool passes_silent_target_checks(const roblox::player& player, const Vec3& camera_pos, std::vector<int>* flags) {
        if (!player.head.address) return false;

        if (features::silentaim::teamcheck && globals::is_teammate(player)) return false;

        if ((*flags)[0] && player.health <= 0) return false;

        if ((*flags)[1]) {
            if (!wallcheck::can_see(camera_pos, player.head.get_pos())) return false;
        }

        if ((*flags)[2]) {
            float distance = vec3_distance(camera_pos, player.head.get_pos());
            if (distance > features::silentaim::range) return false;
        }

        if ((*flags)[3]) {
            auto body_effects = player.bodyeffects;
            if (body_effects.address) {
                if (body_effects.findfirstchild("K.O").read_bool_value()) return false;
            }
        }

        if ((*flags)[4]) {
            auto body_effects = player.bodyeffects;
            if (body_effects.address) {
                if (body_effects.findfirstchild("Dead").read_bool_value()) return false;
            }
        }

        if ((*flags)[5]) {
            Vec2 screen_coords = roblox::worldtoscreen(player.head.get_pos());
            if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) return false;
        }

        if ((*flags)[6]) {
            if (player.name == globals::lp.name) return false;
        }

        return true;
    }

    static inline roblox::player get_silent_target_closest_to_mouse(bool use_fov, float fov_size, std::vector<int>* flags) {
        Vec2 mouse_pos = get_mouse_position();
        const auto& players = get_all_players();
        if (players.empty()) return {};

        Vec3 camera_pos = get_current_camera().getPos();
        Vec2 screen_dims = get_screen_dimensions();

        roblox::player closest = {};
        float closest_distance_squared = 9e18f;
        const float fov_size_squared = fov_size * fov_size;

        for (const auto& player : players) {
            if (!passes_silent_target_checks(player, camera_pos, flags)) continue;

            Vec2 screen_coords = roblox::worldtoscreen(player.head.get_pos());
            if (screen_coords.x == -1.0f || screen_coords.y == -1.0f) continue;

            if (!is_on_screen(screen_coords, screen_dims)) continue;

            const float dx = mouse_pos.x - screen_coords.x;
            const float dy = mouse_pos.y - screen_coords.y;
            const float distance_squared = dx * dx + dy * dy;

            if (use_fov && distance_squared > fov_size_squared) continue;

            if (distance_squared < closest_distance_squared) {
                closest_distance_squared = distance_squared;
                closest = player;
            }
        }

        return closest;
    }

    static inline roblox::instance get_silent_part_from_player(const roblox::player& player, const Vec2& cursor_pos) {
        switch (features::silentaim::hitpart) {
        case 0: return player.head;
        case 1: return player.uppertorso;
        case 2: return player.lowertorso;
        case 3: return player.hrp;
        case 4: return get_closest_part_to_cursor(player, cursor_pos);
        default: return player.head;
        }
    }

    static inline Vec3 calculate_silent_prediction(const Vec3& target_pos, const roblox::player& target_part) {
        if (!features::aimbot::predictions) return target_pos;

        Vec3 velocity = target_part.hrp.get_velocity();
        Vec3 prediction_factor = Vec3(
            features::aimbot::predictionsx,
            features::aimbot::predictionsy,
            features::aimbot::predictionsx
        );

        return Vec3(
            target_pos.x + (velocity.x / prediction_factor.x),
            target_pos.y + (velocity.y / prediction_factor.y),
            target_pos.z + (velocity.z / prediction_factor.z)
        );
    }

    static inline bool is_auto_function_active() {
        return false;
    }

    static inline bool should_silent_aim_be_active() {
        return (features::silentaim::enabled && features::silentaim::silentaimkeybind.enabled) || is_auto_function_active();
    }

    static inline std::string fetchIdfromCase(int case_num, int animationType) {
        std::vector<std::vector<std::string>> animations = {
            {"10921076136", "10921082452", "10921078135", "10921077030", "10921072875", "10921079380", "10921070953", "10714068222", "10714340543"},
            {"616163682", "10921355261", "10921351278", "10921350320", "10921344533", "10921352344", "10921343576", "10714068222", "10714340543"},
            {"82598234841035", "122150855457006", "75290611992385", "98600215928904", "102357151005774", "133308483266208", "88763136693023", "10714068222", "10714340543"},
            {"18537384940", "18537392113", "18537380791", "18537371272", "18537389531", "18537367238", "18537363391", "10714068222", "10714340543"},
            {"10921104374", "10921111375", "10921107367", "10921105765", "10921102574", "10921108971", "10921100400", "10714068222", "10714340543"},
            {"10921320299", "10921326949", "10921322186", "10921321317", "10921316709", "10921324408", "10921325443", "10714068222", "10714340543"},
            {"81024476153754", "109168724482748", "116936326516985", "92294537340807", "94970088341563", "134591743181628", "119377220967554", "10714068222", "10714340543"},
            {"10921240218", "10921244891", "10921242013", "10921241244", "10921232093", "10921243048", "10921229866", "10714068222", "10714340543"},
            {"10980888364", "10980888364", "10921062673", "10921061530", "10921055107", "10921063569", "10921053544", "10714068222", "10714340543"},
            {"10921276116", "10921283326", "10921279832", "10921278648", "10921273958", "10921281000", "10921271391", "10714068222", "10714340543"},
            {"10921157929", "10921162768", "10921160088", "10921159222", "10921155867", "10921161002", "10921154678", "10714068222", "10714340543"},
            {"10921148209", "10921152678", "10921149743", "10921148939", "10921145797", "10921150788", "10921143404", "10714068222", "10714340543"},
            {"72301599441680", "92072849924640", "104325245285198", "121152442762481", "76049494037641", "99384245425157", "131326830509784", "10714068222", "10714340543"},
            {"10921135644", "10921140719", "10921137402", "10921136539", "10921133721", "10921138209", "10921132092", "10714068222", "10714340543"},
            {"10921291831", "10921298616", "10921294559", "10921293373", "10921290167", "10921295495", "10921286911", "10714068222", "10714340543"},
            {"10921306285", "10921312010", "10921308158", "10921307241", "10921302207", "10921309319", "10921300839", "10714068222", "10714340543"},
            {"10921250460", "10921255446", "10921252123", "10921251156", "10921248831", "10921253142", "10921247141", "10714068222", "10714340543"},
            {"10921336997", "10921342074", "1083218792", "10921337907", "10921333667", "10921340419", "10921329322", "10714068222", "10714340543"},
            {"10921039308", "10921046031", "10921042494", "10921040576", "10921036806", "10921044000", "10921032124", "10714068222", "10714340543"},
            {"10921121197", "10921127095", "10921123517", "10921122579", "10921118894", "10921125160", "10921116196", "10714068222", "10714340543"},
            {"750783738", "750785693", "750782230", "750780242", "750782770", "750784579", "750779899", "10714068222", "10714340543"},
            {"117333533048078", "110358958299415", "119846112151352", "129773241321032", "74451233229259", "132697394189921", "134630013742019", "10714068222", "10714340543"},
            {"18747070484", "18747074203", "18747069148", "18747062535", "18747063918", "18747073181", "18747060903", "10714068222", "10714340543"},
            {"16738337225", "16738339158", "16738336650", "16738333171", "16738334710", "16738339158", "16738332169", "10714068222", "10714340543"},
            {"10921261968", "10921269718", "10921263860", "10921262864", "10921258489", "10921264784", "10921257536", "10714068222", "10714340543"},
            {"10714068222", "10714068222", "10714068222", "10714068222", "10714068222", "10714068222", "10714068222", "10714068222", "10714068222"},
            {"10714340543", "10714340543", "10714340543", "10714340543", "10714340543", "10714340543", "10714340543", "10714340543", "10714340543"}
        };

  /*      0: run
            1 : walk
            2 : jump
            3 : fall
            4 : idle
            5 : swim
            6 : climb
            7 : dolphin_dive
            8 : floss*/

        std::vector<std::string> animationNames = {
            "cartoon", "zombie", "adidas_comm", "adidas_sport", "elder", "vampire",
            "catwalk_glam", "oldschool", "bubbly", "stylish", "ninja", "mage",
            "wicked", "levitation", "superhero", "toy", "robot", "werewolf",
            "astronaut", "knight", "pirate", "nfl", "no_boundaries", "bold", "rthro", "floss", "dolphin_dive"
        };

        if (case_num < 0 || case_num >= animations.size() || animationType < 0 || animationType >= 9) {
            return "";
        }

        return animations[case_num][animationType];
    }





    static inline std::string extractIdFromUrl(const std::string& url) {
        std::regex id_regex(R"(id=(\d+))");
        std::smatch match;
        if (std::regex_search(url, match, id_regex)) {
            return match[1].str();
        }
        return "";
    }

 static  inline void cacheOriginalAnimations(auto& fall_anim, auto& jump_anim, auto& run_anim, auto& walk_anim, auto& climb_anim, auto& swim_anim, auto& idle_anim) {
       if (!functional::original_anims.cached) {
           functional::original_anims.fall_id = extractIdFromUrl(fall_anim.read_animationid());
           functional::original_anims.jump_id = extractIdFromUrl(jump_anim.read_animationid());
           functional::original_anims.run_id = extractIdFromUrl(run_anim.read_animationid());
           functional::original_anims.walk_id = extractIdFromUrl(walk_anim.read_animationid());
           functional::original_anims.climb_id = extractIdFromUrl(climb_anim.read_animationid());
           functional::original_anims.swim_id = extractIdFromUrl(swim_anim.read_animationid());
           functional::original_anims.idle_id = extractIdFromUrl(idle_anim.read_animationid());


           functional::original_anims.cached = true;
       }
   }

 static inline void restoreOriginalAnimations(auto& fall_anim, auto& jump_anim, auto& run_anim, auto& walk_anim, auto& climb_anim, auto& swim_anim, auto& idle_anim) {
       if (functional::original_anims.cached) {
           fall_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.fall_id);
           jump_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.jump_id);
           run_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.run_id);
           walk_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.walk_id);
           climb_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.climb_id);
           swim_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.swim_id);
           idle_anim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.idle_id);
       }
   }


 inline static bool isRobloxFocused() {
     return GetForegroundWindow() == FindWindowA(0, "Roblox");
 }

 inline static bool shouldCancelOperation() {
     if (!isRobloxFocused()) return true;
     if (globals::menuactive) return true;
     if (!globals::lp.hrp.address) return true;

     auto body_effects = globals::lp.bodyeffects;
     if (body_effects.address) {
         if (body_effects.findfirstchild("K.O").read_bool_value()) return true;
         if (body_effects.findfirstchild("Dead").read_bool_value()) return true;
     }

     auto children = globals::lp.instance_obj.get_children();
     for (roblox::instance& instance : children) {
         if (instance.get_name() == "GRABBING_CONSTRAINT") {
             return true;
         }
     }

     return false;
 }

 inline static bool isTargetValid(roblox::player target) {
     if (!target.address) return false;

     auto body_effects = target.bodyeffects;
     if (body_effects.address) {
         if (body_effects.findfirstchild("Dead").read_bool_value()) return false;
     }

     auto children = target.instance_obj.get_children();
     for (roblox::instance& instance : children) {
         if (instance.get_name() == "GRABBING_CONSTRAINT") {
             return false;
         }
     }

     return true;
 }

 inline static void waitForFocus() {
     while (!isRobloxFocused() || globals::menuactive) {
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
     }
 }

 inline static Vec3 normalize3(const Vec3& vec) {
     float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
     return (length > 0.001f) ? Vec3{ vec.x / length, vec.y / length, vec.z / length } : Vec3{ 0, 0, 0 };
 }

 inline static Vec3 crossProduct3(const Vec3& vec1, const Vec3& vec2) {
     return {
         vec1.y * vec2.z - vec1.z * vec2.y,
         vec1.z * vec2.x - vec1.x * vec2.z,
         vec1.x * vec2.y - vec1.y * vec2.x
     };
 }

 inline static Mat3 LookAtToMatrix3(const Vec3& cameraPosition, const Vec3& targetPosition) {
     Vec3 forward = normalize3(Vec3{
         targetPosition.x - cameraPosition.x,
         targetPosition.y - cameraPosition.y,
         targetPosition.z - cameraPosition.z
         });

     if (forward.x == 0 && forward.y == 0 && forward.z == 0) {
         forward = Vec3{ 0, 0, -1 };
     }

     Vec3 right = normalize3(crossProduct3({ 0, 1, 0 }, forward));
     Vec3 up = crossProduct3(forward, right);

     Mat3 lookAtMatrix{};
     lookAtMatrix.data[0] = -right.x;   lookAtMatrix.data[1] = up.x;   lookAtMatrix.data[2] = -forward.x;
     lookAtMatrix.data[3] = right.y;    lookAtMatrix.data[4] = up.y;   lookAtMatrix.data[5] = -forward.y;
     lookAtMatrix.data[6] = -right.z;   lookAtMatrix.data[7] = up.z;   lookAtMatrix.data[8] = -forward.z;

     return lookAtMatrix;
 }

 inline static bool safeKeyPress(char key, int holdTime = 5) {
     if (shouldCancelOperation()) return false;

     static std::chrono::steady_clock::time_point lastKeyPress = std::chrono::steady_clock::now();
     auto now = std::chrono::steady_clock::now();

     if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyPress).count() < 100) {
         return false;
     }

     if (!isRobloxFocused() || globals::menuactive) return false;

     BYTE vkCode = MapVirtualKey(key, 0);
     keybd_event(0, vkCode, KEYEVENTF_SCANCODE, 0);
     std::this_thread::sleep_for(std::chrono::milliseconds(holdTime));
     keybd_event(0, vkCode, KEYEVENTF_KEYUP, 0);

     lastKeyPress = now;
     return true;
 }

 inline static void forceKeyUp(char key) {
     BYTE vkCode = MapVirtualKey(key, 0);
     keybd_event(0, vkCode, KEYEVENTF_KEYUP, 0);
 }

 inline static void stomp(roblox::player target) {


     auto localinstance = globals::lp.hrp;
     auto savedPos = localinstance.get_pos();

     int maxAttempts = 50;
     int attempts = 0;

     while (attempts < maxAttempts) {
         attempts++;

         if (target.bodyeffects.findfirstchild("Dead").read_bool_value()) break;

         Vec3 stompPos = Vec3(
             target.lowertorso.get_pos().x,
             target.lowertorso.get_pos().y + 2.5f,
             target.lowertorso.get_pos().z
         );

         if (isRobloxFocused() && !globals::menuactive) {
             for (int i = 0; i < 5000; i++) {
                 localinstance.write_position(stompPos);
             }
         }
         Sleep(10);
         if (!safeKeyPress('E', 30)) break;

         if (shouldCancelOperation()) break;
     }

     forceKeyUp('E');

     if (isRobloxFocused() && !globals::menuactive) {
         for (int i = 0; i < 5000; i++) {
         
             localinstance.write_position(savedPos);
         }
     }
 }

 inline static float calculate_distance_2d(Vec2 first, Vec2 sec) {
     return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2));
 }

 inline static float calculate_distance_3d(Vec3 first, Vec3 sec) {
     return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2) + pow(first.z - sec.z, 2));
 }

 struct PartHitbox {
     Vec2 screen_pos;
     Vec3 world_pos;
     Vec3 part_size;
     float screen_radius;
     std::string name;
 };

 inline static roblox::player get_triggerbot_closest_target() {
     roblox::player closest = {};
     float closest_distance = 9e9;

     POINT point;
     GetCursorPos(&point);
     ScreenToClient(FindWindowA(0, "Roblox"), &point);
     Vec2 cursor_pos = { static_cast<float>(point.x), static_cast<float>(point.y) };
     Vec3 camera_pos = functions::get_current_camera().getPos();

     for (auto player : globals::cachedplayers) {
         if (player.head.address == 0) continue;
         if (player.name == globals::localplayer.get_name()) continue;

     //    if (!wallcheck::screen(player)) continue;

         auto body_effects = player.bodyeffects;
         if (body_effects.address) {
             if (body_effects.findfirstchild("Dead").read_bool_value()) continue;
             if (body_effects.findfirstchild("K.O").read_bool_value()) continue;
         }

         Vec2 head_screen = roblox::worldtoscreen(player.head.get_pos());
         if (head_screen.x == -1.0f || head_screen.y == -1.0f) continue;

         float distance_to_cursor = calculate_distance_2d(cursor_pos, head_screen);
         float world_distance = calculate_distance_3d(camera_pos, player.head.get_pos());

         if (features::triggerbot::rangecheck && world_distance > features::triggerbot::range) continue;

         if (features::triggerbot::wallcheck && !wallcheck::can_see(camera_pos, player.head.get_pos())) continue;

         if (distance_to_cursor < closest_distance) {
             closest_distance = distance_to_cursor;
             closest = player;
         }
     }

     return closest;
 }

 inline static bool is_cursor_over_target_part(roblox::player& player, Vec2& cursor_pos, float& world_distance) {
     HWND hwnd = FindWindowA(0, "Roblox");
     RECT window_rect;
     if (!GetWindowRect(hwnd, &window_rect)) return false;

     Vec3 camera_pos = functions::get_current_camera().getPos();
     world_distance = calculate_distance_3d(camera_pos, player.hrp.get_pos());

     std::vector<PartHitbox> part_hitboxes;

     auto add_part_hitbox = [&](roblox::instance& part, const std::string& name) {
         if (!part.address) return;

         Vec3 world_pos = part.get_pos();
         Vec2 screen_pos = roblox::worldtoscreen(world_pos);

         if (screen_pos.x < 0.0f || screen_pos.y < 0.0f) return;
         if (screen_pos.x > (window_rect.right - window_rect.left)) return;
         if (screen_pos.y > (window_rect.bottom - window_rect.top)) return;

         Vec3 part_size = part.get_part_size();
         float distance_factor = std::max(1.0f, world_distance / 50.0f);
         float screen_radius = (part_size.x + part_size.y) * 15.0f / distance_factor;
         screen_radius = std::max(8.0f, std::min(screen_radius, 35.0f));

         part_hitboxes.push_back({
             screen_pos,
             world_pos,
             part_size,
             screen_radius,
             name
             });
         };

     add_part_hitbox(player.head, "Head");
     add_part_hitbox(player.uppertorso, "UpperTorso");
     add_part_hitbox(player.lowertorso, "LowerTorso");
     add_part_hitbox(player.hrp, "HumanoidRootPart");
     add_part_hitbox(player.leftupperarm, "LeftUpperArm");
     add_part_hitbox(player.rightupperarm, "RightUpperArm");
     add_part_hitbox(player.leftlowerarm, "LeftLowerArm");
     add_part_hitbox(player.rightlowerarm, "RightLowerArm");
     add_part_hitbox(player.lefthand, "LeftHand");
     add_part_hitbox(player.righthand, "RightHand");
     add_part_hitbox(player.leftupperleg, "LeftUpperLeg");
     add_part_hitbox(player.rightupperleg, "RightUpperLeg");
     add_part_hitbox(player.leftlowerleg, "LeftLowerLeg");
     add_part_hitbox(player.rightlowerleg, "RightLowerLeg");
     add_part_hitbox(player.leftfoot, "LeftFoot");
     add_part_hitbox(player.rightfoot, "RightFoot");

     for (const auto& hitbox : part_hitboxes) {
         float distance_to_part = calculate_distance_2d(cursor_pos, hitbox.screen_pos);

         if (distance_to_part <= hitbox.screen_radius) {
             return true;
         }
     }

     return false;
 }


};