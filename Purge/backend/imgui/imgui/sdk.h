#pragma once
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <string>
#include <cstring>
#include <vector>

namespace SDK {
    namespace Gui {
        void InitializeStyle();
        bool Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
        void End();
        bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
        bool Checkbox(const char* label, bool* v);
        bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");
        bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d");
        bool InputText(const char* label, char* buf, size_t buf_size);
        bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f");
        bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100);
        bool ComboBox(const char* label, int* current_item, const char* const items[], int items_count);
        bool MultiComboBox(const char* label, bool* selected_items, const char* const items[], int items_count);
        bool MultiComboBox2(const char* label, std::vector<int>* selected, std::vector<const char*> items);
        bool ColorPicker(const char* label, float* col);
        bool ColorEdit3(const char* label, float* col);
        bool ColorEdit4(const char* label, float* col);
        bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false, ImGuiWindowFlags flags = 0);
        void EndChild();
        void Text(const char* fmt, ...);
        void TextColored(const ImVec4& col, const char* fmt, ...);
        void Separator();
        void Spacing();
        void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
        bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags = 0);
        void EndTabBar();
        bool BeginTabItem(const char* label, bool* p_open = nullptr, ImGuiTabItemFlags flags = 0);
        void EndTabItem();
        bool TreeNode(const char* label);
        bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags = 0);
        void TreePop();
        bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));
        bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0);
        void EndCombo();
        void PushItemWidth(float item_width);
        void PopItemWidth();
        void SetNextWindowSize(const ImVec2& size, ImGuiCond cond = 0);
        void SetNextWindowPos(const ImVec2& pos, ImGuiCond cond = 0, const ImVec2& pivot = ImVec2(0, 0));
        ImVec2 GetContentRegionAvail();
        ImVec2 GetWindowSize();
        ImVec2 GetWindowPos();

        // Custom components
        bool BeginGlowChild(const char* str_id, const ImVec2& size, bool border = true, ImGuiWindowFlags flags = 0);
        void EndGlowChild();
        bool BeginSection(const char* title, float height = 0.0f);
        void EndSection();
        void ColoredSlider(const char* label, float* v, float v_min, float v_max, ImU32 color, const char* format = "%.0f");
        void ButtonRow(const char** buttons, int count, int* selected, float button_height = 25.0f);
        void InputWithLabel(const char* label, char* buf, size_t buf_size, float label_width = 100.0f);
        bool ColoredCheckbox(const char* label, bool* v, ImU32 check_color = IM_COL32(100, 200, 100, 255));

        namespace Colors {
            extern ImVec4 WindowBg;
            extern ImVec4 ChildBg;
            extern ImVec4 Border;
            extern ImVec4 FrameBg;
            extern ImVec4 FrameBgHovered;
            extern ImVec4 FrameBgActive;
            extern ImVec4 TitleBg;
            extern ImVec4 TitleBgActive;
            extern ImVec4 Button;
            extern ImVec4 ButtonHovered;
            extern ImVec4 ButtonActive;
            extern ImVec4 Header;
            extern ImVec4 HeaderHovered;
            extern ImVec4 HeaderActive;
            extern ImVec4 Tab;
            extern ImVec4 TabHovered;
            extern ImVec4 TabActive;
            extern ImVec4 Text;
            extern ImVec4 TextDisabled;
            extern ImVec4 CheckMark;
            extern ImVec4 SliderGrab;
            extern ImVec4 SliderGrabActive;
        }
    }
}
