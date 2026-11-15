#include "../modules.h"
#include "../../../globals.h"
#include "../../../backend/usermode/usermode.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

#pragma comment(lib, "winmm.lib")


uint32_t Vector3ToIntBGRz(Vec3 color) {
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


Vec3 cachedambiance;
static bool fullbrighton;

void runs::runs_lightingeffect(bool firstrecieved) {
    while (firstrecieved) {
        if (!firstrecieved) return;

        try {
            if (globals::ambianceon) {
                Vec3 roastturkey = Vector3(globals::aimbiancecolor[0], globals::aimbiancecolor[1], globals::aimbiancecolor[2]);
                globals::lighting.write_ambiance(roastturkey);
            }
            if (globals::fogon) {
                Vec3 roastturkey = Vector3(globals::fogcolor[0], globals::fogcolor[1], globals::fogcolor[2]);
                globals::lighting.write_fogcolor(roastturkey);
                globals::lighting.write_fogstart(globals::fogstart);
            }
        }
        catch (...) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void runs::runs_world(bool firstrecieved) {
    while (firstrecieved) {
        if (firstrecieved) {
            globals::mouseservice = globals::datamodel.findfirstchild("MouseService").address;

            if (globals::localplayer.address && globals::workspace.address && globals::camera.address) {
                roblox::instance i;
                if (features::playermodifications::localplayerforcefieldchams) {
                    i.write_all_materials(globals::localplayer, roblox::material::ForceField, Vector3ToIntBGRz(Vector3(features::playermodifications::color1[0], features::playermodifications::color1[1], features::playermodifications::color1[2])));
                }
                if (features::playermodifications::localplayerforcefieldtoolchams) {
                    i.write_material_tool(globals::localplayer, roblox::material::ForceField, Vector3ToIntBGRz(Vector3(features::playermodifications::color2[0], features::playermodifications::color2[1], features::playermodifications::color2[2])));
                }
            }
            if (visuals::settings::fullbright) {
                if (!fullbrighton) {
                    cachedambiance = globals::lighting.read_ambiance();
                    globals::lighting.write_ambiance(Vec3(1, 1, 1));
                    fullbrighton = true;
                }
               
            }
            else
                if (fullbrighton) {

                    globals::lighting.write_ambiance(cachedambiance);
                    fullbrighton = false;
                }
            if (features::playermodifications::headless) {
                globals::lp.head.write_part_size(Vector3::zero());

            }
            if (features::playermodifications::korblox) {
                auto lp = globals::lp;

                auto lfoot = lp.rightfoot;
                auto lll = lp.rightlowerleg;
                auto lul = lp.rightupperleg;

                if (lfoot.address && lll.address && lul.address) {

                    lfoot.write_meshid("https://assetdelivery.roblox.com/v1/asset/?id=9598310128");
                    lfoot.write_textureid("https://www.roblox.com/asset/?id=902843398");
                    lfoot.write_transparency(1);

                    lll.write_meshid("https://assetdelivery.roblox.com/v1/asset/?id=9598310138");
                    lll.write_textureid("https://www.roblox.com/asset/?id=902843398");
                    lll.write_transparency(1);

                    lul.write_meshid("https://assetdelivery.roblox.com/v1/asset/?id=9598310133");
                    lul.write_textureid("https://assetdelivery.roblox.com/v1/asset/?id=902843398");
                    lul.write_part_size(Vec3(0.862, 1.5, 0.862));
                }
            }
            if (features::playermodifications::animationchanger) {
                auto localplayer = globals::localplayer.model_instance();
                if (!localplayer.address) continue;
                auto animate = localplayer.findfirstchild("Animate");
                if (!animate.address) {
                    continue;
                }

                try {
                    auto fall_folder = animate.findfirstchild("fall");
                    auto jump_folder = animate.findfirstchild("jump");
                    auto run_folder = animate.findfirstchild("run");
                    auto walk_folder = animate.findfirstchild("walk");
                    auto climb_folder = animate.findfirstchild("climb");
                    auto swim_folder = animate.findfirstchild("swim");
                    auto idle_folder = animate.findfirstchild("idle");

                    auto fall = fall_folder.address ? fall_folder.findfirstchild("FallAnim") : 0;
                    auto jump = jump_folder.address ? jump_folder.findfirstchild("JumpAnim") : 0;
                    auto run = run_folder.address ? run_folder.findfirstchild("RunAnim") : 0;
                    auto walk = walk_folder.address ? walk_folder.findfirstchild("WalkAnim") : 0;
                    auto climb = climb_folder.address ? climb_folder.findfirstchild("ClimbAnim") : 0;
                    auto swim = swim_folder.address ? swim_folder.findfirstchild("Swim") : 0;
                    auto idle1 = idle_folder.address ? idle_folder.findfirstchild("Animation1") : 0;
                    auto idle2 = idle_folder.address ? idle_folder.findfirstchild("Animation2") : 0;

                    if (!functional::original_anims.cached) {
                        if (fall.address) {
                            functional::original_anims.fall_id = functions::extractIdFromUrl(fall.read_animationid());
                        }
                        if (jump.address) {
                            functional::original_anims.jump_id = functions::extractIdFromUrl(jump.read_animationid());
                        }
                        if (run.address) {
                            functional::original_anims.run_id = functions::extractIdFromUrl(run.read_animationid());
                        }
                        if (walk.address) {
                            functional::original_anims.walk_id = functions::extractIdFromUrl(walk.read_animationid());
                        }
                        if (climb.address) {
                            functional::original_anims.climb_id = functions::extractIdFromUrl(climb.read_animationid());
                        }
                        if (swim.address) {
                            functional::original_anims.swim_id = functions::extractIdFromUrl(swim.read_animationid());
                        }
                        if (idle1.address) {
                            functional::original_anims.idle_id = functions::extractIdFromUrl(idle1.read_animationid());
                        }
                        functional::original_anims.cached = true;
                    }

                    if (features::playermodifications::animationchanger) {
                        if (fall.address) {
                            fall.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::fallanimation, 3));
                        }

                        if (jump.address) {
                            jump.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::jumpanimation, 2));
                        }

                        if (run.address) {
                            run.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::runanimation, 0));
                        }

                        if (walk.address) {
                            walk.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::walkanimation, 1));
                        }

                        if (climb.address) {
                            climb.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::climbanimation, 6));
                        }

                        if (swim.address) {
                            swim.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::swimanimation, 5));
                        }

                        if (idle1.address) {
                            idle1.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::idleanimation, 4));
                        }

                        if (idle2.address) {
                            idle2.write_animationid("http://www.roblox.com/asset/?id=" +
                                functions::fetchIdfromCase(features::playermodifications::idleanimation, 4));
                        }
                    }
                    else {
                        if (functional::original_anims.cached) {
                            if (fall.address && !functional::original_anims.fall_id.empty()) {
                                fall.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.fall_id);
                            }
                            if (jump.address && !functional::original_anims.jump_id.empty()) {
                                jump.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.jump_id);
                            }
                            if (run.address && !functional::original_anims.run_id.empty()) {
                                run.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.run_id);
                            }
                            if (walk.address && !functional::original_anims.walk_id.empty()) {
                                walk.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.walk_id);
                            }
                            if (climb.address && !functional::original_anims.climb_id.empty()) {
                                climb.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.climb_id);
                            }
                            if (swim.address && !functional::original_anims.swim_id.empty()) {
                                swim.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.swim_id);
                            }
                            if (idle1.address && !functional::original_anims.idle_id.empty()) {
                                idle1.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.idle_id);
                            }
                            if (idle2.address && !functional::original_anims.idle_id.empty()) {
                                idle2.write_animationid("http://www.roblox.com/asset/?id=" + functional::original_anims.idle_id);
                            }
                        }
                    }

                }
                catch (...) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
            }


            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        else
            return;
    }
}
static Vector3 lookvec(const Matrix3& rotationMatrix) {
    return rotationMatrix.get_column(2);
} 

static Vector3 rightvec(const Matrix3& rotationMatrix) {
    return rotationMatrix.get_column(0);
}

void runs::runs_flight(bool firstrecieved) {
    using clock = std::chrono::high_resolution_clock;
    static bool aircheck = false;
    static float lastprimgrav = -1;

    while (firstrecieved) {
        auto start_time = clock::now();
        if (firstrecieved) {
            if (features::playermodifications::flightenabled && !globals::menuactive) {
                features::playermodifications::flightkeybind.update();
                auto humanoid = globals::lp.humanoid;
                auto hrp = globals::lp.hrp;
                if (features::playermodifications::flightkeybind.enabled) {

                    if (!is_valid_address(humanoid.address) || !is_valid_address(hrp.address)) continue;

                    roblox::camera camera = globals::camera;
                    Matrix3 rotation_matrix = camera.getRot();
                    Vector3 look_vector = lookvec(rotation_matrix);
                    Vector3 right_vector = rightvec(rotation_matrix);
                    Vector3 direction = { 0.0f, 0.0f, 0.0f };

                    aircheck = false;

                    if (GetAsyncKeyState('W') & 0x8000) {
                        direction = direction - look_vector;
                        aircheck = true;
                    }
                    if (GetAsyncKeyState('S') & 0x8000) {
                        direction = direction + look_vector;
                        aircheck = true;
                    }
                    if (GetAsyncKeyState('A') & 0x8000) {
                        direction = direction - right_vector;
                        aircheck = true;
                    }
                    if (GetAsyncKeyState('D') & 0x8000) {
                        direction = direction + right_vector;
                        aircheck = true;
                    }

                    if (direction.magnitude() > 0) {
                        direction = direction.normalize();
                    }

                    for (int i = 0; i < 1; i++) {
                        // todo : use game grav instead
                        /*float current_gravity = hrp.read_primgrav();
                        if (current_gravity == 0) continue;
                        lastprimgrav = current_gravity;
                        hrp.writeprimgrav(0);*/
                    }

                    if (!aircheck) {
                        for (int i = 0; i < 2500; i++) {
                            Vector3 current_vel = hrp.get_velocity();
                            if (current_vel.x == 0.0f && current_vel.y == 0.0f && current_vel.z == 0.0f) continue;
                            hrp.write_velocity({ 0.0f, 0.0f, 0.0f });
                        }
                    }
                    else {

                        static auto last_time = std::chrono::high_resolution_clock::now();
                        auto current_time = std::chrono::high_resolution_clock::now();

                        std::chrono::duration<float> delta = current_time - last_time;
                        float deltaTime = delta.count();

                        last_time = current_time;

                        float lerp_factor = 1.5f;

                        auto current_pos = hrp.get_pos();
                        Vector3 target_pos = Vector3(
                            current_pos.x + direction.x * features::playermodifications::flightspeed,
                            current_pos.y + direction.y * features::playermodifications::flightspeed,
                            current_pos.z + direction.z * features::playermodifications::flightspeed
                        );

                        switch (features::playermodifications::flighttype) {
                        case 0: {
                            for (int i = 0; i < 2500; i++) {
   

                                Vector3 final_pos = Vector3(
                                    current_pos.x + (target_pos.x - current_pos.x) * (lerp_factor * deltaTime),
                                    current_pos.y + (target_pos.y - current_pos.y) * (lerp_factor * deltaTime),
                                    current_pos.z + (target_pos.z - current_pos.z) * (lerp_factor * deltaTime)
                                );

                                hrp.write_position(final_pos);
                            }
                            break;
                        }
                        case 1: {
                            Vector3 target_velocity = direction * features::playermodifications::flightspeed;
                            for (int i = 0; i < 2500; i++) {
                                hrp.write_velocity(target_velocity);
                            }
                            break;
                        }
                        case 2: {
                            break;
                        }
                        }
                    }
                }
                else {
                    if (lastprimgrav != -1) {
                        for (int i = 0; i < 1; i++) {
                            // todo : use game grav instead
                            /*float current_gravity = hrp.read_primgrav();
                            if (current_gravity == lastprimgrav) continue;
                            hrp.writeprimgrav(lastprimgrav);*/
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            static LARGE_INTEGER frequency;
            static LARGE_INTEGER lastTime;
            static bool timeInitialized = false;
            if (!timeInitialized) {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&lastTime);
                timeBeginPeriod(1);
                timeInitialized = true;
            }
            const double targetFrameTime = 1.0 / 165;
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            if (elapsedSeconds < targetFrameTime) {
                DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
                if (sleepMilliseconds > 0) {
                    Sleep(sleepMilliseconds);
                }
            }
            do {
                QueryPerformanceCounter(&currentTime);
                elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            } while (elapsedSeconds < targetFrameTime);
            lastTime = currentTime;
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
        else
            return;
    }
}

void runs::runs_jumppower(bool firstrecieved) {
    static float original_jumppower = -1.0f;
    static bool jumppower_saved = false;

    while (firstrecieved) {
        if (firstrecieved) {
            if (features::playermodifications::jumpenabled && !globals::menuactive) {
                features::playermodifications::jumpkeybind.update();
                if (features::playermodifications::jumpkeybind.enabled) {
                    auto humanoid = globals::lp.humanoid;
                    auto hrp = globals::lp.hrp;
                    if (!is_valid_address(humanoid.address) || !is_valid_address(hrp.address)) continue;

                    if (!jumppower_saved) {
                        original_jumppower = humanoid.read_jumppower();
                        jumppower_saved = true;
                    }

                    for (int i = 0; i < 2500; i++) {
                        float current_jumppower = humanoid.read_jumppower();
                        if (current_jumppower == features::playermodifications::jumppower) continue;
                        humanoid.write_jumppower(features::playermodifications::jumppower);
                    }
                }
                else {
                    if (jumppower_saved) {
                        auto humanoid = globals::lp.humanoid;
                        if (is_valid_address(humanoid.address)) {
                            humanoid.write_jumppower(original_jumppower);
                        }
                        jumppower_saved = false;
                        original_jumppower = -1.0f;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else {
                if (jumppower_saved) {
                    auto humanoid = globals::lp.humanoid;
                    if (is_valid_address(humanoid.address)) {
                        humanoid.write_jumppower(original_jumppower);
                    }
                    jumppower_saved = false;
                    original_jumppower = -1.0f;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            static LARGE_INTEGER frequency;
            static LARGE_INTEGER lastTime;
            static bool timeInitialized = false;
            if (!timeInitialized) {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&lastTime);
                timeBeginPeriod(1);
                timeInitialized = true;
            }
            const double targetFrameTime = 1.0 / 165;
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            if (elapsedSeconds < targetFrameTime) {
                DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
                if (sleepMilliseconds > 0) {
                    Sleep(sleepMilliseconds);
                }
            }
            do {
                QueryPerformanceCounter(&currentTime);
                elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            } while (elapsedSeconds < targetFrameTime);
            lastTime = currentTime;
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
        else
            return;
    }
}

void runs::runs_speed(bool firstrecieved) {
    static float original_walkspeed = -1.0f;
    static bool walkspeed_saved = false;

    while (firstrecieved) {
        if (firstrecieved) {
            if (features::playermodifications::speedenabled && !globals::menuactive) {
                features::playermodifications::speedkeybind.update();
                if (features::playermodifications::speedkeybind.enabled) {
                    auto humanoid = globals::lp.humanoid;
                    auto hrp = globals::lp.hrp;
                    if (!is_valid_address(humanoid.address) || !is_valid_address(hrp.address)) continue;

                    auto dir = humanoid.get_move_dir();
                    Vector3 target_velocity = Vector3(
                        dir.x * features::playermodifications::speed,
                        hrp.get_velocity().y,
                        dir.z * features::playermodifications::speed
                    );
                    auto current_pos = hrp.get_pos();
                    Vector3 target_pos = Vector3(
                        current_pos.x + dir.x * features::playermodifications::speed,
                        current_pos.y,
                        current_pos.z + dir.z * features::playermodifications::speed
                    );
                    Vector3 current_vel = hrp.get_velocity();
                    switch (features::playermodifications::speedtype) {

                    case 0: {
                        for (int i = 0; i < 2500; i++) {
                            if (current_vel.x == target_velocity.x && current_vel.z == target_velocity.z) continue;
                            hrp.write_velocity(target_velocity);
                        }
                        break;
                    }

                    case 1: {
                        if (!walkspeed_saved) {
                            original_walkspeed = humanoid.read_walkspeed();
                            walkspeed_saved = true;
                        }

                        for (int i = 0; i < 2500; i++) {
                            float current_walkspeed = humanoid.read_walkspeed();
                            if (current_walkspeed == features::playermodifications::speed) continue;
                            humanoid.write_walkspeed(features::playermodifications::speed);
                        }
                        break;
                    }

                    default: {
                        for (int i = 0; i < 2500; i++) {
                            float lerp_factor = 0.01f;
                            Vector3 final_pos = Vector3(
                                current_pos.x + (target_pos.x - current_pos.x) * lerp_factor,
                                target_pos.y,
                                current_pos.z + (target_pos.z - current_pos.z) * lerp_factor
                            );
                            hrp.write_position(final_pos);
                        }
                        break;
                    }
                    }
                }
                else {
                    if (walkspeed_saved && features::playermodifications::speedtype == 1) {
                        auto humanoid = globals::lp.humanoid;
                        if (is_valid_address(humanoid.address)) {
                            humanoid.write_walkspeed(original_walkspeed);
                        }
                        walkspeed_saved = false;
                        original_walkspeed = -1.0f;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else {
                if (walkspeed_saved) {
                    auto humanoid = globals::lp.humanoid;
                    if (is_valid_address(humanoid.address)) {
                        humanoid.write_walkspeed(original_walkspeed);
                    }
                    walkspeed_saved = false;
                    original_walkspeed = -1.0f;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            static LARGE_INTEGER frequency;
            static LARGE_INTEGER lastTime;
            static bool timeInitialized = false;
            if (!timeInitialized) {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&lastTime);
                timeBeginPeriod(1);
                timeInitialized = true;
            }
            const double targetFrameTime = 1.0 / 165;
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            if (elapsedSeconds < targetFrameTime) {
                DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
                if (sleepMilliseconds > 0) {
                    Sleep(sleepMilliseconds);
                }
            }
            do {
                QueryPerformanceCounter(&currentTime);
                elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            } while (elapsedSeconds < targetFrameTime);
            lastTime = currentTime;
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
        else {
            return;
        }
    }
}

bool is_key_pressed(int vkey) {
    return (GetAsyncKeyState(vkey) & 0x8000) != 0;
}

static Vector3 cam_pos;
static bool freecammed;

void update_movement() {
    Vector3 look_vector = lookvec(globals::camera.getRot());
    Vector3 right_vector = rightvec(globals::camera.getRot());
    Vector3 up_vector = { 0.0f, 1.0f, 0.0f };
    Vector3 direction = { 0.0f, 0.0f, 0.0f };

    if (is_key_pressed('W')) {
        direction = direction - look_vector;
    }
    if (is_key_pressed('S')) {
        direction = direction + look_vector;
    }
    if (is_key_pressed('A')) {
        direction = direction - right_vector;
    }
    if (is_key_pressed('D')) {
        direction = direction + right_vector;
    }
    if (is_key_pressed(VK_SPACE)) {
        direction = direction + up_vector;
    }
    if (is_key_pressed(VK_LSHIFT)) {
        direction = direction - up_vector;
    }

    if (direction.magnitude() > 0) {
        direction = direction.normalize();
    }

    float speed_multiplier = is_key_pressed(VK_LCONTROL) ? 2.0f : 1.0f;
    float move_speed = features::playermodifications::freecamspeed * speed_multiplier;

    cam_pos = cam_pos + (direction * (move_speed / 10.0f));
}

static bool campossed;
static Vec3 ogcampos;

void runs::runs_freecam(bool firstrecieved) {
    while (firstrecieved) {
        if (firstrecieved) {
            if (features::playermodifications::freecamenabled && !globals::menuactive) {
                features::playermodifications::freecamkeybind.update();
                if (features::playermodifications::freecamkeybind.enabled) {
                    if (!freecammed) {
                        globals::lp.hrp.setAnchored(true);
                        cam_pos = globals::camera.getPos();
                        ogcampos = globals::lp.hrp.get_pos();
                        campossed = true;
                        freecammed = true;
                    }

                    update_movement();

                    for (int i = 0; i < 10000; i++) {
                        Vector3 current_hrp_pos = globals::lp.hrp.get_pos();
                        if (current_hrp_pos.x == ogcampos.x && current_hrp_pos.y == ogcampos.y && current_hrp_pos.z == ogcampos.z) {
                            Vector3 current_cam_pos = globals::camera.getPos();
                            if (current_cam_pos.x == cam_pos.x && current_cam_pos.y == cam_pos.y && current_cam_pos.z == cam_pos.z) continue;
                            globals::camera.writePos(cam_pos);
                        }
                        else {
                            globals::lp.hrp.write_position(ogcampos);
                            globals::camera.writePos(cam_pos);
                        }
                    }
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (freecammed && !features::playermodifications::freecamkeybind.enabled) {
                globals::lp.hrp.setAnchored(false);
                campossed = false;
                ogcampos = Vec3::zero();
                freecammed = false;
            }

            static LARGE_INTEGER frequency;
            static LARGE_INTEGER lastTime;
            static bool timeInitialized = false;
            if (!timeInitialized) {
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&lastTime);
                timeBeginPeriod(1);
                timeInitialized = true;
            }
            const double targetFrameTime = 1.0 / 165;
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            if (elapsedSeconds < targetFrameTime) {
                DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
                if (sleepMilliseconds > 0) {
                    Sleep(sleepMilliseconds);
                }
            }
            do {
                QueryPerformanceCounter(&currentTime);
                elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            } while (elapsedSeconds < targetFrameTime);
            lastTime = currentTime;
        }
        else
            return;
    }
}