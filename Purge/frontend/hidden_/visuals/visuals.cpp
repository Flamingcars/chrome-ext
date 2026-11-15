#include "visuals.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>
#include <algorithm>
#include <cmath>
#include "../../../backend/usermode/usermode.h"
#include "../../../includes.h"
#include "coolfeatures/bulletracer.h"
#include "coolfeatures/trail.h"
#include "../cache/visual2ndcachefile.h"
#include "../cache/fallen/fallencache.h"

#define max
#undef max
#define min
#undef min

using namespace roblox;

namespace {
    struct BoxFillColors {
        ImU32 top = 0;
        ImU32 bottom = 0;
    };

    // The two colours a fill is built from: the fill colour and the gradient colour.
    BoxFillColors box_fill_colors() {
        const ImVec4 base(
            visuals::settings::boxfilledcolor[0],
            visuals::settings::boxfilledcolor[1],
            visuals::settings::boxfilledcolor[2],
            visuals::settings::boxfilledcolor[3]);

        const ImVec4 blend(
            visuals::settings::boxfilledcolor2[0],
            visuals::settings::boxfilledcolor2[1],
            visuals::settings::boxfilledcolor2[2],
            visuals::settings::boxfilledcolor2[3]);

        BoxFillColors colors;
        colors.top = ImGui::ColorConvertFloat4ToU32(base);
        colors.bottom = ImGui::ColorConvertFloat4ToU32(blend);

        if (visuals::settings::boxfillstyle == 0)
            colors.bottom = colors.top;

        return colors;
    }

    void draw_fill_rect(ImDrawList* draw, ImVec2 top_left, ImVec2 bottom_right) {
        if (bottom_right.x <= top_left.x || bottom_right.y <= top_left.y) return;

        const BoxFillColors colors = box_fill_colors();

        if (visuals::settings::boxfillstyle == 0) {
            draw->AddRectFilled(top_left, bottom_right, colors.top);
            return;
        }

        draw->AddRectFilledMultiColor(top_left, bottom_right,
            colors.top, colors.top, colors.bottom, colors.bottom);
    }

    bool static_box_bounds(const ESPPlayerData& player, Vec3& bounds_min, Vec3& bounds_max) {
        const Vec3 root = player.position;

        const auto plausible = [](float value) { return value == value && value > -10000.0f && value < 10000.0f; };

        if (!plausible(root.x) || !plausible(root.y) || !plausible(root.z)) return false;

        const float ground = root.y - render::k_static_ground_offset;

        bounds_min = Vec3{ root.x - render::k_static_half_width, ground, root.z - render::k_static_half_depth };
        bounds_max = Vec3{ root.x + render::k_static_half_width, ground + render::k_static_height, root.z + render::k_static_half_depth };

        return true;
    }

    // Screen rectangle of that fixed box, so the 2D styles can share the same sizing mode.
    bool static_box_screen(const ESPPlayerData& player, ImVec2& top_left, ImVec2& bottom_right) {
        Vec3 bounds_min = Vec3::zero();
        Vec3 bounds_max = Vec3::zero();

        if (!static_box_bounds(player, bounds_min, bounds_max)) return false;

        const float corners[8][3] = {
            { bounds_min.x, bounds_min.y, bounds_min.z },
            { bounds_max.x, bounds_min.y, bounds_min.z },
            { bounds_min.x, bounds_max.y, bounds_min.z },
            { bounds_max.x, bounds_max.y, bounds_min.z },
            { bounds_min.x, bounds_min.y, bounds_max.z },
            { bounds_max.x, bounds_min.y, bounds_max.z },
            { bounds_min.x, bounds_max.y, bounds_max.z },
            { bounds_max.x, bounds_max.y, bounds_max.z }
        };

        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        int visible = 0;

        for (const auto& corner : corners) {
            const Vec2 point = roblox::worldtoscreen(Vec3{ corner[0], corner[1], corner[2] });

            if (point.x == -1.0f && point.y == -1.0f) continue;

            min_x = (std::min)(min_x, point.x);
            max_x = (std::max)(max_x, point.x);
            min_y = (std::min)(min_y, point.y);
            max_y = (std::max)(max_y, point.y);

            visible++;
        }

        if (visible < 4 || max_x <= min_x || max_y <= min_y) return false;

        top_left = ImVec2(min_x, min_y);
        bottom_right = ImVec2(max_x, max_y);

        return true;
    }
}

// left/top/right/bottom are screen coordinates. The corner style used to treat the right
// and bottom edges as sizes, which drew every corner box at roughly double scale.
void drawBox(float left, float top, float right, float bottom, ImColor color) {
    const float width = right - left;
    const float height = bottom - top;

    if (width <= 0.0f || height <= 0.0f) return;

    ImU32 outlineColor = IM_COL32(0, 0, 0, color.Value.w * 255);

    auto draw = ImGui::GetBackgroundDrawList();

    if (visuals::settings::boxfilled) {
        draw_fill_rect(draw, ImVec2(left, top), ImVec2(right, bottom));
    }

    if (visuals::settings::boxtype == 0) {
        if (visuals::settings::boxglow) {
            draw->AddShadowRect(ImVec2(left, top), ImVec2(right, bottom),
                ImGui::ColorConvert(visuals::settings::boxglowcolor), visuals::settings::box_glow_thickness,
                ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 0);
        }

        // Matches the menu preview byte for byte: one 1.5 px box with a 3 px black outline a
        // pixel outside it, instead of the old three stacked 1 px rects.
        if (visuals::settings::boxoutline) {
            draw->AddRect(ImVec2(left - 1.0f, top - 1.0f), ImVec2(right + 1.0f, bottom + 1.0f),
                outlineColor, 0.0f, 0, 3.0f);
        }

        draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.0f, 0, 1.5f);
    }
    else if (visuals::settings::boxtype == 1) {
        // Same arm proportions and stroke weights as the preview's corner style.
        const float arm_x = width * 0.28f;
        const float arm_y = height * 0.18f;

        const auto arm = [&](ImVec2 from, ImVec2 to) {
            if (visuals::settings::boxoutline)
                draw->AddLine(from, to, outlineColor, 5.5f);

            draw->AddLine(from, to, color, 1.5f);
        };

        arm(ImVec2(left, top), ImVec2(left + arm_x, top));
        arm(ImVec2(left, top), ImVec2(left, top + arm_y));

        arm(ImVec2(right, top), ImVec2(right - arm_x, top));
        arm(ImVec2(right, top), ImVec2(right, top + arm_y));

        arm(ImVec2(left, bottom), ImVec2(left + arm_x, bottom));
        arm(ImVec2(left, bottom), ImVec2(left, bottom - arm_y));

        arm(ImVec2(right, bottom), ImVec2(right - arm_x, bottom));
        arm(ImVec2(right, bottom), ImVec2(right, bottom - arm_y));
    }
}

// Outlines only: a filled silhouette over a character hides the model, so the 3D style
// ignores the fill settings entirely.
void drawBox3D(const ESPPlayerData& player, ImU32 color, float thickness) {
    const Vec3 root = player.position;
    const Vec3 head = player.head_position;

    const auto plausible = [](float value) { return value == value && value > -10000.0f && value < 10000.0f; };

    if (!plausible(root.x) || !plausible(root.y) || !plausible(root.z)) return;
    if (!plausible(head.x) || !plausible(head.y) || !plausible(head.z)) return;

    Vec3 bounds_min = Vec3::zero();
    Vec3 bounds_max = Vec3::zero();

    if (visuals::settings::boxsizing == 1) {
        if (!static_box_bounds(player, bounds_min, bounds_max)) return;
    }
    else {
        // The cached player copy carries world positions only, not character parts, so the
        // box is derived from them: the root sits mid body, the head marks the top.
        const float top = head.y + 0.55f;
        const float bottom = root.y - 3.0f;
        const float height = top - bottom;

        if (height < 2.0f || height > 12.0f) return;

        const float half_width = (std::min)((std::max)(height * 0.38f, 1.2f), 2.6f);
        const float half_depth = half_width * 0.5f;

        bounds_min = Vec3{ root.x - half_width, bottom, root.z - half_depth };
        bounds_max = Vec3{ root.x + half_width, top, root.z + half_depth };
    }

    const Vec3 corners[8] = {
        Vec3{ bounds_min.x, bounds_min.y, bounds_min.z },
        Vec3{ bounds_max.x, bounds_min.y, bounds_min.z },
        Vec3{ bounds_min.x, bounds_max.y, bounds_min.z },
        Vec3{ bounds_max.x, bounds_max.y, bounds_min.z },
        Vec3{ bounds_min.x, bounds_min.y, bounds_max.z },
        Vec3{ bounds_max.x, bounds_min.y, bounds_max.z },
        Vec3{ bounds_min.x, bounds_max.y, bounds_max.z },
        Vec3{ bounds_max.x, bounds_max.y, bounds_max.z }
    };

    ImVec2 screen[8];
    bool visible[8] = {};
    int visible_count = 0;

    for (int i = 0; i < 8; ++i) {
        const Vec2 point = roblox::worldtoscreen(corners[i]);

        visible[i] = point.x > 0.0f && point.y > 0.0f;
        screen[i] = ImVec2(point.x, point.y);

        if (visible[i]) visible_count++;
    }

    if (visible_count < 3) return;

    auto draw = ImGui::GetBackgroundDrawList();

    static const int edges[12][2] = {
        { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
        { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    const ImU32 outline_color = IM_COL32(0, 0, 0, 255);

    for (const auto& edge : edges) {
        if (!visible[edge[0]] || !visible[edge[1]]) continue;

        if (visuals::settings::boxoutline)
            draw->AddLine(screen[edge[0]], screen[edge[1]], outline_color, thickness + 2.0f);

        draw->AddLine(screen[edge[0]], screen[edge[1]], color, thickness);
    }
}


void threeDCirlce(const Vec3& position, float baseRadius, int segments, float thickness) {
    auto dimensions = globals::visualengine.get_dimensions();
    auto viewmatrix = globals::visualengine.GetViewMatrix();
    auto drawList = ImGui::GetBackgroundDrawList();
    float currentTime = ImGui::GetTime();

    float radius = baseRadius * 1.5f;

    float pulsingRadius = radius + sin(currentTime) * 0.2f * radius;

    for (int i = 0; i <= segments; i++) {
        float hue = fmod(currentTime * 0.1f + (float)i / segments, 1.0f);
        ImVec4 color = ImColor::HSV(hue, 1.0f, 1.0f, 1.0f);

        float theta = 2.0f * M_PI * i / segments;
        float nextTheta = 2.0f * M_PI * (i + 1) / segments;

        Vec3 pointOnCircle = {
            position.x + pulsingRadius * cos(theta),
            position.y,
            position.z + pulsingRadius * sin(theta)
        };
        Vec3 nextPointOnCircle = {
            position.x + pulsingRadius * cos(nextTheta),
            position.y,
            position.z + pulsingRadius * sin(nextTheta)
        };

        auto screenPoint = roblox::worldtoscreen(pointOnCircle);
        auto nextScreenPoint = roblox::worldtoscreen(nextPointOnCircle);

        if (screenPoint.x != -1 && screenPoint.y != -1 && nextScreenPoint.x != -1 && nextScreenPoint.y != -1) {
            drawList->AddLine(ImVec2(screenPoint.x, screenPoint.y), ImVec2(nextScreenPoint.x, nextScreenPoint.y), ImGui::ColorConvertFloat4ToU32(color), thickness);
        }
    }

    float fillOpacity = 0.6f;
    for (float r = pulsingRadius; r > 0; r -= pulsingRadius / 15) {
        float fillHue = fmod(currentTime * 0.1f, 1.0f);
        ImVec4 fillColor = ImColor::HSV(fillHue, 0.5f, 1.0f, fillOpacity);

        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * M_PI * i / segments;
            float nextTheta = 2.0f * M_PI * (i + 1) / segments;

            Vec3 pointOnCircle = {
                position.x + r * cos(theta),
                position.y,
                position.z + r * sin(theta)
            };
            Vec3 nextPointOnCircle = {
                position.x + r * cos(nextTheta),
                position.y,
                position.z + r * sin(nextTheta)
            };

            auto screenPoint = roblox::worldtoscreen(pointOnCircle);
            auto nextScreenPoint = roblox::worldtoscreen(nextPointOnCircle);

            if (screenPoint.x != -1 && screenPoint.y != -1 && nextScreenPoint.x != -1 && nextScreenPoint.y != -1) {
                drawList->AddLine(ImVec2(screenPoint.x, screenPoint.y), ImVec2(nextScreenPoint.x, nextScreenPoint.y), ImGui::ColorConvertFloat4ToU32(fillColor), thickness);
            }
        }
        fillOpacity *= 0.95f;
    }
}

namespace render {
    struct HealthBarAnimation {
        float current_health = 1.0f;
        float target_health = 1.0f;
        std::chrono::steady_clock::time_point last_update;

        void update(float new_health) {
            target_health = new_health;
            auto now = std::chrono::steady_clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();
            last_update = now;

            if (delta > 0) {
                float lerp_speed = 0.005f * delta;
                current_health = current_health + (target_health - current_health) * lerp_speed;

                if (abs(current_health - target_health) < 0.01f) {
                    current_health = target_health;
                }
            }
        }
    };

    struct FadeAnimation {
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

    static std::map<uintptr_t, HealthBarAnimation> health_animations;
    static FadeAnimation box_fade, name_fade, health_fade, distance_fade, tool_fade, skeleton_fade, tracer_fade, arrow_fade, chams_fade, chinahat_fade;

    void draw_text_outlined_freetype(const std::string& text, const ImVec2& position, ImColor color, float font_size) {
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

    void rect_filled(const ImVec2& top_left, const ImVec2& bottom_right, ImU32 color) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(top_left, bottom_right, color);
    }

    void rect(const ImVec2& top_left, const ImVec2& bottom_right, ImU32 color) {
        ImGui::GetBackgroundDrawList()->AddRect(top_left, bottom_right, color);
    }

    // Places a label against one edge of the box. Labels sharing an edge stack outwards in the
    // order they are drawn, so tool name and distance stay apart when both sit on the same side.
    ImVec2 label_position(int side, const Vector2& top_left, const Vector2& bottom_right,
        const ImVec2& text_size, int stack) {
        const float gap = 2.0f;

        // A slot is always at least one full text line, so labels sharing an edge can never
        // overwrite each other whatever font size their caller drew them at.
        const float slot = ImMax(text_size.y, 13.0f) + 4.0f;
        const float center_x = (top_left.x + bottom_right.x) * 0.5f;
        const float center_y = (top_left.y + bottom_right.y) * 0.5f;
        const float offset = slot * static_cast<float>(stack);

        // Step around the health bar when it is locked to the same edge, so a label never sits on
        // top of it.
        const float bar_clearance =
            (visuals::settings::healthbars && visuals::settings::healthbarside == side) ? 6.0f : 0.0f;

        if (side == 2)
            return ImVec2(center_x - text_size.x * 0.5f,
                top_left.y - gap - bar_clearance - text_size.y - offset);

        if (side == 3)
            return ImVec2(center_x - text_size.x * 0.5f, bottom_right.y + gap + bar_clearance + offset);

        if (side == 0)
            return ImVec2(top_left.x - gap - bar_clearance - text_size.x,
                center_y - text_size.y * 0.5f + offset);

        return ImVec2(bottom_right.x + gap + bar_clearance, center_y - text_size.y * 0.5f + offset);
    }

    void draw_health_bar(const Vector2& top_left, const Vector2& bottom_right, float health_percent, float distance, uintptr_t player_id) {

        auto& animation = health_animations[player_id];
        animation.update(health_percent);
        float animated_health = std::max(0.0f, std::min(1.0f, animation.current_health));


        const float bar_width = 3.0f;
        const float bar_offset = 3.0f;
        const float outline_thickness = 1.0f;


        float alpha_factor = health_fade.current_alpha;

        const float box_height = bottom_right.y - top_left.y;
        const float box_width = bottom_right.x - top_left.x;

        // 0 = left, 1 = right, 2 = top, 3 = bottom
        const int side = visuals::settings::healthbarside;
        const bool vertical = (side == 0 || side == 1);

        ImVec2 bar_pos;
        ImVec2 bar_size;

        if (side == 1) {
            bar_pos = ImVec2(bottom_right.x + bar_offset, top_left.y);
            bar_size = ImVec2(bar_width, box_height);
        }
        else if (side == 2) {
            bar_pos = ImVec2(top_left.x, top_left.y - bar_width - bar_offset);
            bar_size = ImVec2(box_width, bar_width);
        }
        else if (side == 3) {
            bar_pos = ImVec2(top_left.x, bottom_right.y + bar_offset);
            bar_size = ImVec2(box_width, bar_width);
        }
        else {
            bar_pos = ImVec2(top_left.x - bar_width - bar_offset, top_left.y);
            bar_size = ImVec2(bar_width, box_height);
        }

        const float inner_length = (vertical ? bar_size.y : bar_size.x) - outline_thickness * 2.0f;
        const float health_fill_length = inner_length * animated_health;

        const ImVec2 bg_top_left = ImVec2(bar_pos.x, bar_pos.y);
        const ImVec2 bg_bottom_right = ImVec2(bar_pos.x + bar_size.x, bar_pos.y + bar_size.y);

        ImVec2 health_top_left;
        ImVec2 health_bottom_right;

        if (vertical) {
            // Fills upward from the bottom edge.
            health_bottom_right = ImVec2(bg_bottom_right.x - outline_thickness, bg_bottom_right.y - outline_thickness);
            health_top_left = ImVec2(bar_pos.x + outline_thickness,
                bg_bottom_right.y - outline_thickness - health_fill_length);
        }
        else {
            // Fills rightward from the left edge.
            health_top_left = ImVec2(bar_pos.x + outline_thickness, bar_pos.y + outline_thickness);
            health_bottom_right = ImVec2(bar_pos.x + outline_thickness + health_fill_length,
                bg_bottom_right.y - outline_thickness);
        }

        rect_filled(
            { bg_top_left.x, bg_top_left.y },
            { bg_bottom_right.x, bg_bottom_right.y },
            IM_COL32(30, 30, 30, static_cast<int>(200 * alpha_factor))
        );

        if (animated_health > 0.0f && health_fill_length > 1.0f) {
            const int alpha = static_cast<int>(255 * alpha_factor);

            const ImU32 top_color = IM_COL32(
                visuals::settings::healthbarsglowcolor[0] * 255,
                visuals::settings::healthbarsglowcolor[1] * 255,
                visuals::settings::healthbarsglowcolor[2] * 255, alpha);

            const ImU32 bottom_color = IM_COL32(
                visuals::settings::healthbarcolor2[0] * 255,
                visuals::settings::healthbarcolor2[1] * 255,
                visuals::settings::healthbarcolor2[2] * 255, alpha);

            // Gradient down the filled part, so the bar reads as a ramp rather than a slab.
            ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
                health_top_left, health_bottom_right,
                top_color, top_color, bottom_color, bottom_color);
        }
        rect(
            { bg_top_left.x, bg_top_left.y },
            { bg_bottom_right.x, bg_bottom_right.y },
            IM_COL32(0, 0, 0, static_cast<int>(255 * alpha_factor))
        );

        int health_percentage = static_cast<int>(std::round(animated_health * 100.0f));
        if (health_percentage < 100) {
            std::string health_text = std::to_string(health_percentage);
            ImVec2 text_size = ImGui::CalcTextSize(health_text.c_str());

            // Keep the number beside the bar, whichever side it is locked to.
            ImVec2 text_pos;

            if (side == 1) {
                text_pos = ImVec2(bg_bottom_right.x + 2.0f, bar_pos.y + (bar_size.y - text_size.y) * 0.5f);
            }
            else if (side == 2) {
                text_pos = ImVec2(bar_pos.x + (bar_size.x - text_size.x) * 0.5f, bar_pos.y - text_size.y - 1.0f);
            }
            else if (side == 3) {
                text_pos = ImVec2(bar_pos.x + (bar_size.x - text_size.x) * 0.5f, bg_bottom_right.y + 1.0f);
            }
            else {
                text_pos = ImVec2(bar_pos.x - text_size.x - 2.0f, bar_pos.y + (bar_size.y - text_size.y) * 0.5f);
            }

            ImU32 text_color = IM_COL32(255, 255, 255, static_cast<int>(255 * alpha_factor));
            draw_text_outlined_freetype(health_text, text_pos, text_color, 9.0f);
        }
    }



    void draw_china_hat_cached(const ChinaHatRenderData& hat_data, float alpha) {
        if (!hat_data.valid) return;

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        ImU32 outline_color_base = IM_COL32(0, 0, 0, 255);
        ImU32 glow_color_base_brim = IM_COL32(255, 165, 0, 255);
        ImU32 glow_color_base_connecting = IM_COL32(255, 165, 0, 255);
        float glow_intensity = 25.f;

        auto create_thick_line_quad_vertices = [](ImVec2 p1, ImVec2 p2, float thickness) -> std::array<ImVec2, 4> {
            ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
            float len = std::hypot(dir.x, dir.y);
            if (len == 0.0f) {
                return { p1, p1, p1, p1 };
            }
            dir.x /= len;
            dir.y /= len;

            ImVec2 normal = ImVec2(-dir.y, dir.x);
            ImVec2 offset = ImVec2(normal.x * thickness * 0.5f, normal.y * thickness * 0.5f);

            return {
                ImVec2(p1.x + offset.x, p1.y + offset.y),
                ImVec2(p2.x + offset.x, p2.y + offset.y),
                ImVec2(p2.x - offset.x, p2.y - offset.y),
                ImVec2(p1.x - offset.x, p1.y - offset.y)
            };
            };

        auto draw_line_with_shadow = [&](const ImVec2& p1, const ImVec2& p2, float thickness, ImU32 main_color_alpha, ImU32 glow_color_alpha, float glow_intensity_val) {
            std::vector<ImVec2> line_points = { p1, p2 };

          //  draw_list->AddPolyline(line_points.data(), line_points.size(), outline_color_base, false, thickness + 1.5f);
            draw_list->AddPolyline(line_points.data(), line_points.size(), main_color_alpha, false, thickness);

            std::array<ImVec2, 4> shadow_quad_vertices = create_thick_line_quad_vertices(p1, p2, thickness);

            draw_list->AddShadowConvexPoly(
                shadow_quad_vertices.data(),
                shadow_quad_vertices.size(),
                main_color_alpha,
                glow_intensity_val * hat_data.distance,
                ImVec2(0, 0),
            ImDrawFlags_ShadowCutOutShapeBackground
            );
            };

        for (const auto& line : hat_data.brim_lines) {
            if (line.valid) {
                ImU32 brim_color_alpha = (line.color & 0x00FFFFFF) | (static_cast<ImU32>(255 * alpha) << 24);
                ImU32 brim_glow_alpha = (glow_color_base_brim & 0x00FFFFFF) | (static_cast<ImU32>(255 * alpha) << 24);

                draw_line_with_shadow(line.start, line.end, line.thickness, brim_color_alpha, brim_glow_alpha, glow_intensity);
            }
        }

        for (const auto& line : hat_data.connecting_lines) {
            if (line.valid) {
                ImU32 connecting_color_alpha = (line.color & 0x00FFFFFF) | (static_cast<ImU32>(50 * alpha) << 24);
                ImU32 connecting_glow_alpha = (glow_color_base_connecting & 0x00FFFFFF) | (static_cast<ImU32>(50 * alpha) << 24);
                draw_line_with_shadow(line.start, line.end, line.thickness, connecting_color_alpha, connecting_glow_alpha, glow_intensity);
            }
        }
    }

    void draw_skeleton_cached(const SkeletonRenderData& skel_data, float alpha, ImU32 color_override = 0) {
        if (!skel_data.valid) return;

        auto draw_list = ImGui::GetBackgroundDrawList();

        for (const auto& line : skel_data.skeleton_lines) {
            if (line.valid) {
                ImU32 line_color = color_override != 0 ? color_override : line.color;
                ImU32 color_with_alpha = (line_color & 0x00FFFFFF) | (static_cast<ImU32>(255 * alpha) << 24);
                draw_list->AddLine(line.start, line.end, color_with_alpha, line.thickness);
            }
        }
    }

    void draw_tracer(const Vector2& screen_pos, ImU32 color) {
        auto draw_list = ImGui::GetBackgroundDrawList();
        auto screen_dims = globals::visualengine.get_dimensions();
        ImVec2 start_point = ImVec2(screen_dims.x / 2.0f, screen_dims.y);

        ImU32 tracer_color = tracer_fade.apply_fade(color);
        draw_list->AddLine(start_point, ImVec2(screen_pos.x, screen_pos.y), tracer_color, 2.0f);
    }

    void draw_offscreen_arrow(const Vector3& player_pos, const Vector3& camera_pos, const std::string& name, float distance) {
        auto draw_list = ImGui::GetBackgroundDrawList();
        auto screen_dims = globals::visualengine.get_dimensions();
        ImVec2 screen_center = ImVec2(screen_dims.x / 2.0f, screen_dims.y / 2.0f);

        Vector3 direction = Vector3(player_pos.x - camera_pos.x, 0, player_pos.z - camera_pos.z);
        float length = std::sqrt(direction.x * direction.x + direction.z * direction.z);

        if (length <= 0) return;

        direction.x /= length;
        direction.z /= length;

        float arrow_distance = (std::min)(screen_dims.x, screen_dims.y) * 0.35f;
        ImVec2 arrow_pos = ImVec2(
            screen_center.x + direction.x * arrow_distance,
            screen_center.y - direction.z * arrow_distance
        );

        arrow_pos.x = (std::max)(20.0f, (std::min)(arrow_pos.x, screen_dims.x - 20.0f));
        arrow_pos.y = (std::max)(20.0f, (std::min)(arrow_pos.y, screen_dims.y - 20.0f));

        ImU32 arrow_fill = arrow_fade.apply_fade(IM_COL32(255, 100, 0, 255));
        ImU32 arrow_border = arrow_fade.apply_fade(IM_COL32(0, 0, 0, 255));

        draw_list->AddCircleFilled(arrow_pos, 10.0f, arrow_fill);
        draw_list->AddCircle(arrow_pos, 10.0f, arrow_border, 0, 2.0f);

        std::string distance_text = "[" + std::to_string(static_cast<int>(distance)) + "m]";
        ImVec2 text_size = ImGui::CalcTextSize(distance_text.c_str());
        ImVec2 text_pos = ImVec2(arrow_pos.x - text_size.x / 2.0f, arrow_pos.y + 15.0f);

        ImU32 text_color = arrow_fade.apply_fade(IM_COL32(255, 100, 0, 255));
        draw_text_outlined_freetype(distance_text, text_pos, text_color, 11.0f);
    }
}

struct BonePosition {
    Vector3 position;
    bool valid;

    BonePosition() : valid(false) {}
};

struct Polygon_t {
    std::vector<ImVec2> vertices;
    float depth;
    float distance;
    int player_address;
};

namespace player_access {
    std::vector<ESPPlayerData> get_cached_esp_players();
    Vector3 get_cached_camera_position();
    std::vector<Polygon_t> get_cached_chams_polygons();
    Vector3 get_cached_bone_position(uintptr_t player_address, size_t bone_index);
    bool is_cached_bone_valid(uintptr_t player_address, size_t bone_index);
}

static ImColor shift_hue(ImColor color, float hue_shift) {
    float r = color.Value.x;
    float g = color.Value.y;
    float b = color.Value.z;
    float a = color.Value.w;

    float max_val = std::max({ r, g, b });
    float min_val = std::min({ r, g, b });
    float delta = max_val - min_val;

    if (delta == 0) return color;

    float h, s, v;
    v = max_val;
    s = delta / max_val;

    if (r == max_val) {
        h = (g - b) / delta;
    }
    else if (g == max_val) {
        h = 2.0f + (b - r) / delta;
    }
    else {
        h = 4.0f + (r - g) / delta;
    }
    h *= 60.0f;
    if (h < 0) h += 360.0f;

    h += hue_shift;
    while (h >= 360.0f) h -= 360.0f;
    while (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r_new, g_new, b_new;
    if (h < 60) {
        r_new = c; g_new = x; b_new = 0;
    }
    else if (h < 120) {
        r_new = x; g_new = c; b_new = 0;
    }
    else if (h < 180) {
        r_new = 0; g_new = c; b_new = x;
    }
    else if (h < 240) {
        r_new = 0; g_new = x; b_new = c;
    }
    else if (h < 300) {
        r_new = x; g_new = 0; b_new = c;
    }
    else {
        r_new = c; g_new = 0; b_new = x;
    }

    return ImColor(r_new + m, g_new + m, b_new + m, a);
}



float cross_product_2d1111(const ImVec2& p0, const ImVec2& a, const ImVec2& b) {
    return (a.x - p0.x) * (b.y - p0.y) - (a.y - p0.y) * (b.x - p0.x);
}

std::vector<ImVec2> convexHull111(std::vector<ImVec2>& points) {
    int n = points.size();
    if (n <= 3) return points;
    int bottommost_index = 0;
    for (int i = 1; i < n; ++i) {
        if (points[i].y < points[bottommost_index].y || (points[i].y == points[bottommost_index].y && points[i].x < points[bottommost_index].x)) {
            bottommost_index = i;
        }
    }
    std::swap(points[0], points[bottommost_index]);
    ImVec2 p0 = points[0];
    std::sort(points.begin() + 1, points.end(), [&](const ImVec2& a, const ImVec2& b) {
        float cross = cross_product_2d1111(p0, a, b);
        if (cross == 0) {
            float distSqA = (p0.x - a.x) * (p0.x - a.x) + (p0.y - a.y) * (p0.y - a.y);
            float distSqB = (p0.x - b.x) * (p0.x - b.x) + (p0.y - b.y) * (p0.y - b.y);
            return distSqA < distSqB;
        }
        return cross > 0;
        });
    std::vector<ImVec2> hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);
    for (int i = 2; i < n; ++i) {
        while (hull.size() > 1 && cross_product_2d1111(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }
    return hull;
}

std::vector<ImVec2> create_union_polygon(const std::vector<Polygon_t>& polygons) {
    if (polygons.empty()) return {};

    std::vector<ImVec2> all_points;

    for (const auto& polygon : polygons) {
        if (polygon.vertices.size() < 3) continue;

        for (const auto& vertex : polygon.vertices) {
            all_points.push_back(vertex);
        }
    }

    if (all_points.size() < 3) return {};

    return convexHull111(all_points);
}

std::vector<ImVec2> expand_polygon(const std::vector<ImVec2>& polygon, float expansion = 2.0f) {
    if (polygon.size() < 3) return polygon;

    std::vector<ImVec2> expanded;
    expanded.reserve(polygon.size());

    ImVec2 centroid(0, 0);
    for (const auto& vertex : polygon) {
        centroid.x += vertex.x;
        centroid.y += vertex.y;
    }
    centroid.x /= polygon.size();
    centroid.y /= polygon.size();

    for (const auto& vertex : polygon) {
        ImVec2 direction(vertex.x - centroid.x, vertex.y - centroid.y);
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);

        if (length > 0.001f) {
            direction.x /= length;
            direction.y /= length;

            expanded.push_back(ImVec2(
                vertex.x + direction.x * expansion,
                vertex.y + direction.y * expansion
            ));
        }
        else {
            expanded.push_back(vertex);
        }
    }

    return expanded;
}


std::vector<std::vector<Polygon_t>> group_polygons_by_player(const std::vector<Polygon_t>& polygons_to_draw) {
    std::vector<std::vector<Polygon_t>> grouped;
    std::map<uintptr_t, std::vector<Polygon_t>> player_map;

    for (const auto& polygon : polygons_to_draw) {
        player_map[polygon.player_address].push_back(polygon);
    }

    for (const auto& pair : player_map) {
        if (!pair.second.empty()) {
            grouped.push_back(pair.second);
        }
    }

    return grouped;
}

std::vector<ImVec2> create_detailed_union_polygon(const std::vector<Polygon_t>& player_polygons) {
    if (player_polygons.empty()) return {};

    std::vector<ImVec2> all_points;

    for (const auto& polygon : player_polygons) {
        if (polygon.vertices.size() < 3) continue;

        for (const auto& vertex : polygon.vertices) {
            bool is_duplicate = false;
            for (const auto& existing : all_points) {
                float dx = vertex.x - existing.x;
                float dy = vertex.y - existing.y;
                if (dx * dx + dy * dy < 4.0f) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                all_points.push_back(vertex);
            }
        }
    }

    if (all_points.size() < 3) return {};

    return convexHull111(all_points);
}

std::vector<ImVec2> smooth_polygon(const std::vector<ImVec2>& polygon, int iterations = 2) {
    if (polygon.size() < 3) return polygon;

    std::vector<ImVec2> smoothed = polygon;

    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<ImVec2> temp = smoothed;

        for (size_t i = 0; i < smoothed.size(); ++i) {
            size_t prev = (i == 0) ? smoothed.size() - 1 : i - 1;
            size_t next = (i + 1) % smoothed.size();

            temp[i].x = (smoothed[prev].x + smoothed[i].x * 2.0f + smoothed[next].x) * 0.25f;
            temp[i].y = (smoothed[prev].y + smoothed[i].y * 2.0f + smoothed[next].y) * 0.25f;
        }

        smoothed = temp;
    }

    return smoothed;
}

namespace visuals {



    void render_chams() {
        render::chams_fade.update(settings::chams || settings::chamsoutline || settings::chamsglow);
        if (render::chams_fade.current_alpha <= 0.01f) return;

        auto polygons_to_draw = player_access::get_cached_chams_polygons();
        if (polygons_to_draw.empty()) return;

        auto draw_list = ImGui::GetBackgroundDrawList();
        draw_list->_FringeScale = 0.f;
        ImDrawListFlags oldFlags = draw_list->Flags;

        static float wave_time = 0.0f;
        wave_time += ImGui::GetIO().DeltaTime * 2.0f;

        ImColor fillColor(
            settings::chamscolor[0],
            settings::chamscolor[1],
            settings::chamscolor[2],
            settings::chamscolor[3] * render::chams_fade.current_alpha
        );
        ImColor outlineColor(
            settings::chamsoutlinecolor[0],
            settings::chamsoutlinecolor[1],
            settings::chamsoutlinecolor[2],
            settings::chamsoutlinecolor[3] * render::chams_fade.current_alpha
        );
        ImColor glowColor(
            settings::chamsglowcolor[0],
            settings::chamsglowcolor[1],
            settings::chamsglowcolor[2],
            settings::chamsglowcolor[3] * render::chams_fade.current_alpha
        );

        if (globals::visuals::chamstype == 1) {
            auto grouped_polygons = group_polygons_by_player(polygons_to_draw);

            for (const auto& player_polygons : grouped_polygons) {
                std::vector<ImVec2> union_polygon = create_detailed_union_polygon(player_polygons);

                if (union_polygon.size() >= 3) {
                    std::vector<ImVec2> smoothed_polygon = smooth_polygon(union_polygon, 1);

                    ImVec2 center(0, 0);
                    for (const auto& vertex : smoothed_polygon) {
                        center.x += vertex.x;
                        center.y += vertex.y;
                    }
                    center.x /= smoothed_polygon.size();
                    center.y /= smoothed_polygon.size();

                    ImColor current_fill = fillColor;
                    ImColor current_outline = outlineColor;
                    ImColor current_glow = glowColor;

                    if (settings::chamsrainbpw || settings::chamswave) {
                        if (settings::chamsrainbpw) {
                            float wave_dist = std::sqrt(center.x * center.x + center.y * center.y) * 0.01f;
                            float wave_eff = (std::sin(wave_time + wave_dist) + 1.0f) * 0.5f;
                            float hue_shift = wave_eff * 360.0f;
                            current_fill = shift_hue(fillColor, hue_shift);
                            current_outline = shift_hue(outlineColor, hue_shift);
                            current_glow = shift_hue(glowColor, hue_shift);
                        }
                        if (settings::chamswave) {
                            float wave_eff = std::sin(wave_time + center.y * 0.01f);
                            float alpha_mult = (wave_eff + 1.0f) * 0.5f;
                            current_fill.Value.w *= alpha_mult;
                            current_outline.Value.w *= alpha_mult;
                            current_glow.Value.w *= alpha_mult;
                        }
                    }

                    if (settings::chamsglow) {
                        float glow_intensity = 120.0f;
                        draw_list->AddShadowConvexPoly(
                            smoothed_polygon.data(),
                            smoothed_polygon.size(),
                            current_glow,
                            glow_intensity,
                            ImVec2(0, 0),
                            ImDrawFlags_ShadowCutOutShapeBackground
                        );
                    }

                    if (settings::chams) {
                        draw_list->AddConvexPolyFilled(
                            smoothed_polygon.data(),
                            smoothed_polygon.size(),
                            current_fill
                        );
                    }

                    if (settings::chamsoutline) {
                        draw_list->AddPolyline(
                            smoothed_polygon.data(),
                            smoothed_polygon.size(),
                            current_outline,
                            true,
                            1.5f
                        );
                    }
                }
            }
        }
        else {
            std::vector<ImVec2> all_vertices;
            std::vector<int> polygon_indices;

            for (const auto& polygon : polygons_to_draw) {
                if (polygon.vertices.size() < 3) continue;

                int start_index = all_vertices.size();
                for (const auto& vertex : polygon.vertices) {
                    all_vertices.push_back(vertex);
                }
                polygon_indices.push_back(start_index);
                polygon_indices.push_back(polygon.vertices.size());
            }

            if (settings::chamsglow) {
                for (size_t i = 0; i < polygon_indices.size(); i += 2) {
                    int start_idx = polygon_indices[i];
                    int count = polygon_indices[i + 1];
                    float distance_approx = 100.0f;
                    float glow_intensity = 100.0f / (distance_approx / 10.0f);

                    ImColor current_glow = glowColor;
                    if (settings::chamsrainbpw || settings::chamswave) {
                        ImVec2 center(0, 0);
                        for (int j = 0; j < count; j++) {
                            center.x += all_vertices[start_idx + j].x;
                            center.y += all_vertices[start_idx + j].y;
                        }
                        center.x /= count;
                        center.y /= count;

                        if (settings::chamsrainbpw) {
                            float wave_dist = std::sqrt(center.x * center.x + center.y * center.y) * 0.01f;
                            float wave_eff = (std::sin(wave_time + wave_dist) + 1.0f) * 0.5f;
                            float hue_shift = wave_eff * 360.0f;
                            current_glow = shift_hue(glowColor, hue_shift);
                        }
                        if (settings::chamswave) {
                            float wave_eff = std::sin(wave_time + center.y * 0.01f);
                            float alpha_mult = (wave_eff + 1.0f) * 0.5f;
                            current_glow.Value.w *= alpha_mult;
                        }
                    }

                    draw_list->AddShadowConvexPoly(
                        &all_vertices[start_idx],
                        count,
                        current_glow,
                        glow_intensity,
                        ImVec2(0, 0),
                        ImDrawFlags_ShadowCutOutShapeBackground
                    );
                }
            }

            if (settings::chams) {
                for (size_t i = 0; i < polygon_indices.size(); i += 2) {
                    int start_idx = polygon_indices[i];
                    int count = polygon_indices[i + 1];

                    ImColor current_fill = fillColor;
                    if (settings::chamsrainbpw || settings::chamswave) {
                        ImVec2 center(0, 0);
                        for (int j = 0; j < count; j++) {
                            center.x += all_vertices[start_idx + j].x;
                            center.y += all_vertices[start_idx + j].y;
                        }
                        center.x /= count;
                        center.y /= count;

                        if (settings::chamsrainbpw) {
                            float wave_dist = std::sqrt(center.x * center.x + center.y * center.y) * 0.01f;
                            float wave_eff = (std::sin(wave_time + wave_dist) + 1.0f) * 0.5f;
                            float hue_shift = wave_eff * 360.0f;
                            current_fill = shift_hue(fillColor, hue_shift);
                        }
                        if (settings::chamswave) {
                            float wave_eff = std::sin(wave_time + center.y * 0.01f);
                            float alpha_mult = (wave_eff + 1.0f) * 0.5f;
                            current_fill.Value.w *= alpha_mult;
                        }
                    }

                    draw_list->AddConvexPolyFilled(&all_vertices[start_idx], count, current_fill);
                }
            }

            if (settings::chamsoutline) {
                for (size_t i = 0; i < polygon_indices.size(); i += 2) {
                    int start_idx = polygon_indices[i];
                    int count = polygon_indices[i + 1];

                    ImColor current_outline = outlineColor;
                    if (settings::chamsrainbpw || settings::chamswave) {
                        ImVec2 center(0, 0);
                        for (int j = 0; j < count; j++) {
                            center.x += all_vertices[start_idx + j].x;
                            center.y += all_vertices[start_idx + j].y;
                        }
                        center.x /= count;
                        center.y /= count;

                        if (settings::chamsrainbpw) {
                            float wave_dist = std::sqrt(center.x * center.x + center.y * center.y) * 0.01f;
                            float wave_eff = (std::sin(wave_time + wave_dist) + 1.0f) * 0.5f;
                            float hue_shift = wave_eff * 360.0f;
                            current_outline = shift_hue(outlineColor, hue_shift);
                        }
                        if (settings::chamswave) {
                            float wave_eff = std::sin(wave_time + center.y * 0.01f);
                            float alpha_mult = (wave_eff + 1.0f) * 0.5f;
                            current_outline.Value.w *= alpha_mult;
                        }
                    }

                    draw_list->AddPolyline(
                        &all_vertices[start_idx],
                        count,
                        current_outline,
                        true,
                        0.5f
                    );
                }
            }
        }

        draw_list->Flags = oldFlags;
    }

    static bool uses_team_color(bool teammate) {
        return settings::esp_teamcolor && teammate;
    }

    static ImU32 team_color() {
        const float* c = settings::esp_teamcolorvalue;
        return IM_COL32(
            static_cast<int>(c[0] * 255.0f),
            static_cast<int>(c[1] * 255.0f),
            static_cast<int>(c[2] * 255.0f),
            static_cast<int>(c[3] * 255.0f));
    }

    // Label colours live in settings as RGB, the same values the preview draws with.
    static ImU32 settings_rgb(const float* c) {
        return IM_COL32(
            static_cast<int>(c[0] * 255.0f),
            static_cast<int>(c[1] * 255.0f),
            static_cast<int>(c[2] * 255.0f),
            255);
    }

    void render_boxes() {
        render::box_fade.update(visuals::settings::boxesp);
        if (render::box_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        ImVec4 base_color = ImVec4(settings::boxcolor[0], settings::boxcolor[1], settings::boxcolor[2], 1.0f);
        ImU32 box_color = render::box_fade.apply_fade(ImGui::ColorConvertFloat4ToU32(base_color));
        ImU32 team_box_color = render::box_fade.apply_fade(team_color());

        for (const auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen) continue;

            ImVec2 top_left(player.top_left.x, player.top_left.y);
            ImVec2 bottom_right(player.bottom_right.x, player.bottom_right.y);

            // Static sizing ignores the cached bounds and projects a fixed humanoid box.
            if (settings::boxsizing == 1 && !static_box_screen(player, top_left, bottom_right)) continue;

            if (top_left.x >= bottom_right.x || top_left.y >= bottom_right.y) continue;

            const ImU32 color = uses_team_color(player.teammate) ? team_box_color : box_color;

            if (settings::boxtype == 2)
                drawBox3D(player, color, 1.5f);
            else
                drawBox(top_left.x, top_left.y, bottom_right.x, bottom_right.y, color);
        }
    }

    void render_names() {
        render::name_fade.update(settings::nameesp);
        if (render::name_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        for (const auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen || player.name.empty()) continue;

            // Same typography as the menu preview: the label font at its native size, so a label
            // in game can never come out a different size than the preview it was tuned against.
            const float label_size = ImGui::GetFontSize();
            ImVec2 text_size = ImGui::CalcTextSize(player.name.c_str());

            ImVec2 text_position = render::label_position(settings::nameside,
                player.top_left, player.bottom_right, text_size, 0);

            ImU32 base_name_color = uses_team_color(player.teammate) ? team_color() : settings_rgb(settings::namecolor);
            ImU32 name_color = render::name_fade.apply_fade(base_name_color);
            render::draw_text_outlined_freetype(player.name, text_position, name_color, label_size);
        }
    }

    void render_healthbars() {
        render::health_fade.update(settings::healthbars);
        if (render::health_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        for (auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen) continue;

            float health_ratio = player.health;
            if (health_ratio != health_ratio) health_ratio = 0.0f;
            health_ratio = (std::max)(0.0f, (std::min)(1.0f, health_ratio));


            render::draw_health_bar(player.top_left, player.bottom_right, health_ratio, player.distance,
                reinterpret_cast<uintptr_t>(&player.player_ref));
        }
    }

    void render_distance() {
        render::distance_fade.update(settings::distanceesp);
        if (render::distance_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        for (const auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen) continue;

            char dist_str[32];
            sprintf_s(dist_str, "%.0fm", player.distance);

            const float label_size = ImGui::GetFontSize();
            ImVec2 text_size = ImGui::CalcTextSize(dist_str);

            int stack = 0;
            if (settings::nameesp && settings::nameside == settings::distanceside) stack++;
            if (settings::toolesp && settings::toolside == settings::distanceside) stack++;

            ImVec2 dist_pos = render::label_position(settings::distanceside,
                player.top_left, player.bottom_right, text_size, stack);

            ImU32 base_dist_color = uses_team_color(player.teammate) ? team_color() : settings_rgb(settings::distancecolor);
            ImU32 dist_color = render::distance_fade.apply_fade(base_dist_color);
            render::draw_text_outlined_freetype(dist_str, dist_pos, dist_color, label_size);
        }
    }

    void render_tools() {
        render::tool_fade.update(settings::toolesp);
        if (render::tool_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        for (const auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen) continue;

            std::string tool_display = player.tool_name.empty() ? "[None]" : player.tool_name;

            const float tool_font_size = ImGui::GetFontSize();
            ImVec2 text_size = ImGui::CalcTextSize(tool_display.c_str());

            int stack = 0;
            if (settings::nameesp && settings::nameside == settings::toolside) stack++;

            ImVec2 tool_pos = render::label_position(settings::toolside,
                player.top_left, player.bottom_right, text_size, stack);

            ImU32 base_color = (tool_display == "[None]") ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 255);
            if (uses_team_color(player.teammate)) base_color = team_color();
            ImU32 tool_color = render::tool_fade.apply_fade(base_color);
            render::draw_text_outlined_freetype(tool_display, tool_pos, tool_color, tool_font_size);
        }
    }



    void render_tracers() {
        render::tracer_fade.update(settings::tracers);
        if (render::tracer_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        if (cached_esp_players.empty()) return;

        for (const auto& player : cached_esp_players) {
            if (!player.valid || !player.on_screen) continue;
            if (uses_team_color(player.teammate)) {
                render::draw_tracer(player.screen_position, team_color());
            }
            else {
                render::draw_tracer(player.screen_position);
            }
        }
    }

    void render_offscreen_arrows() {
        render::arrow_fade.update(settings::offscreenarrows);
        if (render::arrow_fade.current_alpha <= 0.01f) return;

        std::vector<ESPPlayerData> cached_esp_players = player_access::get_cached_esp_players();
        Vector3 cached_camera_position = player_access::get_cached_camera_position();

        if (cached_esp_players.empty()) return;

        for (const auto& player : cached_esp_players) {
            if (!player.valid || player.on_screen) continue;
            render::draw_offscreen_arrow(player.position, cached_camera_position, player.name, player.distance);
        }
    }

    void render_china_hats() {
        render::chinahat_fade.update(settings::chinahats);
        if (render::chinahat_fade.current_alpha <= 0.01f) return;

        std::vector<ChinaHatRenderData> cached_china_hats = get_cached_china_hat_render_data();
        if (cached_china_hats.empty()) return;

        for (const auto& hat_data : cached_china_hats) {
            render::draw_china_hat_cached(hat_data, render::chinahat_fade.current_alpha);
        }
    }

    void render_skeletons() {
        render::skeleton_fade.update(settings::skeletons);
        if (render::skeleton_fade.current_alpha <= 0.01f) return;

        std::vector<SkeletonRenderData> cached_skeletons = get_cached_skeleton_render_data();
        if (cached_skeletons.empty()) return;

        for (const auto& skel_data : cached_skeletons) {
            render::draw_skeleton_cached(skel_data, render::skeleton_fade.current_alpha,
                uses_team_color(skel_data.teammate) ? team_color() : 0);
        }
    }



    void run_esp() {
        if (!settings::espenabled) return;

        player_trail::render_trail();
        render_china_hats();
        //globals::workspace.findfirstchild("Nodes")
        //"Stone_Node"
        //"Metal_Node"
        //"Phosphate_Node"

        //globals::workspace.findfirstchild("Nodes")
        //"Stone_Node"
        //"Metal_Node"
        //"Phosphate_Node"


        render_chams();
        //render_nodes(); // metal // stone // sulfur // 
        //render_trees();
        //render_pickables();
        //render_pickables();
        coolio::render_animal_esp();
        coolio::render_resource_esp();
        render_boxes();
        render_names();
        render_healthbars();
        render_distance();
        render_tools();
        render_skeletons();
        render_tracers();
        render_offscreen_arrows();


    }
}