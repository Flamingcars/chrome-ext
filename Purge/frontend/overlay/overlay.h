#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>
#include "../../backend/imgui/imgui/imgui.h"
#include "../../backend/imgui/imgui/backends/imgui_impl_dx11.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include "../../backend/imgui/imgui/imgui.h"
#include "../../backend/imgui/imgui/backends/imgui_impl_win32.h"
#include "../../backend/imgui/imgui/backends/imgui_impl_dx11.h"
#include "../../backend/imgui/imgui/imgui_internal.h"
#include "../../backend/imgui/imgui/addons/imgui_addons.h"
#include "../../backend/imgui/imgui/sdk.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include "../../backend/imgui/imgui/imgui.h"
#include "../../backend/imgui/imgui/backends/imgui_impl_dx11.h"
#include "../../backend/imgui/imgui/imgui_internal.h"
#include "../../backend/imgui/imgui/addons/imgui_addons.h"
#include "../../backend/imgui/imgui/sdk.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef ARRAYSIZE
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

struct keybind;

namespace  overlay {
    constexpr int OVERLAY_WIDTH = 525;
    constexpr int OVERLAY_HEIGHT = 600;
    constexpr float OVERLAY_OPACITY = 0.95f;
    inline bool ENABLE_VSYNC = false;
    constexpr int SWAP_CHAIN_BUFFERS = 2;
    constexpr float TARGET_FPS = 144.0f;
    constexpr float IDLE_TIMEOUT = 5.0f;
    constexpr float PERFORMANCE_UPDATE_INTERVAL = 1.0f;



    extern bool visible;
    extern std::atomic<bool> should_exit;
    inline void dex();
    inline void playerlist();
    struct PerformanceMetrics {
        float fps = 0.0f;
        float frame_time = 0.0f;
        float gpu_time = 0.0f;
        float cpu_usage = 0.0f;
        size_t memory_usage = 0;
        int frame_count = 0;
        std::chrono::high_resolution_clock::time_point last_update;
        std::chrono::high_resolution_clock::time_point last_frame_start;

        void Reset() {
            fps = frame_time = gpu_time = cpu_usage = 0.0f;
            memory_usage = frame_count = 0;
            last_update = last_frame_start = std::chrono::high_resolution_clock::now();
        }
    };

    struct AnimationState {
        float current_value = 0.0f;
        float target_value = 0.0f;
        float speed = 8.0f;
        int easing_type = 0;
        std::chrono::high_resolution_clock::time_point last_update;
        bool active = false;
        bool needs_update = false;

        void Reset(float value = 0.0f) {
            current_value = target_value = value;
            active = needs_update = false;
            last_update = std::chrono::high_resolution_clock::now();
        }
    };

    struct FontCache {
        ImFont* small_font = nullptr;
        ImFont* medium_font = nullptr;
        ImFont* large_font = nullptr;
        ImFont* logo_font = nullptr;
        ImFont* icon_font = nullptr;
        bool fonts_loaded = false;

        void Clear() {
            small_font = medium_font = large_font = logo_font = icon_font = nullptr;
            fonts_loaded = false;
        }
    };

    struct RenderState {
        bool needs_redraw = true;
        bool idle_mode = false;
        float fade_alpha = 0.0f;
        std::chrono::high_resolution_clock::time_point last_input_time;
        std::chrono::high_resolution_clock::time_point last_render_time;

        void MarkInputReceived() {
            last_input_time = std::chrono::high_resolution_clock::now();
            idle_mode = false;
            needs_redraw = true;
        }

        void Reset() {
            needs_redraw = true;
            idle_mode = false;
            fade_alpha = 0.0f;
            last_input_time = last_render_time = std::chrono::high_resolution_clock::now();
        }
    };

    struct D3DResources {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapchain = nullptr;
        ID3D11RenderTargetView* render_target = nullptr;

        ID3D11Query* timestamp_start = nullptr;
        ID3D11Query* timestamp_end = nullptr;
        ID3D11Query* disjoint_query = nullptr;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data = {};
        bool gpu_timing_available = false;

        UINT resize_width = 0;
        UINT resize_height = 0;
        bool needs_resize = false;

        bool initialized = false;

        void SafeRelease(IUnknown*& p) {
            if (p) {
                p->Release();
                p = nullptr;
            }
        }

        void Reset() {
            SafeRelease(reinterpret_cast<IUnknown*&>(device));
            SafeRelease(reinterpret_cast<IUnknown*&>(context));
            SafeRelease(reinterpret_cast<IUnknown*&>(swapchain));
            SafeRelease(reinterpret_cast<IUnknown*&>(render_target));
            SafeRelease(reinterpret_cast<IUnknown*&>(timestamp_start));
            SafeRelease(reinterpret_cast<IUnknown*&>(timestamp_end));
            SafeRelease(reinterpret_cast<IUnknown*&>(disjoint_query));

            resize_width = resize_height = 0;
            needs_resize = initialized = gpu_timing_available = false;

            std::memset(&disjoint_data, 0, sizeof(disjoint_data));
        }
    };

    struct WindowConfig {
        HWND hwnd = nullptr;
        WNDCLASSEXW wc = {};
        bool fullscreen_mode = false;
        ImVec2 position = ImVec2(0, 0);
        ImVec2 size = ImVec2(OVERLAY_WIDTH, OVERLAY_HEIGHT);
        bool is_transparent = false;

        void Reset() {
            hwnd = nullptr;
            wc = {};
            fullscreen_mode = is_transparent = false;
            position = ImVec2(0, 0);
            size = ImVec2(OVERLAY_WIDTH, OVERLAY_HEIGHT);
        }
    };

    struct TabState {
        int selected_tab = 0;
        int previous_tab = 0;
        std::vector<float> hover_animations;
        float selection_animation = 0.0f;

        void Initialize(int tab_count) {
            hover_animations.resize(tab_count, 0.0f);
            selected_tab = previous_tab = 0;
            selection_animation = 0.0f;
        }
    };

    class OverlayManager {
    private:
        static PerformanceMetrics perf_metrics;
        static RenderState render_state;
        static FontCache font_cache;
        static TabState tab_state;
        static std::unordered_map<std::string, AnimationState> animation_cache;

        static std::chrono::high_resolution_clock::time_point last_cleanup;
        static std::chrono::high_resolution_clock::time_point last_memory_compact;


        static void UpdatePerformanceMetrics();
        static void UpdateAnimations();
        static void UpdateIdleState();
        static float CalculateEasing(float t, int easing_type);

    public:
        static bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
        static void Shutdown();
        static void LoadFonts(ImGuiIO& io);


        static const PerformanceMetrics& GetPerformanceMetrics() { return perf_metrics; }


        static void BeginFrame();
        static void EndFrame();
        static bool ShouldRender();
        static void MarkNeedsRedraw() { render_state.needs_redraw = true; }

        static void PushOptimizedStyle();
        static void PopOptimizedStyle();
        static ImVec2 CenterText(const ImVec2& min, const ImVec2& max, const char* text);
        static ImU32 BlendColors(ImU32 color1, ImU32 color2, float factor);

        static bool OptimizedKeybind(keybind* kb, const ImVec2& size = ImVec2(0, 0));
        static void OptimizedTabs(int& selected_tab, const char* tab_names[], int tab_count);
        static void BeginOptimizedChild(const char* name, const ImVec2& size, bool border = true);
        static void EndOptimizedChild();

        static ImFont* GetSmallFont() { return font_cache.small_font; }
        static ImFont* GetMediumFont() { return font_cache.medium_font; }
        static ImFont* GetLargeFont() { return font_cache.large_font; }
        static ImFont* GetLogoFont() { return font_cache.logo_font; }

        static void SetVisible(bool vis) {
            if (visible != vis) {
                visible = vis;
                render_state.MarkInputReceived();
                float target_alpha = vis ? 1.0f : 0.0f;
                UpdateAnimation("overlay_fade", target_alpha, 12.0f, 1);
                render_state.fade_alpha = animation_cache["overlay_fade"].current_value;
            }
        }
        static bool IsVisible() { return render_state.fade_alpha > 0.01f; }
        static float GetFadeAlpha() { return render_state.fade_alpha; }

        static float GetFPS() { return perf_metrics.fps; }
        static float GetFrameTime() { return perf_metrics.frame_time; }
        static float GetGPUTime() { return perf_metrics.gpu_time; }
        static size_t GetMemoryUsage() { return perf_metrics.memory_usage; }
        static void SetGPUTime(float gpu_time) { perf_metrics.gpu_time = gpu_time; }
        static bool IsIdleMode() { return render_state.idle_mode; }

        static void CompactMemory();
        static void OptimizePerformance();

        static float UpdateAnimation(const std::string& id, float target, float speed = 8.0f, int easing_type = 0);
        static void ResetAnimation(const std::string& id);
        static void ClearAnimationCache();
        static void CleanupInactiveAnimations();
    };

    extern D3DResources d3d_resources;
    extern WindowConfig window_config;

    bool Initialize();
    void Shutdown();
    void LoadInterface();
    extern std::atomic<bool> overlay_running;
    bool InitializeAndRun();
    void RenderFrame();
    void BeginRender();
    void EndRender();
    void PresentFrame();

    bool CreateDeviceD3D(HWND hwnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void HandleResize();

    bool SetupGPUTiming();
    void CleanupGPUTiming();
    void UpdateGPUTimings();

    HWND CreateOverlayWindow();
    void UpdateWindowPosition(HWND overlay_hwnd);
    bool IsTargetWindowActive();
    bool IsTargetWindowFullscreen(HWND target_hwnd);
    void SetOverlayTransparency(bool transparent);
    void ToggleVisibility();

    void ProcessMessages();

    void UpdateFrameStats();
    void AdaptiveFrameLimit();

    void RenderMainOverlay();
    void RenderTopBar();
    void RenderESPOverlay();
    void RenderAimbotTab();
    void RenderPlayerTab();
    void RenderVisualsTab();
    void RenderWorldTab();
    void RenderSettingsTab();
    void RenderESPPreview();

    void RenderPerformanceMetrics();
    void RenderWatermark();

    namespace Utils {
        std::string FormatBytes(size_t bytes);
        ImVec4 HexToColor(uint32_t hex, float alpha = 1.0f);
        uint32_t ColorToHex(const ImVec4& color);
        bool IsPointInRect(const ImVec2& point, const ImVec2& rect_min, const ImVec2& rect_max);
        float GetDeltaTime();
        void OptimizeImGuiStyle(ImGuiStyle& style);

        void ProfileScopeBegin(const char* name);
        void ProfileScopeEnd(const char* name);
        size_t GetProcessMemoryUsage();
        float GetCPUUsage();
    }

    namespace Easing {
        float Linear(float t);
        float EaseIn(float t);
        float EaseOut(float t);
        float EaseInOut(float t);
        float EaseInQuad(float t);
        float EaseOutQuad(float t);
        float EaseInOutQuad(float t);
        float EaseInCubic(float t);
        float EaseOutCubic(float t);
        float EaseInOutCubic(float t);
        float EaseInQuart(float t);
        float EaseOutQuart(float t);
        float EaseInOutQuart(float t);
        float EaseInBounce(float t);
        float EaseOutBounce(float t);
        float EaseInOutBounce(float t);
        float EaseInElastic(float t);
        float EaseOutElastic(float t);
        float EaseInOutElastic(float t);
        float EaseInBack(float t);
        float EaseOutBack(float t);
        float EaseInOutBack(float t);
    }
}