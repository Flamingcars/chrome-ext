#include "classes.h"
#include "../../includes.h"
#include "../usermode/usermode.h"
#include "../../globals.h"
#include "../../frontend/hidden_/modules.h"
#include "../configsystem.h"

std::string roblox::g_launch_error;

roblox::instance ReadVisualEngine() {
    return read<roblox::instance>(base_address + offsets::VisualEnginePointer);
}

roblox::instance ReadDatamodel() {
    uintptr_t padding = read<uintptr_t>(ReadVisualEngine().address + offsets::VisualEngineToDataModel1);
    return read<roblox::instance>(padding + offsets::VisualEngineToDataModel2);
}
void AddGlobalValues() {
    // Aimbot settings
    ConfigSystem::AddGlobalValue("aimbot_enabled", &features::aimbot::enabled);
    ConfigSystem::AddGlobalValue("aimbot_predictions", &features::aimbot::predictions);
    ConfigSystem::AddGlobalValue("aimbot_smoothing", &features::aimbot::smoothing);
    ConfigSystem::AddGlobalValue("aimbot_drawfov", &features::aimbot::drawfov);
    ConfigSystem::AddGlobalValue("aimbot_fovcolor", &features::aimbot::fovcolor);
    ConfigSystem::AddGlobalValue("aimbot_teamcheck", &features::aimbot::teamcheck);
    ConfigSystem::AddGlobalValue("aimbot_stickyaim", &features::aimbot::stickyaim);
    ConfigSystem::AddGlobalValue("aimbot_smoothingamount", &features::aimbot::smoothingamount);
    ConfigSystem::AddGlobalValue("aimbot_fieldofviewsize", &features::aimbot::fieldofviewsize);
    ConfigSystem::AddGlobalValue("aimbot_predictionsx", &features::aimbot::predictionsx);
    ConfigSystem::AddGlobalValue("aimbot_predictionsy", &features::aimbot::predictionsy);
    ConfigSystem::AddGlobalValue("aimbot_hitpart", &features::aimbot::hitpart);
    ConfigSystem::AddGlobalValue("aimbot_aimtype", &features::aimbot::aimtype);
    ConfigSystem::AddGlobalValue("aimbot_flags", features::aimbot::flags);
    ConfigSystem::AddGlobalValue("aimbot_range", &features::aimbot::range);
    ConfigSystem::AddGlobalValue("aimbot_healththreshhold", &features::aimbot::healththreshhold);
    ConfigSystem::AddGlobalValue("aimbot_autoswitch", &features::aimbot::autoswitch);
    ConfigSystem::AddGlobalValue("aimbot_dynamic_mouse", &features::aimbot::dynamic_mouse);
    ConfigSystem::AddGlobalValue("aimbot_mouse_size", &features::aimbot::mouse_size);
    ConfigSystem::AddGlobalValue("aimbot_mouse_thickness", &features::aimbot::mouse_thickness);
    ConfigSystem::AddGlobalValue("aimbot_mouse_style", &features::aimbot::mouse_style);
    ConfigSystem::AddGlobalValue("aimbot_mouse_color", &features::aimbot::mouse_color);
    ConfigSystem::AddGlobalValue("aimbot_mouse_outline", &features::aimbot::mouse_outline);
    ConfigSystem::AddGlobalValue("aimbot_mouse_outline_color", &features::aimbot::mouse_outline_color);
    ConfigSystem::AddGlobalValue("aimbot_mouse_outline_thickness", &features::aimbot::mouse_outline_thickness);
    ConfigSystem::AddGlobalValue("aimbot_mouse_animate", &features::aimbot::mouse_animate);
    ConfigSystem::AddGlobalValue("aimbot_mouse_animation_speed", &features::aimbot::mouse_animation_speed);
    ConfigSystem::AddGlobalValue("aimbot_mouse_gap", &features::aimbot::mouse_gap);
    ConfigSystem::AddGlobalValue("aimbot_mouse_smooth", &features::aimbot::mouse_smooth);
    ConfigSystem::AddGlobalValue("aimbot_mouse_smooth_speed", &features::aimbot::mouse_smooth_speed);
    ConfigSystem::AddKeybind("aimbot_key", "aimbot_type", &features::aimbot::aimbotbind.key, reinterpret_cast<int*>(&features::aimbot::aimbotbind.type));

    // Silent Aim settings
    ConfigSystem::AddGlobalValue("silentaim_enabled", &features::silentaim::enabled);
    ConfigSystem::AddGlobalValue("silentaim_stickyaim", &features::silentaim::stickyaim);
    ConfigSystem::AddGlobalValue("silentaim_drawfov", &features::silentaim::drawfov);
    ConfigSystem::AddGlobalValue("silentaim_fovcolor", &features::silentaim::fovcolor);
    ConfigSystem::AddGlobalValue("silentaim_teamcheck", &features::silentaim::teamcheck);
    ConfigSystem::AddGlobalValue("silentaim_fieldofview", &features::silentaim::fieldofview);
    ConfigSystem::AddGlobalValue("silentaim_hitpart", &features::silentaim::hitpart);
    ConfigSystem::AddGlobalValue("silentaim_wallcheck", &features::silentaim::wallcheck);
    ConfigSystem::AddGlobalValue("silentaim_healthcheck", &features::silentaim::healthcheck);
    ConfigSystem::AddGlobalValue("silentaim_knockcheck", &features::silentaim::knockcheck);
    ConfigSystem::AddGlobalValue("silentaim_autoswitch", &features::silentaim::autoswitch);
    ConfigSystem::AddGlobalValue("silentaim_spoofmouse", &features::silentaim::spoofmouse);
    ConfigSystem::AddGlobalValue("silentaim_range", &features::silentaim::range);
    ConfigSystem::AddGlobalValue("silentaim_healththreshhold", &features::silentaim::healththreshhold);
    ConfigSystem::AddGlobalValue("silentaim_hitchance", &features::silentaim::hitchance);
    ConfigSystem::AddGlobalValue("silentaim_flags", features::silentaim::flags);
    ConfigSystem::AddKeybind("silentaim_key", "silentaim_type", &features::silentaim::silentaimkeybind.key, reinterpret_cast<int*>(&features::silentaim::silentaimkeybind.type));

    // Triggerbot settings
    ConfigSystem::AddGlobalValue("triggerbot_enabled", &features::triggerbot::enabled);
    ConfigSystem::AddGlobalValue("triggerbot_wallcheck", &features::triggerbot::wallcheck);
    ConfigSystem::AddGlobalValue("triggerbot_reactiontime", &features::triggerbot::reactiontime);
    ConfigSystem::AddKeybind("triggerbot_key", "triggerbot_type", &features::triggerbot::triggerbotbind.key, reinterpret_cast<int*>(&features::triggerbot::triggerbotbind.type));

    // Features visual settings
    ConfigSystem::AddGlobalValue("features_soldier_esp_enabled", &features::visuals::soldier_esp_enabled);
    ConfigSystem::AddGlobalValue("features_stoneesp", &features::visuals::stoneesp);
    ConfigSystem::AddGlobalValue("features_phosphateesp", &features::visuals::phosphateesp);
    ConfigSystem::AddGlobalValue("features_metalesp", &features::visuals::metalesp);
    ConfigSystem::AddGlobalValue("features_cornesp", &features::visuals::cornesp);
    ConfigSystem::AddGlobalValue("features_woolesp", &features::visuals::woolesp);
    ConfigSystem::AddGlobalValue("features_pumpkinesp", &features::visuals::pumpkinesp);
    ConfigSystem::AddGlobalValue("features_tomatoesp", &features::visuals::tomatoesp);
    ConfigSystem::AddGlobalValue("features_lemonesp", &features::visuals::lemonesp);
    ConfigSystem::AddGlobalValue("features_blueberryesp", &features::visuals::blueberryesp);
    ConfigSystem::AddGlobalValue("features_raspberryesp", &features::visuals::raspberryesp);
    ConfigSystem::AddGlobalValue("features_resource_esp_enabled", &features::visuals::resource_esp_enabled);
    ConfigSystem::AddGlobalValue("features_animal_esp_enabled", &features::visuals::animal_esp_enabled);
    ConfigSystem::AddGlobalValue("features_deeresp", &features::visuals::deeresp);
    ConfigSystem::AddGlobalValue("features_wildboaresp", &features::visuals::wildboaresp);
    ConfigSystem::AddGlobalValue("features_wolfesp", &features::visuals::wolfesp);

    // Visual settings
    ConfigSystem::AddGlobalValue("visuals_espenabled", &visuals::settings::espenabled);
    ConfigSystem::AddGlobalValue("visuals_localesp", &visuals::settings::localesp);
    ConfigSystem::AddGlobalValue("visuals_espmaxdistance", &visuals::settings::espmaxdistance);
    ConfigSystem::AddGlobalValue("visuals_espteamcheck", &visuals::settings::espteamcheck);
    ConfigSystem::AddGlobalValue("visuals_esp_teamcolor", &visuals::settings::esp_teamcolor);
    ConfigSystem::AddGlobalValue("visuals_esp_teamcolorvalue", &visuals::settings::esp_teamcolorvalue);
    ConfigSystem::AddGlobalValue("visuals_textfont", &visuals::settings::textfont);
    ConfigSystem::AddGlobalValue("visuals_resizetextbasedondistance", &visuals::settings::resizetextbasedondistance);
    ConfigSystem::AddGlobalValue("visuals_toolesp", &visuals::settings::toolesp);
    ConfigSystem::AddGlobalValue("visuals_tracers", &visuals::settings::tracers);
    ConfigSystem::AddGlobalValue("visuals_offscreenarrows", &visuals::settings::offscreenarrows);
    ConfigSystem::AddGlobalValue("visuals_d_base", &visuals::settings::D_base);
    ConfigSystem::AddGlobalValue("visuals_s_default", &visuals::settings::S_default);
    ConfigSystem::AddGlobalValue("visuals_s_min", &visuals::settings::S_min);
    ConfigSystem::AddGlobalValue("visuals_nameesp", &visuals::settings::nameesp);
    ConfigSystem::AddGlobalValue("visuals_namecolor", &visuals::settings::namecolor);
    ConfigSystem::AddGlobalValue("visuals_nameoutline", &visuals::settings::nameoutline);
    ConfigSystem::AddGlobalValue("visuals_distanceesp", &visuals::settings::distanceesp);
    ConfigSystem::AddGlobalValue("visuals_distancecolor", &visuals::settings::distancecolor);
    ConfigSystem::AddGlobalValue("visuals_distanceoutline", &visuals::settings::distanceoutline);
    ConfigSystem::AddGlobalValue("visuals_distanceformat", &visuals::settings::distanceformat);
    ConfigSystem::AddGlobalValue("visuals_boxesp", &visuals::settings::boxesp);
    ConfigSystem::AddGlobalValue("visuals_boxcolor", &visuals::settings::boxcolor);
    ConfigSystem::AddGlobalValue("visuals_boxglowcolor", &visuals::settings::boxglowcolor);
    ConfigSystem::AddGlobalValue("visuals_box_glow_thickness", &visuals::settings::box_glow_thickness);
    ConfigSystem::AddGlobalValue("visuals_boxoutline", &visuals::settings::boxoutline);
    ConfigSystem::AddGlobalValue("visuals_boxglow", &visuals::settings::boxglow);
    ConfigSystem::AddGlobalValue("visuals_boxtype", &visuals::settings::boxtype);
    ConfigSystem::AddGlobalValue("visuals_boxsizing", &visuals::settings::boxsizing);
    ConfigSystem::AddGlobalValue("visuals_boxfilled", &visuals::settings::boxfilled);
    ConfigSystem::AddGlobalValue("visuals_boxfilledcolor", &visuals::settings::boxfilledcolor);
    ConfigSystem::AddGlobalValue("visuals_boxfillstyle", &visuals::settings::boxfillstyle);
    ConfigSystem::AddGlobalValue("visuals_boxfilledcolor2", &visuals::settings::boxfilledcolor2);
    ConfigSystem::AddGlobalValue("visuals_fullbright", &visuals::settings::fullbright);
    ConfigSystem::AddGlobalValue("visuals_stoneespcolor", &visuals::settings::stoneespcolor);
    ConfigSystem::AddGlobalValue("visuals_phosphateespcolor", &visuals::settings::phosphateespcolor);
    ConfigSystem::AddGlobalValue("visuals_metalespcolor", &visuals::settings::metalespcolor);
    ConfigSystem::AddGlobalValue("visuals_cornespcolor", &visuals::settings::cornespcolor);
    ConfigSystem::AddGlobalValue("visuals_woolespcolor", &visuals::settings::woolespcolor);
    ConfigSystem::AddGlobalValue("visuals_pumpkinespcolor", &visuals::settings::pumpkinespcolor);
    ConfigSystem::AddGlobalValue("visuals_tomatoespcolor", &visuals::settings::tomatoespcolor);
    ConfigSystem::AddGlobalValue("visuals_lemonespcolor", &visuals::settings::lemonespcolor);
    ConfigSystem::AddGlobalValue("visuals_blueberryespcolor", &visuals::settings::blueberryespcolor);
    ConfigSystem::AddGlobalValue("visuals_raspberryespcolor", &visuals::settings::raspberryespcolor);
    ConfigSystem::AddGlobalValue("visuals_deercolor", &visuals::settings::deercolor);
    ConfigSystem::AddGlobalValue("visuals_wildboarcolor", &visuals::settings::wildboarcolor);
    ConfigSystem::AddGlobalValue("visuals_wolfcolor", &visuals::settings::wolfcolor);
    ConfigSystem::AddGlobalValue("visuals_chams", &visuals::settings::chams);
    ConfigSystem::AddGlobalValue("visuals_chamsglow", &visuals::settings::chamsglow);
    ConfigSystem::AddGlobalValue("visuals_chamsoutline", &visuals::settings::chamsoutline);
    ConfigSystem::AddGlobalValue("visuals_chamscolor", &visuals::settings::chamscolor);
    ConfigSystem::AddGlobalValue("visuals_chamsglowcolor", &visuals::settings::chamsglowcolor);
    ConfigSystem::AddGlobalValue("visuals_chamsoutlinecolor", &visuals::settings::chamsoutlinecolor);
    ConfigSystem::AddGlobalValue("visuals_chamswave", &visuals::settings::chamswave);
    ConfigSystem::AddGlobalValue("visuals_chamsrainbpw", &visuals::settings::chamsrainbpw);
    ConfigSystem::AddGlobalValue("visuals_chamsglowvalue", &visuals::settings::chamsglowvalue);
    ConfigSystem::AddGlobalValue("visuals_chamsoutlinethickness", &visuals::settings::chamsoutlinethickness);
    ConfigSystem::AddGlobalValue("visuals_lineesp", &visuals::settings::lineesp);
    ConfigSystem::AddGlobalValue("visuals_linecolor", &visuals::settings::linecolor);
    ConfigSystem::AddGlobalValue("visuals_lineoutline", &visuals::settings::lineoutline);
    ConfigSystem::AddGlobalValue("visuals_linelocation", &visuals::settings::linelocation);
    ConfigSystem::AddGlobalValue("visuals_skeletonsoutlines", &visuals::settings::skeletonsoutlines);
    ConfigSystem::AddGlobalValue("visuals_skeletons", &visuals::settings::skeletons);
    ConfigSystem::AddGlobalValue("visuals_healthbars", &visuals::settings::healthbars);
    ConfigSystem::AddGlobalValue("visuals_healthbaroutline", &visuals::settings::healthbaroutline);
    ConfigSystem::AddGlobalValue("visuals_healthbarsglow", &visuals::settings::healthbarsglow);
    ConfigSystem::AddGlobalValue("visuals_healthbarsglowcolor", &visuals::settings::healthbarsglowcolor);
    ConfigSystem::AddGlobalValue("visuals_healthbarcolor2", &visuals::settings::healthbarcolor2);
    ConfigSystem::AddGlobalValue("visuals_healthbarside", &visuals::settings::healthbarside);
    ConfigSystem::AddGlobalValue("visuals_nameside", &visuals::settings::nameside);
    ConfigSystem::AddGlobalValue("visuals_distanceside", &visuals::settings::distanceside);
    ConfigSystem::AddGlobalValue("visuals_toolside", &visuals::settings::toolside);
    ConfigSystem::AddGlobalValue("visuals_healthbarglow_value", &visuals::settings::healthbarglow_value);
    ConfigSystem::AddGlobalValue("visuals_trail", &visuals::settings::trail);
    ConfigSystem::AddGlobalValue("visuals_headdotesp", &visuals::settings::headdotesp);
    ConfigSystem::AddGlobalValue("visuals_headdotespcolor", &visuals::settings::headdotespcolor);
    ConfigSystem::AddGlobalValue("visuals_headdotespoutline", &visuals::settings::headdotespoutline);
    ConfigSystem::AddGlobalValue("visuals_headdotradius", &visuals::settings::headdotradius);
    ConfigSystem::AddGlobalValue("visuals_hitcolor", &visuals::settings::hitcolor);
    ConfigSystem::AddGlobalValue("visuals_trailcolor", &visuals::settings::trailcolor);
    ConfigSystem::AddGlobalValue("visuals_skeletonscolor", &visuals::settings::skeletonscolor);
    ConfigSystem::AddGlobalValue("visuals_chinahats", &visuals::settings::chinahats);
    ConfigSystem::AddGlobalValue("visuals_chinahatfilled", &visuals::settings::chinahatfilled);
    ConfigSystem::AddGlobalValue("visuals_chinahatoutline", &visuals::settings::chinahatoutline);
    ConfigSystem::AddGlobalValue("visuals_chinahatglow", &visuals::settings::chinahatglow);
    ConfigSystem::AddGlobalValue("visuals_chinahatbrim", &visuals::settings::chinahatbrim);
    ConfigSystem::AddGlobalValue("visuals_chinahatcolor", &visuals::settings::chinahatcolor);
    ConfigSystem::AddGlobalValue("visuals_chinahatglowcolor", &visuals::settings::chinahatglowcolor);
    ConfigSystem::AddGlobalValue("visuals_chinahat_glow_thickness", &visuals::settings::chinahat_glow_thickness);

    // Player Modifications
    ConfigSystem::AddGlobalValue("playermod_speedenabled", &features::playermodifications::speedenabled);
    ConfigSystem::AddGlobalValue("playermod_jumpenabled", &features::playermodifications::jumpenabled);
    ConfigSystem::AddGlobalValue("playermod_flightenabled", &features::playermodifications::flightenabled);
    ConfigSystem::AddGlobalValue("playermod_hipheightenabled", &features::playermodifications::hipheightenabled);
    ConfigSystem::AddGlobalValue("playermod_freecamenabled", &features::playermodifications::freecamenabled);
    ConfigSystem::AddGlobalValue("playermod_speedtype", &features::playermodifications::speedtype);
    ConfigSystem::AddGlobalValue("playermod_flighttype", &features::playermodifications::flighttype);
    ConfigSystem::AddGlobalValue("playermod_speed", &features::playermodifications::speed);
    ConfigSystem::AddGlobalValue("playermod_jumppower", &features::playermodifications::jumppower);
    ConfigSystem::AddGlobalValue("playermod_flightspeed", &features::playermodifications::flightspeed);
    ConfigSystem::AddGlobalValue("playermod_freecamspeed", &features::playermodifications::freecamspeed);
    ConfigSystem::AddGlobalValue("playermod_korblox", &features::playermodifications::korblox);
    ConfigSystem::AddGlobalValue("playermod_headless", &features::playermodifications::headless);
    ConfigSystem::AddGlobalValue("playermod_animationchanger", &features::playermodifications::animationchanger);
    ConfigSystem::AddGlobalValue("playermod_localplayerforcefieldchams", &features::playermodifications::localplayerforcefieldchams);
    ConfigSystem::AddGlobalValue("playermod_localplayerforcefieldtoolchams", &features::playermodifications::localplayerforcefieldtoolchams);
    ConfigSystem::AddGlobalValue("playermod_jumpanimation", &features::playermodifications::jumpanimation);
    ConfigSystem::AddGlobalValue("playermod_idleanimation", &features::playermodifications::idleanimation);
    ConfigSystem::AddGlobalValue("playermod_walkanimation", &features::playermodifications::walkanimation);
    ConfigSystem::AddGlobalValue("playermod_climbanimation", &features::playermodifications::climbanimation);
    ConfigSystem::AddGlobalValue("playermod_runanimation", &features::playermodifications::runanimation);
    ConfigSystem::AddGlobalValue("playermod_fallanimation", &features::playermodifications::fallanimation);
    ConfigSystem::AddGlobalValue("playermod_swimanimation", &features::playermodifications::swimanimation);
    ConfigSystem::AddGlobalValue("playermod_color1", &features::playermodifications::color1);
    ConfigSystem::AddGlobalValue("playermod_color2", &features::playermodifications::color2);
    ConfigSystem::AddKeybind("speed_key", "speed_type", &features::playermodifications::speedkeybind.key, reinterpret_cast<int*>(&features::playermodifications::speedkeybind.type));
    ConfigSystem::AddKeybind("jump_key", "jump_type", &features::playermodifications::jumpkeybind.key, reinterpret_cast<int*>(&features::playermodifications::jumpkeybind.type));
    ConfigSystem::AddKeybind("flight_key", "flight_type", &features::playermodifications::flightkeybind.key, reinterpret_cast<int*>(&features::playermodifications::flightkeybind.type));
    ConfigSystem::AddKeybind("freecam_key", "freecam_type", &features::playermodifications::freecamkeybind.key, reinterpret_cast<int*>(&features::playermodifications::freecamkeybind.type));

    // World Modifications
    ConfigSystem::AddGlobalValue("worldmod_masterswitch", &features::worldmodifications::masterswitch);
}

namespace {
    const char* game_name_for(std::uint64_t gameid) {
        switch (gameid) {
        case 6035872082ULL: return "Rivals";
        case 1008451066ULL: return "Da Hood";
        case 3634139746ULL: return "Hood Customs";
        case 111958650ULL:  return "Arsenal";
        case 3747388906ULL: return "Fallen";
        case 1168263273ULL: return "Bad Business";
        case 113491250ULL:  return "PhantomForces";
        default:            return "Universal";
        }
    }

    void scan_globals() {
        globals::visualengine = ReadVisualEngine();
        globals::datamodel = ReadDatamodel();
        globals::workspace = globals::datamodel.read_workspace();
        globals::players = globals::datamodel.read_service("Players");
        globals::localplayer = globals::players.local_player();
        globals::lighting = globals::datamodel.read_service("Lighting");
        globals::camera = static_cast<roblox::camera>(globals::workspace.read_service("Camera").address);
        globals::gamename = game_name_for(globals::datamodel.read_gameid());
    }

    void watch_roblox_process() {
        const DWORD pid = static_cast<DWORD>(mem::handle_manager.process_id.load(std::memory_order_acquire));
        if (!pid) return;

        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) return;

        WaitForSingleObject(process, INFINITE);
        CloseHandle(process);

        // Roblox is gone, so there is nothing left to run against
        ExitProcess(0);
    }
}

bool roblox::startup()
{
    static MemoryManagerRAII memory_manager;

    if (!memory_manager.initialize()) {
        g_launch_error = "Open Roblox, then run again.";
        return false;
    }

    system("cls");
    scan_globals();

    // Every pointer the loader reads is a fixed offset into one specific client
    // build. On a different build those resolve to the wrong fields, and the menu
    // would open doing nothing at all. Give the client a moment to finish loading,
    // then name the mismatch instead of failing silently.
    auto plausible = [](const roblox::instance& instance) {
        return instance.address > 0x10000 && instance.address < 0x7FFFFFFFFFFF;
    };

    for (int attempt = 0; attempt < 20; ++attempt) {
        if (plausible(globals::visualengine) && plausible(globals::datamodel))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        scan_globals();
    }

    if (!plausible(globals::visualengine) || !plausible(globals::datamodel)) {
        // Only abort when the running build can be shown to differ; a client that
        // is merely still loading keeps the old waiting behaviour below.
        const bool build_mismatch = !attached_client_build.empty() &&
            attached_client_build != offsets::ClientVersion;

        if (build_mismatch) {
            g_launch_error =
                "Could not read Roblox memory.\n"
                "  Offsets are for: " + std::string(offsets::ClientVersion) + "\n"
                "  Client running:  " + attached_client_build + "\n"
                "Update the offsets in includes.h for the running build, then rebuild.";

            return false;
        }
    }

    AddGlobalValues();
    ConfigSystem::Initialize();

    hook::init();

    std::thread(watch_roblox_process).detach();




    while (true) {
        if (hummer::rescanning) {
            roblox::shutdown();
            scan_globals();
            hummer::rescanning = false;
        }
        if (globals::datamodel != ReadDatamodel() || !globals::localplayer.address || !globals::players.address) {


            hummer::rescanning = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return true;
}

bool roblox::shutdown() {
    try {
        globals::visualengine = {};
        globals::datamodel = {};
        globals::workspace = {};
        globals::players = {};
        globals::localplayer = {};
        globals::lighting = {};
        globals::camera = {};
        return true;
    }
    catch (const std::exception& e) {
        Logger::error("Exception during shutdown: {}", e.what());
        Logger::info("Attempting emergency cleanup...");
        try {
            globals::visualengine = {};
            globals::datamodel = {};
            globals::workspace = {};
            globals::players = {};
            globals::localplayer = {};
            globals::lighting = {};
            globals::camera = {};
            Logger::info("Emergency cleanup completed");
            return false;
        }
        catch (...) {
            Logger::error("Emergency cleanup failed");
            return false;
        }
    }
    catch (...) {
        Logger::error("Unknown exception during shutdown");
        try {
            globals::visualengine = {};
            globals::datamodel = {};
            globals::workspace = {};
            globals::players = {};
            globals::localplayer = {};
            globals::lighting = {};
            globals::camera = {};
            return false;
        }
        catch (...) {
            return false;
        }
    }
}