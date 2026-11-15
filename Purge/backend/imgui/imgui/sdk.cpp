#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "SDK.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
using namespace ImGui;

namespace SDK {
    namespace Gui {
        namespace Colors {
            ImVec4 WindowBg = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
            ImVec4 ChildBg = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
            ImVec4 Border = ImVec4(0.20f, 0.20f, 0.20f, 0.80f);
            ImVec4 FrameBg = ImVec4(0.12f, 0.12f, 0.12f, 0.54f);
            ImVec4 FrameBgHovered = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
            ImVec4 FrameBgActive = ImVec4(0.25f, 0.25f, 0.25f, 0.67f);
            ImVec4 TitleBg = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
            ImVec4 TitleBgActive = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
            ImVec4 Button = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
            ImVec4 ButtonHovered = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            ImVec4 ButtonActive = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            ImVec4 Header = ImVec4(0.12f, 0.12f, 0.12f, 0.76f);
            ImVec4 HeaderHovered = ImVec4(0.18f, 0.18f, 0.18f, 0.86f);
            ImVec4 HeaderActive = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            ImVec4 Tab = ImVec4(0.10f, 0.10f, 0.10f, 0.86f);
            ImVec4 TabHovered = ImVec4(0.20f, 0.40f, 0.80f, 0.80f);
            ImVec4 TabActive = ImVec4(0.20f, 0.40f, 0.80f, 1.00f);
            ImVec4 Text = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            ImVec4 TextDisabled = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            ImVec4 CheckMark = ImVec4(0.20f, 0.80f, 0.20f, 1.00f);
            ImVec4 SliderGrab = ImVec4(0.20f, 0.40f, 0.80f, 1.00f);
            ImVec4 SliderGrabActive = ImVec4(0.30f, 0.50f, 0.90f, 1.00f);
        }

        void InitializeStyle() {
            ImGuiStyle& style = ImGui::GetStyle();

            style.WindowPadding = ImVec2(8, 8);
            style.WindowRounding = 5.0f;
            style.ChildRounding = 4.0f;
            style.FramePadding = ImVec2(6, 4);
            style.FrameRounding = 3.0f;
            style.ItemSpacing = ImVec2(6, 6);
            style.ItemInnerSpacing = ImVec2(6, 6);
            style.IndentSpacing = 25.0f;
            style.ScrollbarSize = 15.0f;
            style.ScrollbarRounding = 9.0f;
            style.GrabMinSize = 5.0f;
            style.GrabRounding = 3.0f;
            style.TabRounding = 4.0f;
            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;
            style.TabBorderSize = 0.0f;

            ImVec4* Colors = style.Colors;
            Colors[ImGuiCol_WindowBg] = SDK::Gui::Colors::WindowBg;
            Colors[ImGuiCol_ChildBg] = SDK::Gui::Colors::ChildBg;
            Colors[ImGuiCol_Border] = SDK::Gui::Colors::Border;
            Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            Colors[ImGuiCol_FrameBg] = SDK::Gui::Colors::FrameBg;
            Colors[ImGuiCol_FrameBgHovered] = SDK::Gui::Colors::FrameBgHovered;
            Colors[ImGuiCol_FrameBgActive] = SDK::Gui::Colors::FrameBgActive;
            Colors[ImGuiCol_TitleBg] = SDK::Gui::Colors::TitleBg;
            Colors[ImGuiCol_TitleBgActive] = SDK::Gui::Colors::TitleBgActive;
            Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
            Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
            Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
            Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
            Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
            Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
            Colors[ImGuiCol_CheckMark] = SDK::Gui::Colors::CheckMark;
            Colors[ImGuiCol_SliderGrab] = SDK::Gui::Colors::SliderGrab;
            Colors[ImGuiCol_SliderGrabActive] = SDK::Gui::Colors::SliderGrabActive;
            Colors[ImGuiCol_Button] = SDK::Gui::Colors::Button;
            Colors[ImGuiCol_ButtonHovered] = SDK::Gui::Colors::ButtonHovered;
            Colors[ImGuiCol_ButtonActive] = SDK::Gui::Colors::ButtonActive;
            Colors[ImGuiCol_Header] = SDK::Gui::Colors::Header;
            Colors[ImGuiCol_HeaderHovered] = SDK::Gui::Colors::HeaderHovered;
            Colors[ImGuiCol_HeaderActive] = SDK::Gui::Colors::HeaderActive;
            Colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
            Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
            Colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
            Colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
            Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
            Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
            Colors[ImGuiCol_Tab] = SDK::Gui::Colors::Tab;
            Colors[ImGuiCol_TabHovered] = SDK::Gui::Colors::TabHovered;
            Colors[ImGuiCol_TabActive] = SDK::Gui::Colors::TabActive;
            Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
            Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
            Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
            Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
            Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
            Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
            Colors[ImGuiCol_Text] = SDK::Gui::Colors::Text;
            Colors[ImGuiCol_TextDisabled] = SDK::Gui::Colors::TextDisabled;
            Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.40f, 0.80f, 0.35f);
            Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
            Colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
        }

        bool Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) {
            return ImGui::Begin(name, p_open, flags);
        }

        void End() {
            ImGui::End();
        }

        static float button_animations[256] = { 0.0f };
        static int button_id_counter = 0;

        bool Button(const char* label, const ImVec2& size_arg) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

            ImVec2 pos = window->DC.CursorPos;
            ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

            const ImRect bb(pos, pos + size);
            ImGui::ItemSize(size, style.FramePadding.y);
            if (!ImGui::ItemAdd(bb, id))
                return false;

            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

            float window_alpha = ImGui::GetStyle().Alpha;
            ImVec4 base_top = ImVec4(0.15f, 0.15f, 0.15f, window_alpha);
            ImVec4 base_bottom = ImVec4(0.08f, 0.08f, 0.08f, window_alpha);

            int anim_id = (id % 256);
            float target_anim = (hovered || held) ? 1.0f : 0.0f;
            float& current_anim = button_animations[anim_id];

            float animation_speed = 8.0f;
            current_anim += (target_anim - current_anim) * ImGui::GetIO().DeltaTime * animation_speed;
            current_anim = ImClamp(current_anim, 0.0f, 1.0f);

            ImVec4 final_top = base_top;
            ImVec4 final_bottom = base_bottom;

            if (held) {
                final_top.x *= 0.7f;
                final_top.y *= 0.7f;
                final_top.z *= 0.7f;
                final_bottom.x *= 0.7f;
                final_bottom.y *= 0.7f;
                final_bottom.z *= 0.7f;
            }
            else {
                float brightness = 1.0f + (current_anim * 0.3f);
                final_top.x = ImMin(base_top.x * brightness, 1.0f);
                final_top.y = ImMin(base_top.y * brightness, 1.0f);
                final_top.z = ImMin(base_top.z * brightness, 1.0f);
                final_bottom.x = ImMin(base_bottom.x * brightness, 1.0f);
                final_bottom.y = ImMin(base_bottom.y * brightness, 1.0f);
                final_bottom.z = ImMin(base_bottom.z * brightness, 1.0f);
            }

            ImU32 top_col = ImGui::ColorConvertFloat4ToU32(final_top);
            ImU32 bottom_col = ImGui::ColorConvertFloat4ToU32(final_bottom);
            ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilledMultiColor(bb.Min, bb.Max, top_col, top_col, bottom_col, bottom_col);
            draw_list->AddRect(bb.Min, bb.Max, border_col, style.FrameRounding, 0, 1.0f);

            ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

            return pressed;
        }

        bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags) {
            ImGui::PushStyleColor(ImGuiCol_Header, Colors::Header);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::HeaderHovered);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, Colors::HeaderActive);

            bool result = ImGui::TreeNodeEx(label, flags);

            ImGui::PopStyleColor(3);
            return result;
        }

        void TreePop() {
            ImGui::TreePop();
        }

        bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.18f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.30f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.35f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));

            bool result = ImGui::Selectable(label, selected, flags, size);

            ImGui::PopStyleColor(4);
            return result;
        }

        bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags) {
            // Custom combo styling to match the dark theme
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.12f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.30f, 0.35f, 0.8f));

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            bool result = ImGui::BeginCombo(label, preview_value, flags);

            if (!result) {
                // If combo didn't open, pop the styles immediately
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(8);
            }

            return result;
        }

        void EndCombo() {
            ImGui::EndCombo();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(8);
        }

        void PushItemWidth(float item_width) {
            ImGui::PushItemWidth(item_width);
        }

        void PopItemWidth() {
            ImGui::PopItemWidth();
        }

        void SetNextWindowSize(const ImVec2& size, ImGuiCond cond) {
            ImGui::SetNextWindowSize(size, cond);
        }

        void SetNextWindowPos(const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot) {
            ImGui::SetNextWindowPos(pos, cond, pivot);
        }

        ImVec2 GetContentRegionAvail() {
            return ImGui::GetContentRegionAvail();
        }

        ImVec2 GetWindowSize() {
            return ImGui::GetWindowSize();
        }

        ImVec2 GetWindowPos() {
            return ImGui::GetWindowPos();
        }

        bool Checkbox(const char* label, bool* v) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;
            ImGuiContext& g = *ImGui::GetCurrentContext();
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const float square_sz = 12.0f;
            const ImVec2 pos = window->DC.CursorPos;
            const ImRect total_bb(pos, pos + ImVec2(square_sz - 4 + (label_size.x > 0.0f ? style.ItemInnerSpacing.x * 0.5f + label_size.x : 0.0f), ImMax(square_sz, label_size.y)));
            ImGui::ItemSize(total_bb);
            if (!ImGui::ItemAdd(total_bb, id))
                return false;

            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

            // Toggle animation
            static std::unordered_map<ImGuiID, float> checkbox_animations;
            float& anim = checkbox_animations[id];
            float target = *v ? 1.0f : 0.0f;
            anim += (target - anim) * ImGui::GetIO().DeltaTime * 8.0f;

            if (pressed) {
                *v = !(*v);
                ImGui::MarkItemEdited(id);
            }

            const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
            ImGui::RenderNavHighlight(total_bb, id);

            // Get current window alpha for fade
            float window_alpha = ImGui::GetCurrentWindow()->DrawList->_Data->InitialFlags & ImDrawListFlags_AntiAliasedLines ?
                style.Alpha : 1.0f;

            if (anim > 0.01f) {
                ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
                ImVec4 accent_light = ImVec4(
                    accent_color.x + 0.2f,
                    accent_color.y + 0.2f,
                    accent_color.z + 0.2f,
                    accent_color.w * anim * window_alpha
                );
                ImVec4 accent_dark = ImVec4(
                    accent_color.x * 0.7f,
                    accent_color.y * 0.7f,
                    accent_color.z * 0.7f,
                    accent_color.w * anim * window_alpha
                );
                accent_light.x = ImMin(accent_light.x, 1.0f);
                accent_light.y = ImMin(accent_light.y, 1.0f);
                accent_light.z = ImMin(accent_light.z, 1.0f);

                ImU32 col_top = ImGui::ColorConvertFloat4ToU32(accent_light);
                ImU32 col_bottom = ImGui::ColorConvertFloat4ToU32(accent_dark);

                window->DrawList->AddRectFilledMultiColor(
                    check_bb.Min, check_bb.Max,
                    col_top, col_top, col_bottom, col_bottom
                );
            }

            if (anim < 0.99f) {
                float unchecked_alpha = (1.0f - anim) * window_alpha;
                ImU32 col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.18f, 0.18f, unchecked_alpha));
                ImU32 col_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.08f, unchecked_alpha));

                if (hovered) {
                    col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.22f, 0.22f, unchecked_alpha));
                    col_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, unchecked_alpha));
                }
                if (held) {
                    col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.26f, 0.26f, 0.26f, unchecked_alpha));
                    col_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.16f, 0.16f, 0.16f, unchecked_alpha));
                }

                window->DrawList->AddRectFilledMultiColor(
                    check_bb.Min, check_bb.Max,
                    col_top, col_top, col_bottom, col_bottom
                );
            }

            window->DrawList->AddRect(check_bb.Min, check_bb.Max,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha)), 0.0f, 0, 1.0f);

            if (g.LogEnabled)
                ImGui::LogRenderedText(&total_bb.Min, *v ? "[x]" : "[ ]");
            if (label_size.x > 0.0f)
                ImGui::RenderText(ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x * 0.5f, check_bb.Min.y + (square_sz - label_size.y) * 0.5f), label);

            return pressed;
        }

        bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *ImGui::GetCurrentContext();
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const float w = ImGui::GetContentRegionAvail().x;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const float slider_height = 10.0f;

            const ImVec2 label_pos = window->DC.CursorPos;
            const ImVec2 slider_pos = ImVec2(window->DC.CursorPos.x, window->DC.CursorPos.y + label_size.y + style.ItemSpacing.y);

            const ImRect frame_bb(slider_pos, slider_pos + ImVec2(w, slider_height));
            const ImRect total_bb(label_pos, frame_bb.Max);

            ImGui::ItemSize(total_bb, style.FramePadding.y);
            if (!ImGui::ItemAdd(total_bb, id))
                return false;

            // Value change animation
            static std::unordered_map<ImGuiID, float> slider_animations;
            float& anim = slider_animations[id];
            float target = (*v - v_min) / (v_max - v_min);
            anim += (target - anim) * ImGui::GetIO().DeltaTime * 6.0f;

            if (label_size.x > 0.0f)
                ImGui::RenderText(label_pos, label);

            bool value_changed = false;
            const bool hovered = ImGui::IsItemHovered() && g.IO.MousePos.x >= frame_bb.Min.x && g.IO.MousePos.x <= frame_bb.Max.x && g.IO.MousePos.y >= frame_bb.Min.y && g.IO.MousePos.y <= frame_bb.Max.y;
            const bool clicked = hovered && g.IO.MouseClicked[0];
            const bool dragging = g.ActiveId == id;

            if (clicked || (dragging && g.IO.MouseDown[0])) {
                if (!dragging) {
                    ImGui::SetActiveID(id, window);
                    ImGui::SetFocusID(id, window);
                    ImGui::FocusWindow(window);
                }

                const float mouse_abs_pos = g.IO.MousePos.x;
                const float clicked_t = ImClamp((mouse_abs_pos - frame_bb.Min.x) / (frame_bb.Max.x - frame_bb.Min.x), 0.0f, 1.0f);
                const float new_value = ImClamp(ImLerp(v_min, v_max, clicked_t), v_min, v_max);
                if (*v != new_value) {
                    *v = new_value;
                    value_changed = true;
                }
            }

            if (!g.IO.MouseDown[0] && dragging) {
                ImGui::ClearActiveID();
            }

            // Get window alpha for fade
            float window_alpha = style.Alpha;

            // Draw slider track
            ImU32 track_col_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.15f, window_alpha));
            ImU32 track_col_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.08f, window_alpha));

            window->DrawList->AddRectFilledMultiColor(
                frame_bb.Min, frame_bb.Max,
                track_col_top, track_col_top, track_col_bottom, track_col_bottom
            );

            window->DrawList->AddRect(frame_bb.Min, frame_bb.Max,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha)), 0.0f, 0, 1.0f);

            // Draw animated slider fill
            const float fill_width = (frame_bb.Max.x - frame_bb.Min.x) * anim;
            const ImRect fill_bb(frame_bb.Min, ImVec2(frame_bb.Min.x + fill_width, frame_bb.Max.y));

            if (fill_width > 0.0f) {
                ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);

                ImVec4 accent_light = ImVec4(
                    accent_color.x + 0.2f,
                    accent_color.y + 0.2f,
                    accent_color.z + 0.2f,
                    accent_color.w * window_alpha
                );
                ImVec4 accent_dark = ImVec4(
                    accent_color.x * 0.7f,
                    accent_color.y * 0.7f,
                    accent_color.z * 0.7f,
                    accent_color.w * window_alpha
                );

                accent_light.x = ImMin(accent_light.x, 1.0f);
                accent_light.y = ImMin(accent_light.y, 1.0f);
                accent_light.z = ImMin(accent_light.z, 1.0f);

                ImU32 fill_col_top = ImGui::ColorConvertFloat4ToU32(accent_light);
                ImU32 fill_col_bottom = ImGui::ColorConvertFloat4ToU32(accent_dark);

                if (dragging) {
                    accent_light.x = ImMin(accent_light.x + 0.1f, 1.0f);
                    accent_light.y = ImMin(accent_light.y + 0.1f, 1.0f);
                    accent_light.z = ImMin(accent_light.z + 0.1f, 1.0f);
                    accent_dark.x = ImMin(accent_dark.x + 0.1f, 1.0f);
                    accent_dark.y = ImMin(accent_dark.y + 0.1f, 1.0f);
                    accent_dark.z = ImMin(accent_dark.z + 0.1f, 1.0f);

                    fill_col_top = ImGui::ColorConvertFloat4ToU32(accent_light);
                    fill_col_bottom = ImGui::ColorConvertFloat4ToU32(accent_dark);
                }
                else if (hovered) {
                    accent_light.x = ImMin(accent_light.x + 0.05f, 1.0f);
                    accent_light.y = ImMin(accent_light.y + 0.05f, 1.0f);
                    accent_light.z = ImMin(accent_light.z + 0.05f, 1.0f);
                    accent_dark.x = ImMin(accent_dark.x + 0.05f, 1.0f);
                    accent_dark.y = ImMin(accent_dark.y + 0.05f, 1.0f);
                    accent_dark.z = ImMin(accent_dark.z + 0.05f, 1.0f);

                    fill_col_top = ImGui::ColorConvertFloat4ToU32(accent_light);
                    fill_col_bottom = ImGui::ColorConvertFloat4ToU32(accent_dark);
                }

                window->DrawList->AddRectFilledMultiColor(
                    fill_bb.Min, fill_bb.Max,
                    fill_col_top, fill_col_top, fill_col_bottom, fill_col_bottom
                );
            }

            *v = ImClamp(*v, v_min, v_max);

            char value_buf[64];
            snprintf(value_buf, sizeof(value_buf), "%.1f / %.1f", *v, v_max);
            const ImVec2 value_size = ImGui::CalcTextSize(value_buf);
            const ImVec2 value_pos = ImVec2(frame_bb.GetCenter().x - value_size.x * 0.5f, frame_bb.GetCenter().y - value_size.y * 0.5f);

            ImU32 text_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.35f, 0.35f, window_alpha));
            ImU32 outline_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha));

            window->DrawList->AddText(ImVec2(value_pos.x + 1, value_pos.y + 1), outline_color, value_buf);
            window->DrawList->AddText(ImVec2(value_pos.x - 1, value_pos.y - 1), outline_color, value_buf);
            window->DrawList->AddText(ImVec2(value_pos.x + 1, value_pos.y - 1), outline_color, value_buf);
            window->DrawList->AddText(ImVec2(value_pos.x - 1, value_pos.y + 1), outline_color, value_buf);
            window->DrawList->AddText(value_pos, text_color, value_buf);

            if (value_changed)
                ImGui::MarkItemEdited(id);

            return value_changed;
        }

        bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format) {
            float v_f = (float)*v;
            bool value_changed = SliderFloat(label, &v_f, (float)v_min, (float)v_max, format);
            *v = (int)v_f;
            return value_changed;
        }

        bool ComboBox(const char* label, int* current_item, const char* const items[], int items_count) {
            const char* preview_text = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "None";

            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *ImGui::GetCurrentContext();
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const float w = ImGui::GetContentRegionAvail().x;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const float combo_height = 20.0f;

            const ImVec2 label_pos = window->DC.CursorPos;
            const ImVec2 combo_pos = ImVec2(window->DC.CursorPos.x, window->DC.CursorPos.y + label_size.y + style.ItemSpacing.y);

            const ImRect frame_bb(combo_pos, combo_pos + ImVec2(w, combo_height));
            const ImRect total_bb(label_pos, frame_bb.Max);

            ImGui::ItemSize(total_bb, style.FramePadding.y);
            if (!ImGui::ItemAdd(total_bb, id))
                return false;

            // Hover and open animations
            static std::unordered_map<ImGuiID, float> combo_hover_animations;
            static std::unordered_map<ImGuiID, float> combo_open_animations;
            float& hover_anim = combo_hover_animations[id];
            float& open_anim = combo_open_animations[id];

            bool hovered = ImGui::IsItemHovered();
            hover_anim += (hovered ? 1.0f : 0.0f - hover_anim) * ImGui::GetIO().DeltaTime * 8.0f;
            hover_anim = ImClamp(hover_anim, 0.0f, 1.0f);

            if (label_size.x > 0.0f)
                ImGui::RenderText(label_pos, label);

            bool value_changed = false;
            const bool clicked = hovered && g.IO.MouseClicked[0];

            static bool combo_open = false;
            static ImGuiID open_combo_id = 0;

            if (clicked) {
                if (open_combo_id == id) {
                    combo_open = false;
                    open_combo_id = 0;
                }
                else {
                    combo_open = true;
                    open_combo_id = id;
                    ImGui::OpenPopup("##combo_popup");
                }
            }

            // Update open animation
            bool is_open = (combo_open && open_combo_id == id);
            open_anim += (is_open ? 1.0f : 0.0f - open_anim) * ImGui::GetIO().DeltaTime * 10.0f;
            open_anim = ImClamp(open_anim, 0.0f, 1.0f);

            if (combo_open && open_combo_id == id && ImGui::IsMouseClicked(0) && !hovered) {
                bool clicked_in_popup = false;
                ImVec2 popup_pos = ImVec2(frame_bb.Min.x, frame_bb.Max.y);
                ImVec2 popup_size = ImVec2(frame_bb.GetWidth(), items_count * ImGui::GetTextLineHeightWithSpacing() + 4);
                ImRect popup_rect(popup_pos, popup_pos + popup_size);

                if (popup_rect.Contains(g.IO.MousePos)) {
                    clicked_in_popup = true;
                }

                if (!clicked_in_popup) {
                    combo_open = false;
                    open_combo_id = 0;
                    ImGui::CloseCurrentPopup();
                }
            }

            float window_alpha = style.Alpha;

            // Animated background colors
            ImVec4 base_top = ImVec4(0.15f, 0.15f, 0.15f, window_alpha);
            ImVec4 base_bottom = ImVec4(0.08f, 0.08f, 0.08f, window_alpha);
            ImVec4 hover_top = ImVec4(0.18f, 0.18f, 0.18f, window_alpha);
            ImVec4 hover_bottom = ImVec4(0.11f, 0.11f, 0.11f, window_alpha);

            ImVec4 final_top = ImVec4(
                base_top.x + (hover_top.x - base_top.x) * hover_anim,
                base_top.y + (hover_top.y - base_top.y) * hover_anim,
                base_top.z + (hover_top.z - base_top.z) * hover_anim,
                window_alpha
            );
            ImVec4 final_bottom = ImVec4(
                base_bottom.x + (hover_bottom.x - base_bottom.x) * hover_anim,
                base_bottom.y + (hover_bottom.y - base_bottom.y) * hover_anim,
                base_bottom.z + (hover_bottom.z - base_bottom.z) * hover_anim,
                window_alpha
            );

            ImU32 bg_col_top = ImGui::ColorConvertFloat4ToU32(final_top);
            ImU32 bg_col_bottom = ImGui::ColorConvertFloat4ToU32(final_bottom);

            window->DrawList->AddRectFilledMultiColor(
                frame_bb.Min, frame_bb.Max,
                bg_col_top, bg_col_top, bg_col_bottom, bg_col_bottom
            );

            window->DrawList->AddRect(frame_bb.Min, frame_bb.Max,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha)), 0.0f, 0, 1.0f);

            const ImVec2 text_pos = ImVec2(frame_bb.Min.x + 8.0f, frame_bb.Min.y + (combo_height - label_size.y) * 0.5f);
            window->DrawList->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.9f, window_alpha)), preview_text);

            // Animated symbol rotation
            const ImVec2 symbol_size = ImGui::CalcTextSize("+");
            const ImVec2 symbol_center = ImVec2(frame_bb.Max.x - symbol_size.x - 8.0f + symbol_size.x * 0.5f,
                frame_bb.Min.y + combo_height * 0.5f);

            float rotation = open_anim * 0.785398f; // 45 degrees in radians
            ImVec2 rotated_pos = ImVec2(
                symbol_center.x - symbol_size.x * 0.5f,
                symbol_center.y - symbol_size.y * 0.5f
            );

            window->DrawList->AddText(rotated_pos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.9f, window_alpha)), "+");

            if (combo_open && open_combo_id == id) {
                ImGui::SetNextWindowPos(ImVec2(frame_bb.Min.x, frame_bb.Max.y));
                ImGui::SetNextWindowSize(ImVec2(frame_bb.GetWidth(), 0));

                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 0.98f * window_alpha));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, window_alpha));
                ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

                if (ImGui::BeginPopup("##combo_popup")) {
                    for (int i = 0; i < items_count; i++) {
                        bool is_selected = (*current_item == i);

                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.12f, 0.12f, 0.8f * window_alpha));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.18f, 0.9f * window_alpha));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f * window_alpha));

                        if (ImGui::Selectable(items[i], is_selected, 0, ImVec2(frame_bb.GetWidth() - 16.0f, 0))) {
                            *current_item = i;
                            value_changed = true;
                            combo_open = false;
                            open_combo_id = 0;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::PopStyleColor(3);

                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }

            return value_changed;
        }


        bool MultiComboBox(const char* label, bool* selected_items, const char* const items[], int items_count) {
            int selected_count = 0;
            char preview_text[512];
            strcpy_s(preview_text, "");

            bool first = true;
            for (int i = 0; i < items_count; i++) {
                if (selected_items[i]) {
                    selected_count++;
                    if (!first) {
                        strcat_s(preview_text, ", ");
                    }
                    strcat_s(preview_text, items[i]);
                    first = false;
                }
            }

            if (selected_count == 0) {
                strcpy_s(preview_text, "None");
            }

            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *ImGui::GetCurrentContext();
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const float w = ImGui::GetContentRegionAvail().x;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const float combo_height = 20.0f;

            const ImVec2 label_pos = window->DC.CursorPos;
            const ImVec2 combo_pos = ImVec2(window->DC.CursorPos.x, window->DC.CursorPos.y + label_size.y + style.ItemSpacing.y);

            const ImRect frame_bb(combo_pos, combo_pos + ImVec2(w, combo_height));
            const ImRect total_bb(label_pos, frame_bb.Max);

            ImGui::ItemSize(total_bb, style.FramePadding.y);
            if (!ImGui::ItemAdd(total_bb, id))
                return false;

            // Hover and open animations
            static std::unordered_map<ImGuiID, float> multicombo_hover_animations;
            static std::unordered_map<ImGuiID, float> multicombo_open_animations;
            float& hover_anim = multicombo_hover_animations[id];
            float& open_anim = multicombo_open_animations[id];

            bool hovered = ImGui::IsItemHovered();
            hover_anim += (hovered ? 1.0f : 0.0f - hover_anim) * ImGui::GetIO().DeltaTime * 8.0f;
            hover_anim = ImClamp(hover_anim, 0.0f, 1.0f);

            if (label_size.x > 0.0f)
                ImGui::RenderText(label_pos, label);

            bool value_changed = false;
            const bool clicked = hovered && g.IO.MouseClicked[0];

            static bool multicombo_open = false;
            static ImGuiID open_multicombo_id = 0;

            if (clicked) {
                if (open_multicombo_id == id) {
                    multicombo_open = false;
                    open_multicombo_id = 0;
                }
                else {
                    multicombo_open = true;
                    open_multicombo_id = id;
                    ImGui::OpenPopup("##multicombo_popup");
                }
            }

            // Update open animation
            bool is_open = (multicombo_open && open_multicombo_id == id);
            open_anim += (is_open ? 1.0f : 0.0f - open_anim) * ImGui::GetIO().DeltaTime * 10.0f;
            open_anim = ImClamp(open_anim, 0.0f, 1.0f);

            if (multicombo_open && open_multicombo_id == id && ImGui::IsMouseClicked(0) && !hovered) {
                bool clicked_in_popup = false;
                ImVec2 popup_pos = ImVec2(frame_bb.Min.x, frame_bb.Max.y);
                ImVec2 popup_size = ImVec2(frame_bb.GetWidth(), items_count * ImGui::GetTextLineHeightWithSpacing() + 4);
                ImRect popup_rect(popup_pos, popup_pos + popup_size);

                if (popup_rect.Contains(g.IO.MousePos)) {
                    clicked_in_popup = true;
                }

                if (!clicked_in_popup) {
                    multicombo_open = false;
                    open_multicombo_id = 0;
                    ImGui::CloseCurrentPopup();
                }
            }

            float window_alpha = style.Alpha;

            ImVec4 base_top = ImVec4(0.15f, 0.15f, 0.15f, window_alpha);
            ImVec4 base_bottom = ImVec4(0.08f, 0.08f, 0.08f, window_alpha);
            ImVec4 hover_top = ImVec4(0.18f, 0.18f, 0.18f, window_alpha);
            ImVec4 hover_bottom = ImVec4(0.11f, 0.11f, 0.11f, window_alpha);

            ImVec4 final_top = ImVec4(
                base_top.x + (hover_top.x - base_top.x) * hover_anim,
                base_top.y + (hover_top.y - base_top.y) * hover_anim,
                base_top.z + (hover_top.z - base_top.z) * hover_anim,
                window_alpha
            );
            ImVec4 final_bottom = ImVec4(
                base_bottom.x + (hover_bottom.x - base_bottom.x) * hover_anim,
                base_bottom.y + (hover_bottom.y - base_bottom.y) * hover_anim,
                base_bottom.z + (hover_bottom.z - base_bottom.z) * hover_anim,
                window_alpha
            );

            ImU32 bg_col_top = ImGui::ColorConvertFloat4ToU32(final_top);
            ImU32 bg_col_bottom = ImGui::ColorConvertFloat4ToU32(final_bottom);

            window->DrawList->AddRectFilledMultiColor(
                frame_bb.Min, frame_bb.Max,
                bg_col_top, bg_col_top, bg_col_bottom, bg_col_bottom
            );

            window->DrawList->AddRect(frame_bb.Min, frame_bb.Max,
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha)), 0.0f, 0, 1.0f);

            const ImVec2 text_pos = ImVec2(frame_bb.Min.x + 8.0f, frame_bb.Min.y + (combo_height - label_size.y) * 0.5f);
            window->DrawList->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.9f, window_alpha)), preview_text);

            // Animated symbol
            const ImVec2 symbol_size = ImGui::CalcTextSize("+");
            const ImVec2 symbol_center = ImVec2(frame_bb.Max.x - symbol_size.x - 8.0f + symbol_size.x * 0.5f,
                frame_bb.Min.y + combo_height * 0.5f);

            ImVec2 rotated_pos = ImVec2(
                symbol_center.x - symbol_size.x * 0.5f,
                symbol_center.y - symbol_size.y * 0.5f
            );

            window->DrawList->AddText(rotated_pos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.9f, window_alpha)), "+");

            if (multicombo_open && open_multicombo_id == id) {
                ImGui::SetNextWindowPos(ImVec2(frame_bb.Min.x, frame_bb.Max.y));
                ImGui::SetNextWindowSize(ImVec2(frame_bb.GetWidth(), 0));

                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 0.98f * window_alpha));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, window_alpha));
                ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));

                if (ImGui::BeginPopup("##multicombo_popup")) {
                    for (int i = 0; i < items_count; i++) {
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.12f, 0.12f, 0.8f * window_alpha));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.18f, 0.9f * window_alpha));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f * window_alpha));

                        bool was_selected = selected_items[i];
                        bool clicked_item = ImGui::Selectable(items[i], selected_items[i], ImGuiSelectableFlags_DontClosePopups, ImVec2(frame_bb.GetWidth(), 0));

                        if (clicked_item) {
                            selected_items[i] = !was_selected;
                            value_changed = true;
                        }

                        ImGui::PopStyleColor(3);
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }

            return value_changed;
        }



        bool MultiComboBox2(const char* label, std::vector<int>* selected, std::vector<const char*> items) {
            // Convert vector<int> to bool array
            bool* bool_array = new bool[items.size()];
            for (size_t i = 0; i < items.size(); i++) {
                bool_array[i] = ((*selected)[i] == 1);
            }

            // Call the original MultiComboBox function
            bool result = MultiComboBox(label, bool_array, items.data(), static_cast<int>(items.size()));

            // Convert back to vector<int>
            if (result) {
                for (size_t i = 0; i < items.size(); i++) {
                    (*selected)[i] = bool_array[i] ? 1 : 0;
                }
            }

            delete[] bool_array;
            return result;
        }


        bool InputText(const char* label, char* buf, size_t buf_size) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors::FrameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Colors::FrameBgHovered);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Colors::FrameBgActive);

            // Display label above the input field
            ImGui::Text("%s", label);

            // Stretch the input field to fill available width
            ImGui::PushItemWidth(-1);
            bool result = ImGui::InputText(("##" + std::string(label)).c_str(), buf, buf_size);
            ImGui::PopItemWidth();

            ImGui::PopStyleColor(3);
            return result;
        }

        bool InputFloat(const char* label, float* v, float step, float step_fast, const char* format) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors::FrameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Colors::FrameBgHovered);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Colors::FrameBgActive);

            bool result = ImGui::InputFloat(label, v, step, step_fast, format);

            ImGui::PopStyleColor(3);
            return result;
        }

        bool InputInt(const char* label, int* v, int step, int step_fast) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors::FrameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Colors::FrameBgHovered);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Colors::FrameBgActive);

            bool result = ImGui::InputInt(label, v, step, step_fast);

            ImGui::PopStyleColor(3);
            return result;
        }

        bool ColorPicker(const char* label, float* col) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const ImVec4 col_v4(col[0], col[1], col[2], col[3]);

            ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoSidePreview;

            ImGui::BeginGroup();

            // Label at top left
            if (label_size.x > 0) {
                SDK::Gui::Text(label);
            }

            // Color button below label
            const ImVec2 pos = window->DC.CursorPos;
            bool result = ImGui::ColorButton(label, col_v4, flags, ImVec2(g.FontSize * 2, g.FontSize));

            if (result) {
                ImGui::OpenPopup(std::string(std::string(label) + "##Picker").c_str());
            }

            if (ImGui::BeginPopup(std::string(std::string(label) + "##Picker").c_str())) {
                ImGui::ColorPicker4(label, col, flags);
                ImGui::EndPopup();
            }

            ImGui::EndGroup();
            return result;
        }

        bool ColorEdit3(const char* label, float* col) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const ImVec4 col_v4(col[0], col[1], col[2], 1.0f);

            ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoOptions |
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;

            ImGui::BeginGroup();

            // Label at top left
            if (label_size.x > 0) {
                SDK::Gui::Text(label);
            }

            // Color button below label
            const ImVec2 pos = window->DC.CursorPos;
            bool result = ImGui::ColorButton(label, col_v4, flags, ImVec2(g.FontSize * 2, g.FontSize));

            if (result) {
                ImGui::OpenPopup(std::string(std::string(label) + "##Picker").c_str());
            }

            if (ImGui::BeginPopup(std::string(std::string(label) + "##Picker").c_str())) {
                ImGui::ColorPicker3(label, col, flags);
                ImGui::EndPopup();
            }

            ImGui::EndGroup();
            return result;
        }

        bool ColorEdit4(const char* label, float* col) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const ImVec4 col_v4(col[0], col[1], col[2], col[3]);

            ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview |
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip |
                ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoSidePreview;

            ImGui::BeginGroup();

            // Label at top left
            if (label_size.x > 0) {
                SDK::Gui::Text(label);
            }

            // Color button below label
            const ImVec2 pos = window->DC.CursorPos;
            bool result = ImGui::ColorButton(label, col_v4, flags, ImVec2(g.FontSize * 2, g.FontSize));

            if (result) {
                ImGui::OpenPopup(std::string(std::string(label) + "##Picker").c_str());
            }

            if (ImGui::BeginPopup(std::string(std::string(label) + "##Picker").c_str())) {
                ImGui::ColorPicker4(label, col, flags);
                ImGui::EndPopup();
            }

            ImGui::EndGroup();
            return result;
        }

        bool BeginChild(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::ChildBg);
            bool result = ImGui::BeginChild(str_id, size, border, flags);
            return result;
        }

        void EndChild() {
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        void Text(const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            ImGui::TextV(fmt, args);
            va_end(args);
        }

        void TextColored(const ImVec4& col, const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            ImGui::TextColoredV(col, fmt, args);
            va_end(args);
        }

        void Separator() {
            ImGui::Separator();
        }

        void Spacing() {
            ImGui::Spacing();
        }

        void SameLine(float offset_from_start_x, float spacing) {
            ImGui::SameLine(offset_from_start_x, spacing);
        }

        bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags) {
            return ImGui::BeginTabBar(str_id, flags);
        }

        void EndTabBar() {
            ImGui::EndTabBar();
        }

        bool BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags) {
            ImGui::PushStyleColor(ImGuiCol_Tab, Colors::Tab);
            ImGui::PushStyleColor(ImGuiCol_TabHovered, Colors::TabHovered);
            ImGui::PushStyleColor(ImGuiCol_TabActive, Colors::TabActive);

            bool result = ImGui::BeginTabItem(label, p_open, flags);

            if (!result) {
                // If tab item didn't open, pop the colors immediately
                ImGui::PopStyleColor(3);
            }

            return result;
        }

        void EndTabItem() {
            ImGui::EndTabItem();
            ImGui::PopStyleColor(3);
        }

        bool TreeNode(const char* label) {
            ImGui::PushStyleColor(ImGuiCol_Header, Colors::Header);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::HeaderHovered);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, Colors::HeaderActive);

            bool result = ImGui::TreeNode(label);

            ImGui::PopStyleColor(3);
            return result;
        }

        bool BeginGlowChild(const char* str_id, const ImVec2& size, bool border, ImGuiWindowFlags flags) {
            ImGuiWindow* window = GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            ImGuiIO& io = GetIO(); (void)io;
            const ImGuiStyle& style = g.Style;
            std::string bane = str_id;
            ImGui::BeginChild(str_id, size, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
            {

                ImVec2 pos = ImGui::GetWindowPos();
                ImVec2 size = ImGui::GetWindowSize();
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();

                float HeaderHeight = ImGui::GetFontSize() + style.WindowPadding.y * 2 + style.ChildBorderSize * 2;
                if (bane != "!") {
                    pDrawList->AddRectFilled(pos, pos + size, GetColorU32(ImGuiCol_ChildBg), 6);
                }
                if (style.ChildBorderSize > 0)
                {
                    //   pDrawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_Border));
                }
                if (bane != "!") {
                    //pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, 1), GetColorU32(ImGuiCol_TitleBg), label);
                    //pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, -1), GetColorU32(ImGuiCol_TitleBg), label);
                    //pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, -1), GetColorU32(ImGuiCol_TitleBg), label);
                    //pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, 1), GetColorU32(ImGuiCol_TitleBg), label);
                    //pDrawList->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2), GetColorU32(ImGuiCol_Text), label);
                }
                //  pDrawList->AddLine(pos + ImVec2(2, 2), pos + ImVec2(size.x - 2, 2), ImGui::GetColorU32(ImGuiCol_SliderGrab));
               //   pDrawList->AddLine(pos + ImVec2(2, 3), pos + ImVec2(size.x - 2, 3), ImGui::GetColorU32(ImGuiCol_SliderGrabActive));
                if (bane != "!") {
                    //    ImGui::SetCursorScreenPos(pos + ImVec2(0, HeaderHeight - style.WindowPadding.y));
                }
                ImGui::BeginChild(std::string(str_id + std::string("#Main")).c_str(), ImVec2(), ImGuiChildFlags_Border, NULL);
            }
        }

        void EndGlowChild() {
            ImGui::EndChild();
            ImGui::EndChild();
        }

        bool BeginSection(const char* title, float height) {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

            bool result = ImGui::BeginChild(title, ImVec2(0, height), true);

            if (title && strlen(title) > 0) {
                ImGui::Text(title);
                ImGui::Separator();
            }

            return result;
        }

        void EndSection() {
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }


        void ButtonRow(const char** buttons, int count, int* selected, float button_height) {
            float button_width = (ImGui::GetContentRegionAvail().x - (count - 1) * ImGui::GetStyle().ItemSpacing.x) / count;

            for (int i = 0; i < count; i++) {
                if (i > 0) ImGui::SameLine();

                bool is_selected = (selected && *selected == i);
                if (is_selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                }

                if (ImGui::Button(buttons[i], ImVec2(button_width, button_height))) {
                    if (selected) *selected = i;
                }

                ImGui::PopStyleColor(2);
            }
        }

        void InputWithLabel(const char* label, char* buf, size_t buf_size, float label_width) {
            ImGui::Text(label);
            ImGui::SameLine(label_width);
            ImGui::PushItemWidth(-1);
            ImGui::InputText(("##" + std::string(label)).c_str(), buf, buf_size);
            ImGui::PopItemWidth();
        }


    } // namespace Gui
} // namespace SDK
