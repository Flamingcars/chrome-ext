#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <algorithm>
#define M_PI 3.14159265358979323846

#pragma once
#include <cmath>
#include <algorithm>
#include <limits>

#define max
#undef max
#define min
#undef min

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace math {
    constexpr float EPSILON = 1e-6f;
    constexpr float DEG_TO_RAD = M_PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / M_PI;

    inline float clamp(float value, float min, float max) {
        return std::max(min, std::min(max, value));
    }

    inline float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    inline bool approximately_equal(float a, float b, float epsilon = EPSILON) {
        return std::abs(a - b) < epsilon;
    }

    struct Vec2 final {
        float x, y;

        Vec2() noexcept : x(0), y(0) {}
        Vec2(float x, float y) noexcept : x(x), y(y) {}

        Vec2 operator+(const Vec2& other) const noexcept {
            return { x + other.x, y + other.y };
        }

        Vec2 operator-(const Vec2& other) const noexcept {
            return { x - other.x, y - other.y };
        }

        Vec2 operator*(float factor) const noexcept {
            return { x * factor, y * factor };
        }

        Vec2 operator*(const Vec2& other) const noexcept {
            return { x * other.x, y * other.y };
        }

        Vec2 operator/(float factor) const noexcept {
            return { x / factor, y / factor };
        }

        Vec2 operator/(const Vec2& other) const noexcept {
            return { x / other.x, y / other.y };
        }

        Vec2& operator+=(const Vec2& other) noexcept {
            x += other.x;
            y += other.y;
            return *this;
        }

        Vec2& operator-=(const Vec2& other) noexcept {
            x -= other.x;
            y -= other.y;
            return *this;
        }

        Vec2& operator*=(float factor) noexcept {
            x *= factor;
            y *= factor;
            return *this;
        }

        Vec2& operator/=(float factor) noexcept {
            x /= factor;
            y /= factor;
            return *this;
        }

        bool operator==(const Vec2& other) const noexcept {
            return approximately_equal(x, other.x) && approximately_equal(y, other.y);
        }

        bool operator!=(const Vec2& other) const noexcept {
            return !(*this == other);
        }

        float magnitude() const noexcept {
            return std::sqrt(x * x + y * y);
        }

        float magnitude_squared() const noexcept {
            return x * x + y * y;
        }

        Vec2 normalize() const noexcept {
            float mag = magnitude();
            if (mag < EPSILON) return { 0, 0 };
            return { x / mag, y / mag };
        }

        float dot(const Vec2& other) const noexcept {
            return x * other.x + y * other.y;
        }

        float distance(const Vec2& other) const noexcept {
            return (*this - other).magnitude();
        }

        float distance_squared(const Vec2& other) const noexcept {
            return (*this - other).magnitude_squared();
        }

        Vec2 perpendicular() const noexcept {
            return { -y, x };
        }

        static Vec2 lerp(const Vec2& a, const Vec2& b, float t) noexcept {
            return a + (b - a) * t;
        }

        static const Vec2& zero() noexcept {
            static const Vec2 v(0, 0);
            return v;
        }

        static const Vec2& one() noexcept {
            static const Vec2 v(1, 1);
            return v;
        }

        static const Vec2& right() noexcept {
            static const Vec2 v(1, 0);
            return v;
        }

        static const Vec2& up() noexcept {
            static const Vec2 v(0, 1);
            return v;
        }
    };

    struct Vec3 final {
        float x, y, z;

        Vec3() noexcept : x(0), y(0), z(0) {}
        Vec3(float x, float y, float z) noexcept : x(x), y(y), z(z) {}
        Vec3(const Vec2& v, float z) noexcept : x(v.x), y(v.y), z(z) {}

        inline const float& operator[](int i) const noexcept {
            return reinterpret_cast<const float*>(this)[i];
        }

        inline float& operator[](int i) noexcept {
            return reinterpret_cast<float*>(this)[i];
        }

        Vec3 operator+(const Vec3& other) const noexcept {
            return { x + other.x, y + other.y, z + other.z };
        }

        Vec3 operator-(const Vec3& other) const noexcept {
            return { x - other.x, y - other.y, z - other.z };
        }

        Vec3 operator*(float value) const noexcept {
            return { x * value, y * value, z * value };
        }

        Vec3 operator*(const Vec3& other) const noexcept {
            return { x * other.x, y * other.y, z * other.z };
        }

        Vec3 operator/(float value) const noexcept {
            return { x / value, y / value, z / value };
        }

        Vec3 operator/(const Vec3& other) const noexcept {
            return { x / other.x, y / other.y, z / other.z };
        }

        Vec3 operator-() const noexcept {
            return { -x, -y, -z };
        }

        Vec3& operator+=(const Vec3& other) noexcept {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }

        Vec3& operator-=(const Vec3& other) noexcept {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }

        Vec3& operator*=(float scalar) noexcept {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        Vec3& operator/=(float scalar) noexcept {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        }

        Vec3& operator*=(const Vec3& other) noexcept {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            return *this;
        }

        Vec3& operator/=(const Vec3& other) noexcept {
            x /= other.x;
            y /= other.y;
            z /= other.z;
            return *this;
        }

        bool operator==(const Vec3& other) const noexcept {
            return approximately_equal(x, other.x) &&
                approximately_equal(y, other.y) &&
                approximately_equal(z, other.z);
        }

        bool operator!=(const Vec3& other) const noexcept {
            return !(*this == other);
        }

        float magnitude() const noexcept {
            return std::sqrt(x * x + y * y + z * z);
        }

        float magnitude_squared() const noexcept {
            return x * x + y * y + z * z;
        }

        Vec3 normalize() const noexcept {
            float mag = magnitude();
            if (mag < EPSILON) return { 0, 0, 0 };
            return { x / mag, y / mag, z / mag };
        }

        Vec3 direction() const noexcept {
            return normalize();
        }

        float dot(const Vec3& other) const noexcept {
            return x * other.x + y * other.y + z * other.z;
        }

        Vec3 cross(const Vec3& other) const noexcept {
            return {
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            };
        }

        float distance(const Vec3& other) const noexcept {
            return (*this - other).magnitude();
        }

        float distance_squared(const Vec3& other) const noexcept {
            return (*this - other).magnitude_squared();
        }

        Vec3 reflect(const Vec3& normal) const noexcept {
            return *this - normal * (2.0f * dot(normal));
        }

        Vec3 project(const Vec3& onto) const noexcept {
            float onto_mag_sq = onto.magnitude_squared();
            if (onto_mag_sq < EPSILON) return zero();
            return onto * (dot(onto) / onto_mag_sq);
        }

        Vec3 reject(const Vec3& from) const noexcept {
            return *this - project(from);
        }

        static Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
            return a + (b - a) * t;
        }

        static Vec3 slerp(const Vec3& a, const Vec3& b, float t) noexcept {
            float dot_product = clamp(a.normalize().dot(b.normalize()), -1.0f, 1.0f);
            float theta = std::acos(dot_product) * t;
            Vec3 relative_vec = (b - a * dot_product).normalize();
            return a * std::cos(theta) + relative_vec * std::sin(theta);
        }

        static const Vec3& zero() noexcept {
            static const Vec3 v(0, 0, 0);
            return v;
        }

        static const Vec3& one() noexcept {
            static const Vec3 v(1, 1, 1);
            return v;
        }

        static const Vec3& right() noexcept {
            static const Vec3 v(1, 0, 0);
            return v;
        }

        static const Vec3& up() noexcept {
            static const Vec3 v(0, 1, 0);
            return v;
        }

        static const Vec3& forward() noexcept {
            static const Vec3 v(0, 0, 1);
            return v;
        }

        Vec2 xy() const noexcept { return { x, y }; }
        Vec2 xz() const noexcept { return { x, z }; }
        Vec2 yz() const noexcept { return { y, z }; }
    };

    struct Vec4 final {
        float x, y, z, w;

        Vec4() noexcept : x(0), y(0), z(0), w(0) {}
        Vec4(float x, float y, float z, float w) noexcept : x(x), y(y), z(z), w(w) {}
        Vec4(const Vec3& v, float w) noexcept : x(v.x), y(v.y), z(v.z), w(w) {}
        Vec4(const Vec2& v, float z, float w) noexcept : x(v.x), y(v.y), z(z), w(w) {}

        inline const float& operator[](int i) const noexcept {
            return reinterpret_cast<const float*>(this)[i];
        }

        inline float& operator[](int i) noexcept {
            return reinterpret_cast<float*>(this)[i];
        }

        Vec4 operator+(const Vec4& other) const noexcept {
            return { x + other.x, y + other.y, z + other.z, w + other.w };
        }

        Vec4 operator-(const Vec4& other) const noexcept {
            return { x - other.x, y - other.y, z - other.z, w - other.w };
        }

        Vec4 operator*(float scalar) const noexcept {
            return { x * scalar, y * scalar, z * scalar, w * scalar };
        }

        Vec4 operator/(float scalar) const noexcept {
            return { x / scalar, y / scalar, z / scalar, w / scalar };
        }

        bool operator==(const Vec4& other) const noexcept {
            return approximately_equal(x, other.x) &&
                approximately_equal(y, other.y) &&
                approximately_equal(z, other.z) &&
                approximately_equal(w, other.w);
        }

        float magnitude() const noexcept {
            return std::sqrt(x * x + y * y + z * z + w * w);
        }

        Vec4 normalize() const noexcept {
            float mag = magnitude();
            if (mag < EPSILON) return { 0, 0, 0, 0 };
            return { x / mag, y / mag, z / mag, w / mag };
        }

        float dot(const Vec4& other) const noexcept {
            return x * other.x + y * other.y + z * other.z + w * other.w;
        }

        Vec3 xyz() const noexcept { return { x, y, z }; }
        Vec2 xy() const noexcept { return { x, y }; }
    };

    struct Mat3 final {
        float data[9];

        Mat3() noexcept {
            for (int i = 0; i < 9; ++i) data[i] = 0;
        }

        Mat3(float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22) noexcept {
            data[0] = m00; data[1] = m01; data[2] = m02;
            data[3] = m10; data[4] = m11; data[5] = m12;
            data[6] = m20; data[7] = m21; data[8] = m22;
        }

        static Mat3 identity() noexcept {
            return Mat3(1, 0, 0,
                0, 1, 0,
                0, 0, 1);
        }

        Vec3 to_euler_angles() const noexcept {
            Vec3 angles;
            angles.y = std::asin(clamp(data[6], -1.0f, 1.0f));

            if (std::abs(data[6]) < 0.9999f) {
                angles.x = std::atan2(-data[7], data[8]);
                angles.z = std::atan2(-data[3], data[0]);
            }
            else {
                angles.x = 0.0f;
                angles.z = std::atan2(data[1], data[4]);
            }

            return angles * RAD_TO_DEG;
        }

        static Mat3 from_euler_angles(const Vec3& angles) noexcept {
            Vec3 rad_angles = angles * DEG_TO_RAD;
            float pitch = rad_angles.x;
            float yaw = rad_angles.y;
            float roll = rad_angles.z;

            float cosPitch = std::cos(pitch), sinPitch = std::sin(pitch);
            float cosYaw = std::cos(yaw), sinYaw = std::sin(yaw);
            float cosRoll = std::cos(roll), sinRoll = std::sin(roll);

            return Mat3(
                cosYaw * cosRoll,
                cosYaw * sinRoll,
                -sinYaw,
                sinPitch * sinYaw * cosRoll - cosPitch * sinRoll,
                sinPitch * sinYaw * sinRoll + cosPitch * cosRoll,
                sinPitch * cosYaw,
                cosPitch * sinYaw * cosRoll + sinPitch * sinRoll,
                cosPitch * sinYaw * sinRoll - sinPitch * cosRoll,
                cosPitch * cosYaw
            );
        }

        Vec3 get_forward() const noexcept { return { data[2], data[5], data[8] }; }
        Vec3 get_right() const noexcept { return { data[0], data[3], data[6] }; }
        Vec3 get_up() const noexcept { return { data[1], data[4], data[7] }; }

        Mat3 transpose() const noexcept {
            Mat3 result;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    result.data[i + j * 3] = data[j + i * 3];
            return result;
        }

        Vec3 multiply_vector(const Vec3& vec) const noexcept {
            return {
                data[0] * vec.x + data[1] * vec.y + data[2] * vec.z,
                data[3] * vec.x + data[4] * vec.y + data[5] * vec.z,
                data[6] * vec.x + data[7] * vec.y + data[8] * vec.z
            };
        }

        Mat3 operator*(const Mat3& other) const noexcept {
            Mat3 result;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    result.data[i * 3 + j] = 0;
                    for (int k = 0; k < 3; ++k) {
                        result.data[i * 3 + j] += data[i * 3 + k] * other.data[k * 3 + j];
                    }
                }
            }
            return result;
        }

        Mat3 operator+(const Mat3& other) const noexcept {
            Mat3 result;
            for (int i = 0; i < 9; ++i)
                result.data[i] = data[i] + other.data[i];
            return result;
        }

        Mat3 operator-(const Mat3& other) const noexcept {
            Mat3 result;
            for (int i = 0; i < 9; ++i)
                result.data[i] = data[i] - other.data[i];
            return result;
        }

        Mat3 operator*(float scalar) const noexcept {
            Mat3 result;
            for (int i = 0; i < 9; ++i)
                result.data[i] = data[i] * scalar;
            return result;
        }

        Vec3 get_column(int index) const noexcept {
            return { data[index], data[index + 3], data[index + 6] };
        }

        Vec3 get_row(int index) const noexcept {
            int base = index * 3;
            return { data[base], data[base + 1], data[base + 2] };
        }
    };

    struct Mat4 final {
        float data[16];

        Mat4() noexcept {
            for (int i = 0; i < 16; ++i) data[i] = 0;
        }

        static Mat4 identity() noexcept {
            Mat4 result;
            result.data[0] = result.data[5] = result.data[10] = result.data[15] = 1.0f;
            return result;
        }

        static Mat4 translation(const Vec3& t) noexcept {
            Mat4 result = identity();
            result.data[12] = t.x;
            result.data[13] = t.y;
            result.data[14] = t.z;
            return result;
        }

        static Mat4 scale(const Vec3& s) noexcept {
            Mat4 result = identity();
            result.data[0] = s.x;
            result.data[5] = s.y;
            result.data[10] = s.z;
            return result;
        }

        Vec4 multiply_vector(const Vec4& vec) const noexcept {
            return {
                data[0] * vec.x + data[4] * vec.y + data[8] * vec.z + data[12] * vec.w,
                data[1] * vec.x + data[5] * vec.y + data[9] * vec.z + data[13] * vec.w,
                data[2] * vec.x + data[6] * vec.y + data[10] * vec.z + data[14] * vec.w,
                data[3] * vec.x + data[7] * vec.y + data[11] * vec.z + data[15] * vec.w
            };
        }

        Mat4 operator*(const Mat4& other) const noexcept {
            Mat4 result;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    result.data[i * 4 + j] = 0;
                    for (int k = 0; k < 4; ++k) {
                        result.data[i * 4 + j] += data[i * 4 + k] * other.data[k * 4 + j];
                    }
                }
            }
            return result;
        }
    };

    struct CFrame {
        Mat3 rotation_matrix;
        Vec3 position;

        CFrame() noexcept : rotation_matrix(Mat3::identity()), position(Vec3::zero()) {}
        CFrame(const Vec3& pos) noexcept : rotation_matrix(Mat3::identity()), position(pos) {}
        CFrame(const Vec3& pos, const Mat3& rot) noexcept : rotation_matrix(rot), position(pos) {}

        static CFrame lerp(const CFrame& start, const CFrame& end, float alpha) noexcept {
            Vec3 new_position = Vec3::lerp(start.position, end.position, alpha);
            return CFrame(new_position, start.rotation_matrix);
        }

        static CFrame from_vector3(const Vec3& vec) noexcept {
            return CFrame(vec);
        }

        static CFrame look_at(const Vec3& from, const Vec3& to, const Vec3& up = Vec3::up()) noexcept {
            Vec3 forward = (to - from).normalize();
            Vec3 right = forward.cross(up).normalize();
            Vec3 new_up = right.cross(forward);

            Mat3 rotation(
                right.x, new_up.x, -forward.x,
                right.y, new_up.y, -forward.y,
                right.z, new_up.z, -forward.z
            );

            return CFrame(from, rotation);
        }

        Vec3 transform_point(const Vec3& point) const noexcept {
            return rotation_matrix.multiply_vector(point) + position;
        }

        Vec3 transform_direction(const Vec3& direction) const noexcept {
            return rotation_matrix.multiply_vector(direction);
        }

        CFrame inverse() const noexcept {
            Mat3 inv_rotation = rotation_matrix.transpose();
            Vec3 inv_position = inv_rotation.multiply_vector(-position);
            return CFrame(inv_position, inv_rotation);
        }

        Vec3 get_forward() const noexcept { return rotation_matrix.get_forward(); }
        Vec3 get_right() const noexcept { return rotation_matrix.get_right(); }
        Vec3 get_up() const noexcept { return rotation_matrix.get_up(); }
    };

    using Vector2 = Vec2;
    using Vector3 = Vec3;
    using Vector4 = Vec4;
    using Matrix3 = Mat3;
    using Matrix4 = Mat4;
}

using namespace math;

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace roblox {

    enum class material
    {
        Plastic = 256,
        SmoothPlastic = 272,
        Neon = 288,
        Wood = 512,
        WoodPlanks = 528,
        Marble = 784,
        Basalt = 788,
        Slate = 800,
        CrackedLava = 804,
        Concrete = 816,
        Limestone = 820,
        Granite = 832,
        Pavement = 836,
        Brick = 848,
        Pebble = 864,
        Cobblestone = 880,
        Rock = 896,
        Sandstone = 912,
        CorrodedMetal = 1040,
        DiamondPlate = 1056,
        Foil = 1072,
        Metal = 1088,
        Grass = 1280,
        LeafyGrass = 1284,
        Sand = 1296,
        Fabric = 1312,
        Snow = 1328,
        Mud = 1344,
        Ground = 1360,
        Asphalt = 1376,
        Salt = 1392,
        Ice = 1536,
        Glacier = 1552,
        Glass = 1568,
        ForceField = 1584,
        Air = 1792,
        Water = 2048,
        Cardboard = 2304,
        Carpet = 2305,
        CeramicTiles = 2306,
        ClayRoofTiles = 2307,
        RoofShingles = 2308,
        Leather = 2309,
        Plaster = 2310,
        Rubber = 2311
    };
    enum class camera_type : int
    {
        //
        Fixed = 0,
        Attach = 1,
        Watch = 2,
        Track = 3,
        Follow = 4,
        Custom = 5,
        Scriptable = 6,
        Orbital = 7
    };
    enum state : int
    {
        FALLING_DWN = 0,
        RAGDOLL = 1,
        GETTING_UP = 2,
        JUMPING = 3,
        SWIMMING = 4,
        FREE_FALL = 5,
        FLYING = 6,
        LANDED = 7,
        RUNNING = 8,
        RUNNING_SLAVE = 9,
        RUNNING_NO_PHYS = 10,
        STRAFING_NO_PHYS = 11,
        CLIMBING = 12,
        SEATED = 13,
        PLATFORM_STANDING = 14,
        DEAD = 15,
        PHYSICS = 16,
        NUM_STATE_TYPES = 17,
        xx = 18,
    };

    using namespace math;

  //  Vec2 worldtoscreen(const Vec3& world);
    uint32_t Vector3ToIntBGR(const Vec3& color);

    Vec2 worldtoscreen(Vec3 world);

    class instance {
    public:
        uintptr_t address = 0;
        static std::uint64_t cached_input_object;

        instance() = default;
        instance(uintptr_t addr) : address(addr) {}

        bool operator==(const instance& other) const noexcept {
            return this->address == other.address;
        }

        bool operator!=(const instance& other) const noexcept {
            return this->address != other.address;
        }

        bool is_valid() const noexcept {
            return address != 0;
        }

        // Core methods
        uintptr_t primitive();
        uintptr_t primitive() const;

        // Dimensions and view matrix
        Vec2 GetDimensins();
        Vec2 GetDimensins() const;
        Mat4 GetViewMatrix();
        Mat4 GetViewMatrix() const;
        Vec2 get_dimensions();
        Vec2 get_dimensions() const;

        // Name and identification methods
        std::string get_name();
        std::string get_frame_name();
        void write_name(std::string string);
        std::string get_name() const;
        std::string get_class_name();
        std::string get_class_name() const;
        std::string get_displayname();
        std::string get_displayname() const;
        std::string get_humdisplayname();
        std::string get_humdisplayname() const;

        // Health methods
        float read_health();
        float read_health() const;
        void write_health(float health);
        void write_transparency(float health);
        float read_maxhealth();
        float read_maxhealth() const;

        // Position and movement
        Vec3 get_pos();
        Vec3 get_pos() const;
        Vec3 get_velocity();
        Vec3 get_velocity() const;
        void write_position(Vec3 arg);
        void write_velocity(Vec3 arg);
        Vec3 get_move_dir();
        Vec3 get_move_dir() const;
        void write_move_dir(Vec3 size);

        // CFrame methods
        CFrame read_cframe();
        CFrame read_cframe() const;
        Mat3 read_part_cframe();
        Mat3 read_part_cframe() const;
        void write_cframe(CFrame cframe);
        void write_part_cframe(Mat3 ma3);

        // Size methods
        Vec3 get_part_size();
        Vec3 get_part_size() const;
        void write_part_size(Vec3 size);

        // Camera methods
        Vec3 get_cam_pos();
        Vec3 get_cam_pos() const;
        Mat3 get_cam_rot();
        Mat3 get_cam_rot() const;
        void write_cam_pos(Vec3 pos);
        void write_cam_rot(Mat3 rot);

        // Color and visual methods
        int read_color();
        int read_color() const;
        void write_color(int col);
        void write_ambiance(Vec3 col);
        Vec3 read_ambiance();
        void write_fogcolor(Vec3 col);
        void write_fogstart(float distance);
        void write_fogend(float distance);

        // Collision methods
        void write_cancollide(bool boolleanz);
        bool get_cancollide();
        bool get_cancollide() const;
        void setAnchored(bool booz);

        // Animation methods
        std::string read_animationid();
        std::string read_animationid() const;
        void write_animation(const std::string& s);
        void write_animationid(std::string animid);

        void write_textureid(std::string animid);

        std::uint64_t read_gameid();

        void write_meshid(std::string animid);

        // Humanoid properties
        float read_walkspeed();
        float read_walkspeed() const;
        void write_walkspeed(float value);
        float read_jumppower();
        float read_jumppower() const;
        void write_jumppower(float value);
        float read_hipheight();
        float read_hipheight() const;
        void write_hipheight(float value);
        bool read_sitting();
        bool read_sitting() const;
        void write_sitting(bool value);

        // Gravity methods
        void writeprimgrav(float gravity);
        //float read_primgrav();
        //float read_primgrav() const;

        // Value methods
        Vec3 read_vector3_value();
        Vec3 read_vector3_value() const;
        void write_vector3_value(Vec3 val);
        int read_int_value();
        int read_int_value() const;
        void write_int_value(int val);
        float read_float_value();
        float read_float_value() const;
        void write_float_value(float val);
        double read_double_value();
        double read_double_value() const;
        void write_double_value(double val);
        bool read_bool_value();
        bool read_bool_value() const;
        void write_bool_value(bool val);

        // Navigation methods
        std::vector<instance> get_children();
        std::vector<instance> get_children() const;
        instance findfirstchild(std::string arg);
        instance findfirstchild(std::string arg) const;
        instance read_service(std::string arg);
        instance read_service(std::string arg) const;
        instance read_parent();
        instance read_parent() const;
        instance model_instance();
        instance model_instance() const;
        instance local_player();
        instance local_player() const;
        instance read_workspace();
        instance read_workspace() const;
        instance read_userid();
        instance read_userid() const;


        void setFramePosX(uint64_t position);
        void setFramePosY(uint64_t position);

 
        void initialize_mouse_service(std::uint64_t address);
        void write_mouse_position(std::uint64_t address, float x, float y);


        instance readAdornee();

        void spectate(uint64_t stringhere);
        void unspectate();

        void write_material_part(roblox::instance dynamic, roblox::material material);

        bool has_service(std::string serviceName);

        void write_material_tool(roblox::instance player, roblox::material material, int color);

        roblox::instance write_all_materials(roblox::instance dynamic, roblox::material material, int color);

      instance GetAttribute(std::string AttributeNameToSearchFor);
};

    class wall {
    public:
        Vec3 size = Vec3::zero();
        CFrame cframe;
        Vec3 position = Vec3::zero();
        Mat3 rotation = Mat3::identity();

        wall() = default;
        wall(const Vec3& pos, const Vec3& sz) : position(pos), size(sz) {}
        wall(const CFrame& cf, const Vec3& sz) : cframe(cf), size(sz) {}

        bool is_valid() const noexcept {
            return size.magnitude() > 0;
        }

        Vec3 get_center() const noexcept {
            return position + size * 0.5f;
        }

        bool contains_point(const Vec3& point) const noexcept {
            Vec3 min = position;
            Vec3 max = position + size;
            return point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y &&
                point.z >= min.z && point.z <= max.z;
        }
    };





    class player {
    public:
        uintptr_t address = 0;

        player() = default;
        player(uintptr_t addr) : address(addr) {}

        bool operator==(const player& other) const noexcept {
            return this->address == other.address;
        }

        bool operator!=(const player& other) const noexcept {
            return this->address != other.address;
        }

        bool is_valid() const noexcept {
            return address != 0 && main.is_valid();
        }

        int rigtype = 0;
        std::string name;
        std::string displayname;
        std::string toolname;
        std::string flag;
        int maxhealth = 100;
        int health = 100;
        int armor = 0;
        int status = 0;
        bool pictureDownloaded = false;

        instance playerinstance;
        instance main;
        instance humanoid;
        instance team;
        instance instance_obj;
        instance tool;
        instance userid;
        instance bodyeffects;

        instance torso;
        instance head;
        instance hrp;
        instance uppertorso;
        instance lowertorso;
        instance rightupperarm;
        instance leftupperarm;
        instance rightlowerarm;
        instance leftlowerarm;
        instance lefthand;
        instance righthand;
        instance leftupperleg;
        instance rightupperleg;
        instance rightlowerleg;
        instance leftlowerleg;
        instance rightfoot;
        instance leftfoot;
        bool canbeseen;
        std::vector<instance> bones;

        bool is_r15() const noexcept { return rigtype == 2; }
        bool is_r6() const noexcept { return rigtype == 1; }
        bool is_alive() const noexcept { return status == 1 && health > 0; }
        bool is_dead() const noexcept { return status == 0 || health <= 0; }

        Vec3 get_position() const {
            if (hrp.is_valid()) {
                return hrp.get_pos();
            }
            if (torso.is_valid()) {
                return torso.get_pos();
            }
            return Vec3::zero();
        }

        Vec3 get_head_position() const {
            if (head.is_valid()) {
                return head.get_pos();
            }
            return Vec3::zero();
        }

        float get_distance_to(const player& other) const {
            return get_position().distance(other.get_position());
        }

        float get_distance_to(const Vec3& position) const {
            return get_position().distance(position);
        }

        void update_health() {
            if (humanoid.is_valid()) {
                health = static_cast<int>(humanoid.read_health());
                maxhealth = static_cast<int>(humanoid.read_maxhealth());
            }
        }
    };

    class camera {
    public:
        uintptr_t address = 0;

        camera() = default;
        camera(uintptr_t addr) : address(addr) {}

        bool operator==(const camera& other) const noexcept {
            return this->address == other.address;
        }

        bool operator!=(const camera& other) const noexcept {
            return this->address != other.address;
        }

        bool is_valid() const noexcept {
            return address != 0;
        }

        // Position and rotation - both const and non-const
        Vec3 getPos();
        Vec3 getPos() const;
        Mat3 getRot();
        Mat3 getRot() const;
        void writePos(Vec3 arg);
        void writeRot(Mat3 arg);

        // Camera type
        int getType();
        int getType() const;
        void setType(int type);

        // Camera mode
        int getMode();
        int getMode() const;
        void setMode(int type);

        // Subject
        instance getSubject();
        instance getSubject() const;

        // Utility methods
        CFrame get_cframe() const {
            return CFrame(getPos(), getRot());
        }

        void set_cframe(const CFrame& cframe) {
            writePos(cframe.position);
            writeRot(cframe.rotation_matrix);
        }

        Vec3 get_look_direction() const {
            return getRot().get_forward();
        }
    };

    // Global functions
   
    struct ESPPlayerData {
        uintptr_t player_address;
        Vector3 position;
        Vector3 head_position;
        Vector2 screen_position;
        Vector2 top_left;
        Vector2 bottom_right;
        float distance;
        float health;
        float maxhealth;
        float margin;
        bool valid;
        bool on_screen;
        bool teammate;
        std::string name;
        std::string tool_name;
        roblox::player player_ref;

        ESPPlayerData() : player_address(0), distance(0.0f), health(0.0f), margin(0.0f), valid(false), on_screen(false), teammate(false) {}
    };

    uint32_t Vector3ToIntBGR(Vec3 color);

    // Input handling
    void cached_input_objectzz();

    // Reason the loader could not start, printed in the console by main().
    extern std::string g_launch_error;

    // System functions
    bool startup();
    bool shutdown();

    // Utility functions
    inline Vec3 color_from_rgb(int r, int g, int b) {
        return Vec3(r / 255.0f, g / 255.0f, b / 255.0f);
    }

    inline Vec3 color_from_hex(uint32_t hex) {
        int r = (hex >> 16) & 0xFF;
        int g = (hex >> 8) & 0xFF;
        int b = hex & 0xFF;
        return color_from_rgb(r, g, b);
    }

    inline uint32_t color_to_hex(const Vec3& color) {
        int r = static_cast<int>(clamp(color.x * 255.0f, 0.0f, 255.0f));
        int g = static_cast<int>(clamp(color.y * 255.0f, 0.0f, 255.0f));
        int b = static_cast<int>(clamp(color.z * 255.0f, 0.0f, 255.0f));
        return (r << 16) | (g << 8) | b;
    }
}

namespace player_access {
    Vector3 get_cached_bone_position(uintptr_t player_address, size_t bone_index);
    bool is_cached_bone_valid(uintptr_t player_address, size_t bone_index);
}

