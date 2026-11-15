#pragma once

#include <windows.h>
#include <d3d11.h>
#include "../../backend/imgui/imgui/imgui.h"

#ifndef DXGI_SWAP_EFFECT_FLIP_DISCARD
#define DXGI_SWAP_EFFECT_FLIP_DISCARD ((DXGI_SWAP_EFFECT)4)
#endif

#ifndef D3D_FEATURE_LEVEL_11_1
#define D3D_FEATURE_LEVEL_11_1 ((D3D_FEATURE_LEVEL)0xb100)
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef ImGuiConfigFlags_ViewportsEnable
#define ImGuiConfigFlags_ViewportsEnable 0
#endif

#ifndef ImGuiConfigFlags_DpiEnableScaleViewports
#define ImGuiConfigFlags_DpiEnableScaleViewports 0 
#endif





#ifdef MISSING_IMGUI_BACKENDS
inline void ImGui_ImplWin32_EnableDpiAwareness() {}
inline bool ImGui_ImplWin32_Init(HWND) { return true; }
inline void ImGui_ImplWin32_Shutdown() {}
inline void ImGui_ImplWin32_NewFrame() {}
inline bool ImGui_ImplDX11_Init(ID3D11Device*, ID3D11DeviceContext*) { return true; }
inline void ImGui_ImplDX11_Shutdown() {}
inline void ImGui_ImplDX11_NewFrame() {}
inline void ImGui_ImplDX11_RenderDrawData(ImDrawData*) {}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif