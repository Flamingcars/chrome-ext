#include "../classes.h"
#include "../../../includes.h"
#include "../../usermode/usermode.h"
#include "../../../globals.h"


#include "../classes.h"
#include <thread>
#include <chrono>

static void write_string(std::uint64_t address, const std::string& value) {
    const auto currentStringLength = read<int64_t>(address + 0x18);
    std::uint64_t stringAddress = currentStringLength >= 16u ? read<std::uint64_t>(address) : address;

    for (size_t i = 0; i < value.size() && i < 200; i++) {
        write<char>(stringAddress + i, value[i]);
    }
    write<char>(stringAddress + value.size(), '\0');
    write<int64_t>(address + 0x10, static_cast<int64_t>(value.size()));
}

static void WriteString(std::uint64_t address, const std::string& value) {
    const auto currentStringLength = read<int64_t>(address + 0x18);
    std::uint64_t stringAddress = currentStringLength >= 16u ? read<std::uint64_t>(address) : address;
    for (size_t i = 0; i < value.size() && i < 200; i++) {
        write<char>(stringAddress + i, value[i]);
    }
    write<char>(stringAddress + value.size(), '\0');
    write<int64_t>(address + 0x10, static_cast<int64_t>(value.size()));
}

struct stringed { char data[200]; };

std::string read_string(std::uint64_t address) {
    auto character = read<stringed>(address);
    return character.data;
}

std::string ReadString(std::uint64_t address) {
    const auto stringLength = read<int64_t>(address + 0x10);
    std::uint64_t stringAddress = stringLength >= 16 ? read<std::uint64_t>(address) : address;

    std::string result;
    result.reserve(stringLength);

    for (int64_t i = 0; i < stringLength; i++) {
        char c = read<char>(stringAddress + i);
        if (c == '\0') break;
        result += c;
    }
    return result;
}

std::string fetchstring(std::uint64_t address) {
    int length = read<int>(address + 0x18);
    if (length >= 16u) {
        std::uintptr_t padding = read<std::uintptr_t>(address);
        return read_string(padding);
    }
    return read_string(address);
}


roblox::instance roblox::instance::GetAttribute(std::string AttributeNameToSearchFor)
{
    //uintptr_t OnDemandInstance = read<uintptr_t>(this->address + offsets::OnDemandInstance);
    //uintptr_t AttributeList = read<uintptr_t>(OnDemandInstance + offsets::AttributeList);
    //for (int i = 0x0; i < 0x400; i = i + offsets::AttributeToNext) {
    //    std::string AttributeName = ReadString(AttributeList + i);
    //    roblox::instance AttributeValue = read<roblox::instance>(AttributeList + i + offsets::AttributeToValue);
    //    if (AttributeName.empty()) break;
    //    std::cout << AttributeName << std::endl;
    //    if (AttributeName == AttributeNameToSearchFor) {
    //        return AttributeValue;
    //    }
    //}
    return roblox::instance();
}

std::uint64_t get_current_input_object(std::uint64_t base_address) {
    return read<std::uint64_t>(base_address + offsets::InputObject);
}

std::uint64_t roblox::instance::cached_input_object = 0;

math::Vec2 roblox::instance::GetDimensins() {
    return read<math::Vec2>(this->address + offsets::Dimensions);
}

math::Vec2 roblox::instance::GetDimensins() const {
    return read<math::Vec2>(this->address + offsets::Dimensions);
}

math::Mat4 roblox::instance::GetViewMatrix() {
    return read<math::Mat4>(this->address + offsets::viewmatrix);
}

math::Mat4 roblox::instance::GetViewMatrix() const {
    return read<math::Mat4>(this->address + offsets::viewmatrix);
}

uintptr_t roblox::instance::primitive() {
    if (!is_valid_address(address)) return 0;
    return read<uintptr_t>(this->address + offsets::Primitive);
}

uintptr_t roblox::instance::primitive() const {
    if (!is_valid_address(address)) return 0;
    return read<uintptr_t>(this->address + offsets::Primitive);
}

std::string roblox::instance::get_name() {
    return fetchstring(read<uintptr_t>(this->address + offsets::Name));
}
std::string roblox::instance::get_frame_name() {
    return ReadString(read<uintptr_t>(this->address + 0xc10));
}


void roblox::instance::write_name(std::string string) {
     write_string(this->address + offsets::Name, string);
}

std::string roblox::instance::get_name() const {
    return fetchstring(read<uintptr_t>(this->address + offsets::Name));
}

std::string roblox::instance::get_class_name() {
    return fetchstring(read<uintptr_t>(read<uintptr_t>(this->address + offsets::ClassDescriptor) + offsets::ChildrenEnd));
}

std::string roblox::instance::get_class_name() const {
    return fetchstring(read<uintptr_t>(read<uintptr_t>(this->address + offsets::ClassDescriptor) + offsets::ChildrenEnd));
}

std::string roblox::instance::get_displayname() {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::DisplayName);
    if (name_pointer) {
        std::string ptr_result = fetchstring(name_pointer);
        if (ptr_result != "") return ptr_result;
        return fetchstring(this->address + offsets::DisplayName);
    }
    return "Unable to read displayname";
}

std::string roblox::instance::get_displayname() const {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::DisplayName);
    if (name_pointer) {
        std::string ptr_result = fetchstring(name_pointer);
        if (ptr_result != "") return ptr_result;
        return fetchstring(this->address + offsets::DisplayName);
    }
    return "Unable to read displayname";
}

std::string roblox::instance::get_humdisplayname() {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::HumanoidDisplayName);
    if (name_pointer) {
        std::string ptr_result = fetchstring(name_pointer);
        if (ptr_result != "") return ptr_result;
        return fetchstring(this->address + offsets::HumanoidDisplayName);
    }
    return "Unable to read displayname";
}

std::string roblox::instance::get_humdisplayname() const {
    const std::uint64_t name_pointer = read<std::uint64_t>(this->address + offsets::HumanoidDisplayName);
    if (name_pointer) {
        std::string ptr_result = fetchstring(name_pointer);
        if (ptr_result != "") return ptr_result;
        return fetchstring(this->address + offsets::HumanoidDisplayName);
    }
    return "Unable to read displayname";
}

union convertion
{
    std::uint64_t hex;
    float f;
} conv;

float roblox::instance::read_health() {
    return read<float>(this->address + offsets::Health);
}

float roblox::instance::read_health() const {
    return read<float>(this->address + offsets::Health);
}

void roblox::instance::write_health(float health) {
    write<float>(this->address + offsets::Health, health);
}

void roblox::instance::write_transparency(float health) {
    write<float>(this->address + offsets::Transparency, health);
}

float roblox::instance::read_maxhealth() {
    return read<float>(this->address + offsets::MaxHealth);
}

float roblox::instance::read_maxhealth() const {
    return read<float>(this->address + offsets::MaxHealth);
}

Vec3 roblox::instance::get_pos() {
    return read<Vec3>(this->primitive() + offsets::Position);
}

Vec3 roblox::instance::get_pos() const {
    uintptr_t prim = read<uintptr_t>(this->address + offsets::Primitive);
    if (!is_valid_address(prim)) return {};
    return read<Vec3>(prim + offsets::Position);
}

Vec3 roblox::instance::get_velocity() {
    if (!is_valid_address(address)) return {};
    return read<Vec3>(this->primitive() + offsets::Velocity);
}

Vec3 roblox::instance::get_velocity() const {
    if (!is_valid_address(address)) return {};
    uintptr_t prim = read<uintptr_t>(this->address + offsets::Primitive);
    if (!is_valid_address(prim)) return {};
    return read<Vec3>(prim + offsets::Velocity);
}

void roblox::instance::write_position(Vec3 arg) {
    write_fast<Vec3>(this->primitive() + offsets::Position, arg);
}

void roblox::instance::write_velocity(Vec3 arg) {
    write_fast<Vec3>(this->primitive() + offsets::Velocity, arg);
}

Vec3 roblox::instance::get_move_dir() {
    return read<Vec3>(this->address + offsets::MoveDirection);
}

Vec3 roblox::instance::get_move_dir() const {
    return read<Vec3>(this->address + offsets::MoveDirection);
}

void roblox::instance::write_move_dir(Vec3 size) {
    write<Vec3>(this->address + offsets::MoveDirection, size);
}

CFrame roblox::instance::read_cframe() {
    return read<CFrame>(this->primitive() + offsets::CFrame);
}

CFrame roblox::instance::read_cframe() const {
    return read<CFrame>(this->primitive() + offsets::CFrame);
}

Mat3 roblox::instance::read_part_cframe() {
    return read<Mat3>(this->primitive() + offsets::CFrame);
}

Mat3 roblox::instance::read_part_cframe() const {
    return read<Mat3>(this->primitive() + offsets::CFrame);
}

void roblox::instance::write_cframe(CFrame cframe) {
    write<CFrame>(this->primitive() + offsets::CFrame, cframe);
}

void roblox::instance::write_part_cframe(Mat3 ma3) {
    write<Mat3>(this->primitive() + offsets::CFrame, ma3);
}

Vec3 roblox::instance::get_part_size() {
    return read<Vec3>(this->primitive() + offsets::PartSize);
}

Vec3 roblox::instance::get_part_size() const {

    return read<Vec3>(this->primitive() + offsets::PartSize);
}

void roblox::instance::write_part_size(Vec3 size) {
    write<Vec3>(this->primitive() + offsets::PartSize, size);
}

Vec3 roblox::instance::get_cam_pos() {
    return read<Vec3>(this->address + offsets::CameraPos);
}

Vec3 roblox::instance::get_cam_pos() const {
    return read<Vec3>(this->address + offsets::CameraPos);
}

Mat3 roblox::instance::get_cam_rot() {
    return read<Mat3>(this->address + offsets::CameraRotation);
}

Mat3 roblox::instance::get_cam_rot() const {
    return read<Mat3>(this->address + offsets::CameraRotation);
}

void roblox::instance::write_cam_pos(Vec3 pos) {
    write<Vec3>(this->address + offsets::CameraPos, pos);
}

void roblox::instance::write_cam_rot(Mat3 rot) {
    write<Mat3>(this->address + offsets::CameraRotation, rot);
}

int roblox::instance::read_color() {
    return read<int>(this->primitive() + offsets::MeshPartColor3);
}

int roblox::instance::read_color() const {
    uintptr_t prim = read<uintptr_t>(this->address + offsets::Primitive);
    if (!is_valid_address(prim)) return 0;
    return read<int>(prim + offsets::MeshPartColor3);
}

void roblox::instance::write_color(int col) {
    write<int>(this->primitive() + offsets::MeshPartColor3, col);
}

void roblox::instance::write_ambiance(Vec3 col) {
    write<Vec3>(this->address + offsets::OutdoorAmbient, col);
}
Vec3 roblox::instance::read_ambiance() {
   return read<Vec3>(this->address + offsets::OutdoorAmbient);
}

void roblox::instance::write_fogcolor(Vec3 col) {
    write<Vec3>(this->address + offsets::FogColor, col);
}

void roblox::instance::write_fogstart(float distance) {
    write<float>(this->address + offsets::FogStart, distance);
}

void roblox::instance::write_fogend(float distance) {
    write<float>(this->address + offsets::FogEnd, distance);
}

void roblox::instance::write_cancollide(bool boolleanz) {
    write<bool>(this->address + offsets::CanCollide, boolleanz);
}

bool roblox::instance::get_cancollide() {
    return read<bool>(this->address + offsets::CanCollide);
}

bool roblox::instance::get_cancollide() const {
    return read<bool>(this->address + offsets::CanCollide);
}

void roblox::instance::setAnchored(bool booz) {
    write<bool>(this->primitive() + offsets::Anchored, booz);
}

std::string roblox::instance::read_animationid() {
    return ReadString(this->address + offsets::AnimationId);
}

std::string roblox::instance::read_animationid() const {
    return ReadString(this->address + offsets::AnimationId);
}

void roblox::instance::write_animation(const std::string& s) {
    write_string(this->address + offsets::AnimationId, s);
}

void roblox::instance::write_animationid(std::string animid) {
    WriteString(this->address + offsets::AnimationId, animid);
}

void roblox::instance::write_textureid(std::string animid) {
    WriteString(this->address + offsets::MeshPartTexture, animid);
}
std::uint64_t roblox::instance::read_gameid() {
    return read<std::uint64_t>(this->address + offsets::GameId);
}
void roblox::instance::write_meshid(std::string animid) {
    WriteString(this->address + offsets::MeshPartMeshId, animid);
}
float roblox::instance::read_walkspeed() {
    if (!is_valid_address(address)) return 0;
    return read<float>(this->address + offsets::WalkSpeed);
}

float roblox::instance::read_walkspeed() const {
    if (!is_valid_address(address)) return 0;
    return read<float>(this->address + offsets::WalkSpeed);
}

void roblox::instance::write_walkspeed(float value) {
    write_fast<float>(this->address + offsets::WalkSpeed, value);
    write_fast<float>(this->address + offsets::WalkSpeedCheck, value);
}

float roblox::instance::read_jumppower() {
    return read<float>(this->address + offsets::JumpPower);
}

float roblox::instance::read_jumppower() const {
    return read<float>(this->address + offsets::JumpPower);
}

void roblox::instance::write_jumppower(float value) {
    write_fast<float>(this->address + offsets::JumpPower, value);
}

float roblox::instance::read_hipheight() {
    return read<float>(this->address + offsets::HipHeight);
}

float roblox::instance::read_hipheight() const {
    return read<float>(this->address + offsets::HipHeight);
}

void roblox::instance::write_hipheight(float value) {
    write<float>(this->address + offsets::HipHeight, value);
}

bool roblox::instance::read_sitting() {
    return read<bool>(this->address + offsets::Sit);
}

bool roblox::instance::read_sitting() const {
    return read<bool>(this->address + offsets::Sit);
}

void roblox::instance::write_sitting(bool value) {
    write<bool>(this->address + offsets::Sit, value);
}

void roblox::instance::writeprimgrav(float gravity) {
    // todo : use game grav instead
    //write_fast<float>(read<uintptr_t>(this->address + offsets::Primitive) + offsets::PrimitiveGravity, gravity);
}

// not being used so easy fix - comment
//float roblox::instance::read_primgrav() {
//    return read<float>(read<uintptr_t>(this->address + offsets::Primitive) + offsets::PrimitiveGravity);
//}
//
//float roblox::instance::read_primgrav() const {
//    uintptr_t prim = read<uintptr_t>(this->address + offsets::Primitive);
//    if (!is_valid_address(prim)) return 0.0f;
//    return read<float>(prim + offsets::PrimitiveGravity);
//}

Vec3 roblox::instance::read_vector3_value() {
    return read<Vec3>(this->address + offsets::Value);
}

Vec3 roblox::instance::read_vector3_value() const {
    return read<Vec3>(this->address + offsets::Value);
}

void roblox::instance::write_vector3_value(Vec3 val) {
    write<Vec3>(this->address + offsets::Value, val);
}

int roblox::instance::read_int_value() {
    return read<int>(this->address + offsets::Value);
}

int roblox::instance::read_int_value() const {
    return read<int>(this->address + offsets::Value);
}

void roblox::instance::write_int_value(int val) {
    write<int>(this->address + offsets::Value, val);
}

float roblox::instance::read_float_value() {
    return read<float>(this->address + offsets::Value);
}

float roblox::instance::read_float_value() const {
    return read<float>(this->address + offsets::Value);
}

void roblox::instance::write_float_value(float val) {
    write<float>(this->address + offsets::Value, val);
}

double roblox::instance::read_double_value() {
    return read<double>(this->address + offsets::Value);
}

double roblox::instance::read_double_value() const {
    return read<double>(this->address + offsets::Value);
}

void roblox::instance::write_double_value(double val) {
    write<double>(this->address + offsets::Value, val);
}

bool roblox::instance::read_bool_value() {
    return read<bool>(this->address + offsets::Value);
}

bool roblox::instance::read_bool_value() const {
    return read<bool>(this->address + offsets::Value);
}

void roblox::instance::write_bool_value(bool val) {
    write<bool>(this->address + offsets::Value, val);
}
std::vector<roblox::instance> roblox::instance::get_children() {
    std::vector<roblox::instance> children;
    std::uint64_t begin = read<std::uint64_t>(this->address + offsets::Children);
    std::uint64_t end = read<std::uint64_t>(begin + offsets::ChildrenEnd);
    if (!begin || !end) return children;

    for (auto instances = read<std::uint64_t>(begin); instances != end; instances += 16u) {
        children.emplace_back(read<roblox::instance>(instances));
    }
    return children;
}

std::vector<roblox::instance> roblox::instance::get_children() const {
    std::vector<roblox::instance> children;
    std::uint64_t begin = read<std::uint64_t>(this->address + offsets::Children);
    std::uint64_t end = read<std::uint64_t>(begin + offsets::ChildrenEnd);
    if (!begin || !end) return children;

    for (auto instances = read<std::uint64_t>(begin); instances != end; instances += 16u) {
        children.emplace_back(read<roblox::instance>(instances));
    }
    return children;
}

roblox::instance roblox::instance::findfirstchild(std::string arg) {
    for (auto children : this->get_children()) {
        if (children.get_name() == arg) return children;
    }
    return {};
}

roblox::instance roblox::instance::findfirstchild(std::string arg) const {
    for (auto children : this->get_children()) {
        if (children.get_name() == arg) return children;
    }
    return {};
}

roblox::instance roblox::instance::read_service(std::string arg) {
    for (auto children : this->get_children()) {
        if (children.get_class_name() == arg) return children;
    }
    return {};
}

roblox::instance roblox::instance::read_service(std::string arg) const {
    for (auto children : this->get_children()) {
        if (children.get_class_name() == arg) return children;
    }
    return {};
}

roblox::instance roblox::instance::read_parent() {
    return read<roblox::instance>(this->address + offsets::Parent);
}

roblox::instance roblox::instance::read_parent() const {
    return read<roblox::instance>(this->address + offsets::Parent);
}

roblox::instance roblox::instance::model_instance() {
    return read<roblox::instance>(this->address + offsets::ModelInstance);
}

roblox::instance roblox::instance::model_instance() const {
    return read<roblox::instance>(this->address + offsets::ModelInstance);
}

roblox::instance roblox::instance::local_player() {
    return read<roblox::instance>(this->address + offsets::LocalPlayer);
}

roblox::instance roblox::instance::local_player() const {
    return read<roblox::instance>(this->address + offsets::LocalPlayer);
}

roblox::instance roblox::instance::read_workspace() {
    return read<roblox::instance>(this->address + offsets::Workspace);
}

roblox::instance roblox::instance::read_workspace() const {
    return read<roblox::instance>(this->address + offsets::Workspace);
}

roblox::instance roblox::instance::read_userid() {
    return read<roblox::instance>(this->address + offsets::UserId);
}

roblox::instance roblox::instance::read_userid() const {
    return read<roblox::instance>(this->address + offsets::UserId);
}

Vec2 roblox::instance::get_dimensions() {
    return read<Vec2>(this->address + offsets::Dimensions);
}

Vec2 roblox::instance::get_dimensions() const {
    return read<Vec2>(this->address + offsets::Dimensions);
}

void roblox::instance::setFramePosX(uint64_t position) {
    write<uint64_t>(this->address + offsets::FramePositionOffsetX, position);
}

void roblox::instance::setFramePosY(uint64_t position) {
    write<uint64_t>(this->address + offsets::FramePositionOffsetY, position);
}

void roblox::instance::initialize_mouse_service(std::uint64_t address) {
    cached_input_object = get_current_input_object(address);
    if (cached_input_object && cached_input_object != 0xFFFFFFFFFFFFFFFF) {
        const char* base_pointer = reinterpret_cast<const char*>(cached_input_object);

    }
}

void roblox::instance::write_mouse_position(std::uint64_t address, float x, float y) {
    cached_input_object = get_current_input_object(address);
    if (cached_input_object != 0 && cached_input_object != 0xFFFFFFFFFFFFFFFF) {
        Vec2 new_position = { x, y };
        write<Vec2>(cached_input_object + offsets::MousePosition, new_position);
    }
}

roblox::instance roblox::instance::readAdornee()
{
    return read<instance>(this->address + offsets::Adornee);
}





void roblox::instance::spectate(uint64_t stringhere) {
    write<std::uint64_t>(globals::camera.address + offsets::CameraSubject, stringhere);
}

void roblox::instance::unspectate() {
    write<std::uint64_t>(globals::camera.address + offsets::CameraSubject, globals::lp.humanoid.address);
}

Vec3 roblox::camera::getPos() {
    return read<Vec3>(this->address + offsets::CameraPos);
}

Vec3 roblox::camera::getPos() const {
    return read<Vec3>(this->address + offsets::CameraPos);
}

Mat3 roblox::camera::getRot() {
    return read<Mat3>(this->address + offsets::CameraRotation);
}

Mat3 roblox::camera::getRot() const {
    return read<Mat3>(this->address + offsets::CameraRotation);
}

void roblox::camera::writePos(Vec3 arg) {
    write_fast<Vec3>(this->address + offsets::CameraPos, arg);
}

void roblox::camera::writeRot(Mat3 arg) {
    write_fast<Mat3>(this->address + offsets::CameraRotation, arg);
}

int roblox::camera::getType() {
    return read<int>(this->address + offsets::CameraType);
}

int roblox::camera::getType() const {
    return read<int>(this->address + offsets::CameraType);
}

void roblox::camera::setType(int type) {
    write<int>(this->address + offsets::CameraType, type);
}

int roblox::camera::getMode() {
    return read<int>(this->address + offsets::CameraMode);
}

int roblox::camera::getMode() const {
    return read<int>(this->address + offsets::CameraMode);
}

void roblox::camera::setMode(int type) {
    write<int>(this->address + offsets::CameraMode, type);
}

roblox::instance roblox::camera::getSubject() {
    return read<roblox::instance>(this->address + offsets::CameraSubject);
}

roblox::instance roblox::camera::getSubject() const {
    return read<roblox::instance>(this->address + offsets::CameraSubject);
}

struct Quaternion { float x, y, z, w; };
Vec2 roblox::worldtoscreen(Vec3 world) {
    Quaternion quaternion;
    Vec2 dimensions = globals::visualengine.GetDimensins();
    Mat4 viewmatrix = read<Mat4>(globals::visualengine.address + offsets::viewmatrix);

    quaternion.x = (world.x * viewmatrix.data[0]) + (world.y * viewmatrix.data[1]) + (world.z * viewmatrix.data[2]) + viewmatrix.data[3];
    quaternion.y = (world.x * viewmatrix.data[4]) + (world.y * viewmatrix.data[5]) + (world.z * viewmatrix.data[6]) + viewmatrix.data[7];
    quaternion.z = (world.x * viewmatrix.data[8]) + (world.y * viewmatrix.data[9]) + (world.z * viewmatrix.data[10]) + viewmatrix.data[11];
    quaternion.w = (world.x * viewmatrix.data[12]) + (world.y * viewmatrix.data[13]) + (world.z * viewmatrix.data[14]) + viewmatrix.data[15];

    // This check for points behind the camera is okay.
    if (quaternion.w < -0.01) return { -1, -1 };

    float inv_w = 1.0f / quaternion.w;
    Vec3 ndc;
    ndc.x = quaternion.x * inv_w;
    ndc.y = quaternion.y * inv_w;
    ndc.z = quaternion.z * inv_w;

    Vec2 screenPos = {
        (dimensions.x / 2 * ndc.x) + (dimensions.x / 2),
        -(dimensions.y / 2 * ndc.y) + (dimensions.y / 2)
    };

    //  REMOVE OR COMMENT OUT THIS BLOCK
    /*
    if (screenPos.x < 0 || screenPos.x > dimensions.x || screenPos.y < 0 || screenPos.y > dimensions.y)
        return { -1, -1 };
    */

    // Now, the function will return the correct projected coordinates,
    // even if they are outside the viewport.
    return screenPos;
}

uint32_t roblox::Vector3ToIntBGR(Vec3 color) {
    int blue = static_cast<int>(color.z * 255.0f);
    int green = static_cast<int>(color.y * 255.0f);
    int red = static_cast<int>(color.x * 255.0f);
    if (blue > 255) blue = 255;
    if (blue < 0) blue = 0;
    if (green > 255) green = 255;
    if (green < 0) green = 0;
    if (red > 255) red = 255;
    if (red < 0) red = 0;
    uint32_t result = (static_cast<uint32_t>(blue) << 16) |
        (static_cast<uint32_t>(green) << 8) |
        static_cast<uint32_t>(red);
    return result;
}

void roblox::cached_input_objectzz() {
    std::uint64_t cachedobj = 0;
    while (true) {
        if (globals::mouseservice) {
            std::uint64_t inputobj = get_current_input_object(globals::mouseservice);
            if (inputobj != 0 && inputobj != 0xffffffffffffffff) {
                roblox::instance::cached_input_object = inputobj;
                cachedobj = inputobj;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}






void roblox::instance::write_material_part(roblox::instance dynamic, roblox::material material) {
    auto part = dynamic;
    std::uint64_t primitive = part.primitive();
    if (primitive) {
        write<int16_t>(primitive + offsets::MaterialType, static_cast<int16_t>(material));
    }
}

bool roblox::instance::has_service(std::string serviceName) {
    for (auto& child : this->get_children()) {
        if (child.get_class_name() == serviceName) return true;
    }
    return false;
}

void roblox::instance::write_material_tool(roblox::instance player, roblox::material material, int color) {
    auto character = player.model_instance();
    auto part = character.read_service("Tool").read_service("MeshPart");

    if (part.has_service("MeshPart")) {
        for (auto zz : part.get_children())
        {
            if (!part.address)continue;
            auto party = zz.read_service("MeshPart");
            std::uint64_t primitive = party.primitive();
            if (primitive) {
                if (!part.address)continue;
                if (part.get_class_name() == "MeshPart") {
                    write<int>(part.address + offsets::MeshPartColor3, color);
                }
                write<int16_t>(primitive + offsets::MaterialType, static_cast<int16_t>(material));
            }

        }
    }
    std::uint64_t primitive = part.primitive();
    if (primitive) {
        if (part.get_class_name() == "MeshPart") {
            write<int>(part.address + offsets::MeshPartColor3, color);
        }
        write<int16_t>(primitive + offsets::MaterialType, static_cast<int16_t>(material));
    }
}
roblox::instance roblox::instance::write_all_materials(roblox::instance dynamic, roblox::material material, int color) {
    auto character = dynamic.model_instance();
    for (auto part : character.get_children()) {
        if (!part.address)continue;
        std::uint64_t primitive = part.primitive();
        if (primitive) {
            write<int16_t>(primitive + offsets::MaterialType, static_cast<int16_t>(material));
            if (part.get_class_name() == "MeshPart") {
                write<int>(part.address + offsets::MeshPartColor3, color);
            }
        }
    }
    return *this;
}