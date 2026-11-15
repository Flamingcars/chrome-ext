#pragma once


#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../../../globals.h"



namespace render {
	// Fixed humanoid footprint used by Box Size -> Static, in studs. The box keeps these
	// proportions no matter how the target's limbs happen to be posed.
	inline constexpr float k_static_half_width = 1.8f;
	inline constexpr float k_static_half_depth = 1.0f;
	inline constexpr float k_static_height = 5.0f;
	// The cached position is the root, which sits mid body, so the box drops to the ground
	// before growing to full height.
	inline constexpr float k_static_ground_offset = 3.0f;

	void draw_box(const Vector2& top_left, const Vector2& bottom_right, ImColor color);
	void draw_text_outlined(const std::string& text, const ImVec2& position, ImColor color, float font_size);
	void draw_health_bar(const Vector2& top_left, const Vector2& bottom_right, float health_percent, float distance);
	ImVec2 label_position(int side, const Vector2& top_left, const Vector2& bottom_right, const ImVec2& text_size, int stack);
	void draw_skeleton(const roblox::ESPPlayerData& player_data);
	void draw_tracer(const Vector2& screen_pos, ImU32 color = IM_COL32(255, 255, 255, 200));
	void draw_offscreen_arrow(const Vector3& player_pos, const Vector3& camera_pos, const std::string& name, float distance);
	float calculate_font_size_by_distance(float distance);
}

namespace visuals {
	namespace settings {
		inline bool espenabled = false;
		inline bool localesp;
		inline float espmaxdistance = 5000.0f;
		inline bool espteamcheck;
		inline bool esp_teamcolor = false;
		inline float esp_teamcolorvalue[4] = { 0.35f, 0.70f, 1.00f, 1.00f };
		inline int textfont;
		inline bool resizetextbasedondistance;

		inline bool toolesp;
		inline bool tracers;
		inline bool offscreenarrows;
		inline float D_base = 20;
		inline float S_default = 12;
		inline float S_min = 6;

		inline bool nameesp;
		inline float namecolor[3] = { 1, 1, 1 };
		inline bool nameoutline;

		inline bool distanceesp;
		inline float distancecolor[3] = { 1, 1, 1 };
		inline bool distanceoutline;
		inline int distanceformat;

		inline bool boxesp;
		inline float boxcolor[3] = { 1, 1, 1 };
		inline float boxglowcolor[3] = { 1, 1, 1 };
		inline float box_glow_thickness;
		inline bool boxoutline = false;
		inline bool boxglow;
					inline int boxtype;
			inline int boxsizing;
					inline bool boxfilled;
			inline float boxfilledcolor[4] = { 0, 0, 0, 0.4 };
			inline int boxfillstyle;
			inline float boxfilledcolor2[4] = { 0.55f, 0.48f, 1.0f, 0.12f };

		inline bool fullbright = false;


		inline float stoneespcolor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
		inline float phosphateespcolor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
		inline float metalespcolor[4] = { 1.0f, 0.65f, 0.0f, 1.0f };
		inline float cornespcolor[4] = { 1.0f, 0.84f, 0.0f, 1.0f };
		inline float woolespcolor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		inline float pumpkinespcolor[4] = { 1.0f, 0.55f, 0.0f, 1.0f };
		inline float tomatoespcolor[4] = { 1.0f, 0.27f, 0.0f, 1.0f };
		inline float lemonespcolor[4] = { 1.0f, 1.0f, 0.88f, 1.0f };
		inline float blueberryespcolor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
		inline float raspberryespcolor[4] = { 0.86f, 0.08f, 0.24f, 1.0f };
		inline float deercolor[4] = { 0.6f, 0.4f, 0.2f, 1.0f };     
		inline float wildboarcolor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };   
		inline float wolfcolor[4] = { 0.4f, 0.4f, 0.4f, 1.0f };       

		inline bool chams;
		inline bool chamsglow;
		inline bool chamsoutline;
		inline float chamscolor[4] = { 0,0,0,0.4 };
		inline float chamsglowcolor[4] = { 1, 1, 1,1 };
		inline float chamsoutlinecolor[4] = { 1, 1, 1,1 };
		inline bool chamswave;
			inline bool chamsrainbpw;
	
		inline float chamsglowvalue;
		inline float chamsoutlinethickness;
		inline bool lineesp;
		inline float linecolor[3] = { 1, 1, 1 };
		inline bool lineoutline;
		inline int linelocation;

		inline bool skeletonsoutlines;
		inline bool skeletons;
		inline bool healthbars;
		inline bool healthbaroutline = false;
		inline bool healthbarsglow;
					inline float healthbarsglowcolor[3] = { 1, 1, 1 };
			        inline float healthbarcolor2[3] = { 1, 0.18f, 0.18f };

        // 0 = left, 1 = right, 2 = top, 3 = bottom of the ESP box
        inline int healthbarside = 0;

        // Same edges for the text labels; drag them on the ESP preview to change them.
        inline int nameside = 2;
        inline int distanceside = 3;
        inline int toolside = 3;
		inline float healthbarglow_value;

		inline bool trail;
		inline bool headdotesp;
		inline float headdotespcolor[3] = { 1, 1, 1 };
		inline bool headdotespoutline;
		inline int headdotradius = 5;

		inline float hitcolor[4] = { 1, 1, 1 ,1 };
		inline float trailcolor[4] = { 1, 1, 1,1 };
		inline float skeletonscolor[4] = { 1, 1, 1,1 };

		inline bool chinahats = false;
		inline bool chinahatfilled = false;
		inline bool chinahatoutline = false;
		inline bool chinahatglow = false;
		inline bool chinahatbrim = false;
		inline float chinahatcolor[4] = { 1.0f, 0.8f, 0.0f, 0.8f };
		inline float chinahatglowcolor[4] = { 1.0f, 1.0f, 0.0f, 0.5f };
		inline float chinahat_glow_thickness = 15.0f;

	}

	void render_boxes();
	void render_names();
	void render_healthbars();
	void render_distance();
	void render_tools();
	void render_skeletons();
	void render_tracers();
	void render_offscreen_arrows();
	void run_esp();
}