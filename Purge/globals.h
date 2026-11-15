#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "backend/classes/classes.h"
#include "backend/imgui/imgui/imgui.h"
#include "frontend/overlay/keybindsys/caller.h"
#include "backend/classes/classes.h"

// Friendly / enemy marks from the Player List. Shared so ESP features can read them too, and
// mutex-guarded because the trail cache thread reads them while the menu writes them.
namespace player_relations {
	inline std::unordered_map<std::string, int>& map() {
		static std::unordered_map<std::string, int> entries;
		return entries;
	}

	inline std::mutex& mutex() {
		static std::mutex lock;
		return lock;
	}

	// 0 = none, 1 = friendly, 2 = enemy
	inline int get(const std::string& name) {
		std::lock_guard<std::mutex> guard(mutex());
		const auto it = map().find(name);
		return it == map().end() ? 0 : it->second;
	}

	inline void set(const std::string& name, int value) {
		std::lock_guard<std::mutex> guard(mutex());

		if (value == 0) map().erase(name);
		else map()[name] = value;
	}
}

struct OriginalAnimations {
	std::string fall_id;
	std::string jump_id;
	std::string run_id;
	std::string walk_id;
	std::string climb_id;
	std::string swim_id;
	std::string idle_id;
	bool cached = false;
};

namespace globals {
	inline std::vector<roblox::wall> walls;
	inline bool overlay_visible;
	inline std::vector<std::string> whitelist;
	inline std::vector<std::string> blacklist;
	inline roblox::instance visualengine;
	inline roblox::instance datamodel;
	inline roblox::instance workspace;
	inline roblox::instance players;
	inline roblox::player lp;
	inline bool is_teammate(const roblox::player& player) {
		return lp.team.is_valid() && player.team.is_valid() && lp.team.address == player.team.address;
	}
	inline roblox::instance lighting;
	inline roblox::camera camera;
	inline roblox::instance localplayer;
	inline std::string gamename = "Universal";
	inline std::string username = "law";
	inline std::vector<roblox::player> cachedplayers;
	inline roblox::player cachedtarget;
	inline roblox::player cachedlasttarget;
	inline roblox::instance aim;
	inline uintptr_t mouseservice;
	inline ImDrawList* draw;
	inline std::vector<roblox::ESPPlayerData> cached_esp_players;
	inline Vector3 cached_camera_position;
	inline bool watermark = false;
	inline bool keybindlist = false;
	inline bool snowfall = false;
	inline bool esp_preview = true;

	inline keybind create_menubind() {
		keybind kb("menubind");
		kb.type = keybind::TOGGLE;
		kb.key = VK_INSERT;
		return kb;
	}
	inline keybind menubind = create_menubind();

	inline bool show_performance_metrics = false;
	inline std::string cheatname = "chrome.wtf";
	inline bool currentlydesynced = false;
	inline bool explorer;
	inline bool player_list;
	inline bool music_player;
	inline bool menuactive = true;
	inline float aimbiancecolor[3] = { 1,1,1 };
	inline bool ambianceon = false;
	inline bool fogon = false;
	inline float fogcolor[3] = { 1,1,1 };
	inline bool hitsounds = false;
	inline int hitsoundmode = 0;

	inline float fogstart = 50;
	inline float fogend = 100;


	inline int confignumber;

}

namespace globals {

	namespace visuals {
		inline int chamstype = 0;
	}
	namespace misc {


		inline bool desync_enabled;
		inline bool desync_interval_enabled;
		inline int desync_pulse_interval_ms;

		inline bool desync_interval_is_lagging;
		inline float desync_interval_time_left;
		inline float desync_interval_total_time;

		inline keybind desyncbind("desyncbind");
	}
}

namespace features {
		namespace aimbot {
			inline bool enabled = false;
			inline bool predictions = false;
			inline bool smoothing = false;
			inline bool drawfov = false;
			inline float fovcolor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			inline bool teamcheck = false;
			inline bool stickyaim = false;
			inline keybind aimbotbind("aimbotbind");
			inline float smoothingamount = 10;
			inline float fieldofviewsize = 50;
			inline float predictionsx = 10;
			inline float predictionsy = 10;

			inline int hitpart = 0;
			inline int aimtype = 0;

			inline std::vector<int>* flags = new std::vector<int>{ 0, 0, 0, 0, 0,0,0 };
			inline float range = 1000;
			inline float healththreshhold = 10;

			inline bool autoswitch = false;

			inline bool dynamic_mouse = false;
			inline float mouse_size = 11.8f;
			inline float mouse_thickness = 1.6f;
			inline int mouse_style = 0;
			inline float mouse_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			inline bool mouse_outline = false;
			inline float mouse_outline_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f};
			inline float mouse_outline_thickness = 1.0f;
			inline bool mouse_animate = false;
			inline float mouse_animation_speed = 2.0f;
			inline float mouse_gap = 3.7f;
			inline bool mouse_smooth = false;
			inline float mouse_smooth_speed = 11.3f;

		}
		namespace silentaim {
			inline bool enabled = false;
			inline keybind silentaimkeybind("silentaimbind");

			inline bool stickyaim = false;
			inline bool drawfov = false;
			inline float fovcolor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			inline bool teamcheck = false;
			inline float fieldofview = 150.0f;
			inline int hitpart = 0;

			inline bool wallcheck = false;
			inline bool healthcheck = false;
			inline bool knockcheck = false;
			inline bool autoswitch = false;
			inline bool spoofmouse = false;

			inline float range = 1000.0f;
			inline float healththreshhold = 10.0f;
			inline float hitchance = 100.0f;

			inline std::vector<int>* flags = new std::vector<int>{ 1, 1, 1, 1, 1, 1, 1 };
		}
		namespace triggerbot {
			inline bool enabled = false;
			inline bool rangecheck;
			inline float range = 1000;
			inline keybind triggerbotbind("triggerbotbind");

			inline bool wallcheck = false;
			inline float reactiontime = 0;
		}

		namespace visuals {
			inline bool soldier_esp_enabled = false;
			inline bool stoneesp = false;
			inline bool phosphateesp = false;
			inline bool metalesp = false;
			inline bool cornesp = false;
			inline bool woolesp = false;
			inline bool pumpkinesp = false;
			inline bool tomatoesp = false;
			inline bool lemonesp = false;
			inline bool blueberryesp = false;
			inline bool raspberryesp = false;
			inline bool resource_esp_enabled = false;

			inline bool animal_esp_enabled = false;
			inline bool deeresp = false;
			inline bool wildboaresp = false;
			inline bool wolfesp = false;

		}

		namespace playermodifications {
			inline bool speedenabled;
			inline bool jumpenabled;
			inline bool flightenabled;
			inline bool hipheightenabled;
			inline bool freecamenabled;
			inline bool desync;
			inline keybind speedkeybind("speedbind");
			inline keybind jumpkeybind("jumpkeybind");
			inline keybind flightkeybind("flightkeybind");
			inline keybind freecamkeybind("freecamkeybind");

			inline int speedtype;
			inline int flighttype;

			inline float speed = 16;
			inline float jumppower = 50;
			inline float flightspeed = 100;
			inline float freecamspeed = 1.5f;

			inline bool korblox;
			inline bool headless;
			inline bool animationchanger;
			inline bool localplayerforcefieldchams;
			inline bool localplayerforcefieldtoolchams;


			inline int jumpanimation;
			inline int idleanimation;
			inline int walkanimation;
			inline int climbanimation;
			inline int runanimation;
			inline int fallanimation;
			inline int swimanimation;

			inline float color1[3] = { 1, 1, 1 };
			inline float color2[3] = { 1, 1, 1 };



		}
		namespace worldmodifications {
			inline bool masterswitch;

		}
}


namespace functional {
	inline	OriginalAnimations original_anims;
}

namespace hummer {
	inline bool rescanning = false;
	inline bool heartbeated = false;
	inline bool rancachesystem = false;
	inline bool invaldhelper = false;

	inline volatile bool should_continue = true;
	inline volatile bool main_executing = false;
	inline volatile bool threads_active = false;
	inline volatile uint32_t heartbeat_counter = 0;
	inline volatile bool protection_enabled = true;
	inline volatile bool monitor_active = false;
}

namespace aimbot_cache {
	inline roblox::player current_target = {};
	inline roblox::player last_target = {};
	inline std::mutex target_mutex;
	inline Vec3 cached_camera_pos = {};
	inline Vec2 cached_cursor_pos = {};
	inline Vec2 cached_screen_dims = {};
	inline  roblox::camera cached_camera = {};
	inline std::chrono::steady_clock::time_point last_cache_update = std::chrono::steady_clock::now();
	inline const int cache_refresh_ms = 1;
}