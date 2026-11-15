#include "../../backend/imgui/imgui/imgui.h"
#include <atomic>
#include "overlay.h"
#include "avatar_model.h"
#include <Pdh.h>
#include <algorithm>
#include "../../backend/imgui/imgui/backends/imgui_impl_win32.h"
#include <thread>
#include "../../globals.h"
#include "../hidden_/visuals/visuals.h"
#include "../../backend/usermode/usermode.h"
#include <Psapi.h>
#include <fstream>
#include <iterator>
#include <mutex>

// Windows media sessions, so the top bar can show whatever is actually playing in the
// background instead of a track list of our own.
#pragma comment(lib, "windowsapp.lib")
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <memory>


// Defined in cache.cpp. The cache thread replaces the player vector, so the overlay reads a
// lock-guarded copy through this instead of touching globals::cachedplayers directly.
namespace player_access {
    std::vector<roblox::player> get_cached_players();
}
#include "compataiblity.h"
#include "overlay.h"
#include <dwmapi.h>
#include <filesystem>
#include <thread>
#include <bitset>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#ifdef min
#undef min
#endif
#include <stack>
#ifdef max
#undef max
#endif
#include <Psapi.h>
#include <dwmapi.h>
#include <d3dx11.h>
#include "keybindsys/caller.h"
#include "../../globals.h"
#include "../../backend/imgui/imgui/misc/freetype/imgui_freetype.h"
#include "bytes.h"
#include "../hidden_/visuals/visuals.h"
#include <functional>
#include "../hidden_/visuals/coolfeatures/bulletracer.h"
#include "../hidden_/visuals/coolfeatures/trail.h"
#include "../../backend/configsystem.h"


#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winhttp.lib")


LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Now playing, read from the Windows media session API so it follows whatever app owns playback
// (Spotify, a browser, anything that registers a session). Those WinRT objects have to be created
// and used on one apartment-bound thread, so a single worker polls them about once a second and
// the UI only ever reads a copy of the last snapshot under a lock.
namespace music {
    struct now_playing_t {
        std::string title;
        std::string artist;
        std::string album;
        std::string app;
        bool playing = false;
        bool has_track = false;

        // Album art for the current track. Shared so copying a snapshot into the UI is cheap.
        std::shared_ptr<std::vector<unsigned char>> art;
        int art_width = 0;
        int art_height = 0;
        unsigned art_revision = 0;
    };

    // Transport controls the panel can ask for. Executed on the media thread, which is the one
    // allowed to touch the session objects.
    enum control {
        control_previous = 1,
        control_play_pause = 2,
        control_next = 3
    };

    namespace detail {
        std::mutex snapshot_lock;
        now_playing_t snapshot;
        std::atomic<bool> worker_started{ false };
        std::atomic<int> pending_command{ 0 };

        std::string narrow(winrt::hstring const& value) {
            if (value.empty()) return {};

            const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(),
                nullptr, 0, nullptr, nullptr);
            if (size <= 0) return {};

            std::string out((size_t)size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.size(), out.data(), size,
                nullptr, nullptr);
            return out;
        }

        void publish(const now_playing_t& track) {
            std::lock_guard<std::mutex> guard(snapshot_lock);
            snapshot = track;
        }

        // Decodes the session thumbnail to RGBA8, scaled down by the decoder itself: the panel
        // draws it at 58 px, so a 128 px texture is already generous.
        std::shared_ptr<std::vector<unsigned char>> load_album_art(
            winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties const& properties,
            int& out_width, int& out_height) {
            out_width = 0;
            out_height = 0;

            try {
                auto reference = properties.Thumbnail();
                if (!reference) return nullptr;

                auto stream = reference.OpenReadAsync().get();
                const uint32_t size = (uint32_t)stream.Size();

                if (size == 0 || size > 8u * 1024u * 1024u) return nullptr;

                using namespace winrt::Windows::Graphics::Imaging;
                using namespace winrt::Windows::Storage::Streams;

                auto decoder = BitmapDecoder::CreateAsync(stream).get();

                const uint32_t source_width = decoder.PixelWidth();
                const uint32_t source_height = decoder.PixelHeight();
                if (source_width == 0 || source_height == 0) return nullptr;

                constexpr float art_limit = 128.0f;
                float scale = 1.0f;

                if ((float)source_width > art_limit || (float)source_height > art_limit)
                    scale = art_limit / (float)(source_width > source_height ? source_width : source_height);

                BitmapTransform transform;
                transform.ScaledWidth((uint32_t)((float)source_width * scale));
                transform.ScaledHeight((uint32_t)((float)source_height * scale));

                auto bitmap = decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Rgba8,
                    BitmapAlphaMode::Premultiplied, transform, ExifOrientationMode::IgnoreExifOrientation,
                    ColorManagementMode::DoNotColorManage).get();

                const uint32_t width = bitmap.PixelWidth();
                const uint32_t height = bitmap.PixelHeight();
                if (width == 0 || height == 0) return nullptr;

                Buffer buffer(width * height * 4u);
                bitmap.CopyToBuffer(buffer);

                auto pixels = std::make_shared<std::vector<unsigned char>>(buffer.data(),
                    buffer.data() + buffer.Length());

                out_width = (int)width;
                out_height = (int)height;
                return pixels;
            }
            catch (...) {
                return nullptr;
            }
        }

        void poll_loop() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            }
            catch (...) {
                return;
            }

            using namespace winrt::Windows::Media::Control;

            GlobalSystemMediaTransportControlsSessionManager manager{ nullptr };

            std::string art_key;
            std::shared_ptr<std::vector<unsigned char>> art;
            int art_width = 0;
            int art_height = 0;
            unsigned art_revision = 0;

            while (true) {
                now_playing_t track;

                try {
                    if (!manager)
                        manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

                    auto session = manager.GetCurrentSession();

                    if (session) {
                        const int pending = detail::pending_command.exchange(0);

                        if (pending != 0) {
                            if (pending == (int)control_previous)
                                session.TrySkipPreviousAsync().get();
                            else if (pending == (int)control_play_pause)
                                session.TryTogglePlayPauseAsync().get();
                            else
                                session.TrySkipNextAsync().get();
                        }

                        auto properties = session.TryGetMediaPropertiesAsync().get();
                        auto playback = session.GetPlaybackInfo();

                        track.title = narrow(properties.Title());
                        track.artist = narrow(properties.Artist());
                        track.album = narrow(properties.AlbumTitle());
                        track.app = narrow(session.SourceAppUserModelId());
                        track.playing = playback.PlaybackStatus() ==
                            GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                        track.has_track = !track.title.empty();

                        // Artwork only changes with the track, so it is fetched and decoded once
                        // per track rather than every second.
                        const std::string key = track.title + "|" + track.artist;

                        if (key != art_key) {
                            art_key = key;
                            art = load_album_art(properties, art_width, art_height);
                            art_revision++;
                        }

                        track.art = art;
                        track.art_width = art_width;
                        track.art_height = art_height;
                        track.art_revision = art_revision;
                    }
                }
                catch (...) {
                    // A session can disappear mid query; the next pass picks up whatever is live.
                    track = now_playing_t{};
                }

                publish(track);

                // Slept in short steps: a queued control is then acted on within about 100 ms
                // instead of waiting out the full poll interval.
                for (int i = 0; i < 10 && detail::pending_command.load() == 0; i++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void send(control command) {
        detail::pending_command.store((int)command);
    }

    // Starts the poller on first use and returns the last snapshot immediately.
    now_playing_t current() {
        if (!detail::worker_started.exchange(true))
            std::thread(detail::poll_loop).detach();

        std::lock_guard<std::mutex> guard(detail::snapshot_lock);
        return detail::snapshot;
    }
}


// Snow for the menu. It goes into the background draw list, so it falls over the game and behind
// every menu window, and it is only stepped while the menu is up.
namespace snowfall {
    struct flake {
        float x = 0.0f;
        float y = 0.0f;
        float speed = 0.0f;
        float drift = 0.0f;
        float radius = 0.0f;
        float alpha = 0.0f;
        float phase = 0.0f;
    };

    std::vector<flake> flakes;
    ImVec2 seeded_size(-1.0f, -1.0f);
    float elapsed = 0.0f;

    void reseed(float width, float height) {
        flakes.resize(140);

        for (auto& f : flakes) {
            f.x = (float)rand() / (float)RAND_MAX * width;
            f.y = (float)rand() / (float)RAND_MAX * height;
            f.speed = 16.0f + (float)rand() / (float)RAND_MAX * 36.0f;
            f.drift = -10.0f + (float)rand() / (float)RAND_MAX * 20.0f;
            f.radius = 0.8f + (float)rand() / (float)RAND_MAX * 1.7f;
            f.alpha = 0.35f + (float)rand() / (float)RAND_MAX * 0.5f;
            f.phase = (float)rand() / (float)RAND_MAX * 6.2831853f;
        }

        seeded_size = ImVec2(width, height);
    }

    void draw() {
        const ImGuiIO& io = ImGui::GetIO();
        const float width = io.DisplaySize.x;
        const float height = io.DisplaySize.y;

        if (width < 32.0f || height < 32.0f) return;

        if (flakes.empty() || seeded_size.x != width || seeded_size.y != height)
            reseed(width, height);

        const float dt = ImMin(io.DeltaTime, 0.05f);
        elapsed += dt;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        for (auto& f : flakes) {
            f.y += f.speed * dt;
            f.x += (f.drift + sinf(elapsed * 1.3f + f.phase) * 14.0f) * dt;

            if (f.y > height + 10.0f) {
                f.y = -10.0f;
                f.x = (float)rand() / (float)RAND_MAX * width;
            }

            if (f.x < -10.0f) f.x = width + 10.0f;
            else if (f.x > width + 10.0f) f.x = -10.0f;

            const ImVec2 at(f.x, f.y);
            const int core_alpha = (int)(f.alpha * 255.0f);

            draw->AddCircleFilled(at, f.radius, IM_COL32(255, 255, 255, core_alpha), 8);
        }
    }
}

namespace overlay {
    bool visible = false;

    // Screen rect of the floating bar, published by RenderTopBar so the input loop can keep
    // the overlay clickable over it while the menu is closed and the overlay is click-through.
    static RECT topbar_rect = { 0, 0, 0, 0 };
    static bool topbar_rect_valid = false;

    // Same idea for the floating HUD pieces: the overlay is click-through while the menu is
    // closed, and these rects keep their drag areas usable in that state.
    static RECT watermark_rect = { 0, 0, 0, 0 };
    static bool watermark_rect_valid = false;
    static RECT keybind_rect = { 0, 0, 0, 0 };
    static bool keybind_rect_valid = false;

    // Separate from `visible`: the keybind hides the bar with the menu, while the bar's own
    // menu button only closes the menu and leaves the bar up.
    static bool topbar_visible = true;

    std::atomic<bool> should_exit(false);
    D3DResources d3d_resources = {};
    WindowConfig window_config = {};

    PerformanceMetrics OverlayManager::perf_metrics = {};
    RenderState OverlayManager::render_state = {};
    FontCache OverlayManager::font_cache = {};
    TabState OverlayManager::tab_state = {};
    std::unordered_map<std::string, AnimationState> OverlayManager::animation_cache;
    std::chrono::high_resolution_clock::time_point OverlayManager::last_cleanup;
    std::chrono::high_resolution_clock::time_point OverlayManager::last_memory_compact;

    static unsigned char FontData[] = { 0x00 };

    static PDH_HQUERY cpu_query;
    static PDH_HCOUNTER cpu_counter;
    static bool cpu_monitoring_initialized = false;

    static const char* tab_names[] = { "Aimbot", "Visuals", "World", "Player", "Settings" };
    static constexpr int tab_count = sizeof(tab_names) / sizeof(tab_names[0]);

    bool OverlayManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain) {
        if (!device || !context || !swapchain) {
            return false;
        }

        perf_metrics.Reset();
        render_state.Reset();
        font_cache.Clear();
        animation_cache.clear();

        tab_state.Initialize(tab_count);

        auto now = std::chrono::high_resolution_clock::now();
        last_cleanup = last_memory_compact = now;

        if (!cpu_monitoring_initialized) {
            if (PdhOpenQuery(nullptr, NULL, &cpu_query) == ERROR_SUCCESS) {
                if (PdhAddEnglishCounter(cpu_query, "\\Processor(_Total)\\% Processor Time", NULL, &cpu_counter) == ERROR_SUCCESS) {
                    PdhCollectQueryData(cpu_query);
                    cpu_monitoring_initialized = true;
                }
            }
        }

        return true;
    }

    void OverlayManager::Shutdown() {
        if (cpu_monitoring_initialized) {
            PdhCloseQuery(cpu_query);
            cpu_monitoring_initialized = false;
        }

        animation_cache.clear();
        font_cache.Clear();
        perf_metrics.Reset();
        render_state.Reset();
    }

    static bool LoadFonts(ImGuiIO& io) {
        io.Fonts->AddFontDefault();
        return true;
    }

    void OverlayManager::BeginFrame() {
        auto now = std::chrono::high_resolution_clock::now();

        UpdatePerformanceMetrics();
        UpdateAnimations();
        UpdateIdleState();

        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0 ||
            io.KeysDown[VK_LBUTTON] || io.KeysDown[VK_RBUTTON]) {
            render_state.MarkInputReceived();
        }

        if (std::chrono::duration<float>(now - last_cleanup).count() > 10.0f) {
            CleanupInactiveAnimations();
            last_cleanup = now;
        }

        if (std::chrono::duration<float>(now - last_memory_compact).count() > 30.0f) {
            CompactMemory();
            last_memory_compact = now;
        }
    }

    void OverlayManager::EndFrame() {
        render_state.needs_redraw = false;
        render_state.last_render_time = std::chrono::high_resolution_clock::now();
    }

    bool OverlayManager::ShouldRender() {
        UpdateIdleState();

        if (IsVisible() || render_state.needs_redraw) {
            return true;
        }

        if (render_state.idle_mode) {
            static auto last_idle_render = std::chrono::high_resolution_clock::now();
            auto now = std::chrono::high_resolution_clock::now();

            if (std::chrono::duration<float>(now - last_idle_render).count() > 0.1f) {
                last_idle_render = now;
                return true;
            }
            return false;
        }

        return true;
    }

    void OverlayManager::UpdatePerformanceMetrics() {
        auto now = std::chrono::high_resolution_clock::now();

        float frame_delta = std::chrono::duration<float>(now - perf_metrics.last_frame_start).count();
        perf_metrics.frame_time = perf_metrics.frame_time * 0.95f + frame_delta * 0.05f;
        perf_metrics.last_frame_start = now;
        perf_metrics.frame_count++;

        float time_since_update = std::chrono::duration<float>(now - perf_metrics.last_update).count();
        if (time_since_update >= PERFORMANCE_UPDATE_INTERVAL) {
            perf_metrics.fps = perf_metrics.frame_count / time_since_update;
            perf_metrics.frame_count = 0;
            perf_metrics.last_update = now;

            if (cpu_monitoring_initialized) {
                PDH_FMT_COUNTERVALUE counter_val;
                PdhCollectQueryData(cpu_query);
                if (PdhGetFormattedCounterValue(cpu_counter, PDH_FMT_DOUBLE, nullptr, &counter_val) == ERROR_SUCCESS) {
                    perf_metrics.cpu_usage = static_cast<float>(counter_val.doubleValue);
                }
            }

            perf_metrics.memory_usage = Utils::GetProcessMemoryUsage();
        }
    }

    void OverlayManager::UpdateAnimations() {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = ImGui::GetIO().DeltaTime;

        for (auto& [id, anim] : animation_cache) {
            if (!anim.active) continue;

            float diff = anim.target_value - anim.current_value;
            if (std::abs(diff) > 0.001f) {
                anim.current_value += diff * anim.speed * dt;
            }
            else {
                anim.current_value = anim.target_value;
            }
            anim.last_update = now;
        }
    }

    void OverlayManager::UpdateIdleState() {
        auto now = std::chrono::high_resolution_clock::now();
        float time_since_input = std::chrono::duration<float>(now - render_state.last_input_time).count();

        bool was_idle = render_state.idle_mode;
        render_state.idle_mode = time_since_input > IDLE_TIMEOUT && !visible;

        if (was_idle != render_state.idle_mode) {
            render_state.needs_redraw = true;
        }
    }

    float OverlayManager::CalculateEasing(float t, int easing_type) {
        t = std::clamp(t, 0.0f, 1.0f);

        switch (easing_type) {
        case 0: return Easing::Linear(t);
        case 1: return Easing::EaseOut(t);
        case 2: return Easing::EaseInOut(t);
        case 3: return Easing::EaseIn(t);
        case 4: return Easing::EaseOutBounce(t);
        case 5: return Easing::EaseOutElastic(t);
        case 6: return Easing::EaseOutBack(t);
        default: return t;
        }
    }

    float OverlayManager::UpdateAnimation(const std::string& id, float target, float speed, int easing_type) {
        auto& anim = animation_cache[id];
        auto now = std::chrono::high_resolution_clock::now();

        if (!anim.active) {
            anim.current_value = anim.target_value = target;
            anim.speed = speed;
            anim.easing_type = easing_type;
            anim.last_update = now;
            anim.active = true;
            anim.needs_update = false;
            return target;
        }

        if (std::abs(anim.target_value - target) > 0.001f) {
            anim.target_value = target;
            anim.speed = speed;
            anim.easing_type = easing_type;
            anim.needs_update = true;
        }

        return anim.current_value;
    }

    void OverlayManager::ResetAnimation(const std::string& id) {
        auto it = animation_cache.find(id);
        if (it != animation_cache.end()) {
            it->second.Reset();
        }
    }

    void OverlayManager::CleanupInactiveAnimations() {
        auto now = std::chrono::high_resolution_clock::now();

        for (auto it = animation_cache.begin(); it != animation_cache.end();) {
            if (!it->second.active ||
                std::chrono::duration<float>(now - it->second.last_update).count() > 30.0f) {
                it = animation_cache.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void OverlayManager::PushOptimizedStyle() {
        ImGuiStyle& style = ImGui::GetStyle();

        static bool style_optimized = false;
        if (!style_optimized) {
            Utils::OptimizeImGuiStyle(style);
            style_optimized = true;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, render_state.fade_alpha);
    }

    void OverlayManager::PopOptimizedStyle() {
        ImGui::PopStyleVar();
    }

    ImVec2 OverlayManager::CenterText(const ImVec2& min, const ImVec2& max, const char* text) {
        ImVec2 text_size = ImGui::CalcTextSize(text);
        return min + (max - min) * 0.5f - text_size * 0.5f;
    }

    ImU32 OverlayManager::BlendColors(ImU32 color1, ImU32 color2, float factor) {
        factor = std::clamp(factor, 0.0f, 1.0f);

        ImVec4 c1 = ImGui::ColorConvertU32ToFloat4(color1);
        ImVec4 c2 = ImGui::ColorConvertU32ToFloat4(color2);

        ImVec4 result = ImVec4(
            c1.x + (c2.x - c1.x) * factor,
            c1.y + (c2.y - c1.y) * factor,
            c1.z + (c2.z - c1.z) * factor,
            c1.w + (c2.w - c1.w) * factor
        );

        return ImGui::ColorConvertFloat4ToU32(result);
    }

    void OverlayManager::CompactMemory() {
        ImGuiContext& g = *GImGui;
        if (g.IO.ConfigMemoryCompactTimer >= 0.0f) {
            g.IO.ConfigMemoryCompactTimer = 0.0f;
        }

        CleanupInactiveAnimations();

        if (tab_state.hover_animations.capacity() > tab_count * 2) {
            tab_state.hover_animations.shrink_to_fit();
        }
    }

    void OverlayManager::OptimizePerformance() {
        CompactMemory();
        perf_metrics.frame_count = 0;
        perf_metrics.last_update = std::chrono::high_resolution_clock::now();
        ClearAnimationCache();
    }

    void OverlayManager::ClearAnimationCache() {
        animation_cache.clear();
        tab_state.hover_animations.assign(tab_count, 0.0f);
    }

    bool OverlayManager::OptimizedKeybind(keybind* kb, const ImVec2& size_arg) {
        if (!kb) return false;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(kb->get_name().c_str());
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImGui::CalcItemSize(size_arg, 80.0f, ImGui::GetFrameHeight());
        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        static std::unordered_map<ImGuiID, float> keybind_animations;
        float& anim = keybind_animations[id];
        anim += (hovered ? 1.0f : 0.0f - anim) * ImGui::GetIO().DeltaTime * 8.0f;
        anim = ImClamp(anim, 0.0f, 1.0f);

        std::string name = kb->waiting_for_input ? "[...]" : kb->get_key_name();

        if (ImGui::GetIO().MouseClicked[0] && hovered) {
            if (g.ActiveId == id) {
                kb->waiting_for_input = true;
            }
        }
        else if (ImGui::GetIO().MouseClicked[1] && hovered) {
            ImGui::OpenPopup(kb->get_name().c_str());
        }
        else if (ImGui::GetIO().MouseClicked[0] && !hovered && g.ActiveId == id) {
            ImGui::ClearActiveID();
        }

        if (kb->waiting_for_input) {
            if (ImGui::GetIO().MouseClicked[0] && !hovered) {
                kb->key = VK_LBUTTON;
                ImGui::ClearActiveID();
                kb->waiting_for_input = false;
            }
            else if (kb->set_key()) {
                ImGui::ClearActiveID();
                kb->waiting_for_input = false;
            }
        }

        float window_alpha = style.Alpha;

        ImVec4 base_top = ImVec4(0.15f, 0.15f, 0.15f, window_alpha);
        ImVec4 base_bottom = ImVec4(0.08f, 0.08f, 0.08f, window_alpha);
        ImVec4 hover_top = ImVec4(0.18f, 0.18f, 0.18f, window_alpha);
        ImVec4 hover_bottom = ImVec4(0.11f, 0.11f, 0.11f, window_alpha);

        ImVec4 final_top = ImVec4(
            base_top.x + (hover_top.x - base_top.x) * anim,
            base_top.y + (hover_top.y - base_top.y) * anim,
            base_top.z + (hover_top.z - base_top.z) * anim,
            window_alpha
        );
        ImVec4 final_bottom = ImVec4(
            base_bottom.x + (hover_bottom.x - base_bottom.x) * anim,
            base_bottom.y + (hover_bottom.y - base_bottom.y) * anim,
            base_bottom.z + (hover_bottom.z - base_bottom.z) * anim,
            window_alpha
        );

        ImU32 bg_top = ImGui::ColorConvertFloat4ToU32(final_top);
        ImU32 bg_bottom = ImGui::ColorConvertFloat4ToU32(final_bottom);

        window->DrawList->AddRectFilledMultiColor(bb.Min, bb.Max, bg_top, bg_top, bg_bottom, bg_bottom);
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, window_alpha)), 0.0f, 0, 1.0f);

        ImVec2 text_pos = CenterText(bb.Min, bb.Max, name.c_str());
        ImU32 text_color = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, window_alpha));
        ImU32 outline_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.02f, 0.02f, 0.02f, window_alpha));

        window->DrawList->AddText(ImVec2(text_pos.x - 1, text_pos.y), outline_color, name.c_str());
        window->DrawList->AddText(ImVec2(text_pos.x + 1, text_pos.y), outline_color, name.c_str());
        window->DrawList->AddText(ImVec2(text_pos.x, text_pos.y - 1), outline_color, name.c_str());
        window->DrawList->AddText(ImVec2(text_pos.x, text_pos.y + 1), outline_color, name.c_str());
        window->DrawList->AddText(text_pos, text_color, name.c_str());

        if (ImGui::BeginPopup(kb->get_name().c_str())) {
            if (ImGui::Selectable("Hold", kb->type == keybind::HOLD)) kb->type = keybind::HOLD;
            if (ImGui::Selectable("Toggle", kb->type == keybind::TOGGLE)) kb->type = keybind::TOGGLE;
            if (ImGui::Selectable("Always", kb->type == keybind::ALWAYS)) kb->type = keybind::ALWAYS;
            ImGui::EndPopup();
        }

        return pressed;
    }

    void OverlayManager::OptimizedTabs(int& selected_tab, const char* tab_names[], int tab_count) {
        if (tab_count > static_cast<int>(tab_state.hover_animations.size())) {
            tab_state.hover_animations.resize(tab_count, 0.0f);
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        float border_thickness = 1.2f;
        float black_outline = 1.0f;
        float total_border = border_thickness + black_outline;

        float total_width = ImGui::GetContentRegionAvail().x - (total_border * 2);
        float tab_width = total_width / tab_count;
        float tab_height = 30.0f;

        ImVec2 start_pos = ImVec2(
            ImGui::GetCursorScreenPos().x + total_border,
            ImGui::GetCursorScreenPos().y + total_border
        );

        static ImU32 accent_color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.36f, 0.25f, 0.83f, 1.0f));
        static ImU32 active_bg = ImGui::GetColorU32(ImGuiCol_ChildBg);
        static ImU32 inactive_bg = ImGui::GetColorU32(ImGuiCol_ChildBg);
        static ImU32 hover_bg = ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
        static ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
        static ImU32 black_border = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        float selection_anim = UpdateAnimation("tab_selection", static_cast<float>(selected_tab), 8.0f, 2);

        ImVec2 outer_min = ImVec2(start_pos.x - total_border, start_pos.y - total_border);
        ImVec2 outer_max = ImVec2(start_pos.x + total_width + total_border, start_pos.y + tab_height + total_border);
        draw_list->AddRect(outer_min, outer_max, black_border, 0.0f, 0, black_outline);

        ImVec2 inner_min = ImVec2(start_pos.x - border_thickness, start_pos.y - border_thickness);
        ImVec2 inner_max = ImVec2(start_pos.x + total_width + border_thickness, start_pos.y + tab_height + border_thickness);
        draw_list->AddRect(inner_min, inner_max, border_color, 0.0f, 0, border_thickness);

        for (int i = 0; i < tab_count; i++) {
            ImVec2 tab_min = ImVec2(start_pos.x + i * tab_width, start_pos.y);
            ImVec2 tab_max = ImVec2(start_pos.x + (i + 1) * tab_width, start_pos.y + tab_height);

            bool is_hovered = ImGui::IsMouseHoveringRect(tab_min, tab_max);
            bool is_active = (selected_tab == i);

            if (is_hovered && ImGui::IsMouseClicked(0)) {
                selected_tab = i;
                MarkNeedsRedraw();
            }

            std::string hover_id = "tab_hover_" + std::to_string(i);
            float hover_anim = UpdateAnimation(hover_id, is_hovered ? 1.0f : 0.0f, 6.0f, 1);

            ImU32 bg_color = is_hovered ? hover_bg : (is_active ? active_bg : inactive_bg);
            draw_list->AddRectFilled(tab_min, tab_max, bg_color);

            ImVec2 text_size = ImGui::CalcTextSize(tab_names[i]);
            ImVec2 text_pos = ImVec2(
                tab_min.x + (tab_width - text_size.x) * 0.5f,
                tab_min.y + (tab_height - text_size.y) * 0.5f
            );

            ImVec4 accent_vec = ImVec4(0.36f, 0.25f, 0.83f, 1.0f);
            ImVec4 text_base_color = is_active ? accent_vec : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            ImU32 text_color = ImGui::GetColorU32(text_base_color);
            ImU32 text_outline = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.8f));

            draw_list->AddText(ImVec2(text_pos.x - 1, text_pos.y), text_outline, tab_names[i]);
            draw_list->AddText(ImVec2(text_pos.x + 1, text_pos.y), text_outline, tab_names[i]);
            draw_list->AddText(ImVec2(text_pos.x, text_pos.y - 1), text_outline, tab_names[i]);
            draw_list->AddText(ImVec2(text_pos.x, text_pos.y + 1), text_outline, tab_names[i]);
            draw_list->AddText(text_pos, text_color, tab_names[i]);

            if (is_active) {
                float line_y = start_pos.y + 2.0f;
                draw_list->AddLine(
                    ImVec2(tab_min.x + 2, line_y),
                    ImVec2(tab_max.x - 2, line_y),
                    accent_color, 2.0f
                );
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(start_pos.x - total_border, start_pos.y + tab_height + total_border));
    }

    void OverlayManager::BeginOptimizedChild(const char* name, const ImVec2& size, bool border) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::BeginChild(name, size, border);
    }

    void OverlayManager::EndOptimizedChild() {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }


   


    bool Initialize() {
        window_config.hwnd = CreateOverlayWindow();
        if (!window_config.hwnd) {
            return false;
        }

        if (!CreateDeviceD3D(window_config.hwnd)) {
            CleanupDeviceD3D();
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigMemoryCompactTimer = 60.0f;

        if (!ImGui_ImplWin32_Init(window_config.hwnd)) {
            return false;
        }

        if (!ImGui_ImplDX11_Init(d3d_resources.device, d3d_resources.context)) {
            return false;
        }

        ImGuiStyle& style = ImGui::GetStyle();

        // Redone look: near-black glass surfaces, hairline borders and one indigo
        // accent. Padding and row heights stay small so the panel remains slim.
        const ImVec4 accent = ImVec4(0.42f, 0.36f, 0.91f, 1.00f);
        const ImVec4 accent_soft = ImVec4(0.42f, 0.36f, 0.91f, 0.16f);
        const ImVec4 hairline = ImVec4(0.14f, 0.14f, 0.18f, 0.55f);

        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 5);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);

        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 8.0f;
        style.GrabRounding = 6.0f;

        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
        style.Colors[ImGuiCol_Separator] = hairline;
        style.Colors[ImGuiCol_SliderGrab] = accent;
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.53f, 0.47f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive] = accent_soft;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.039f, 0.039f, 0.051f, 1.00f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.063f, 0.063f, 0.082f, 1.00f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.07f, 0.98f);
        style.Colors[ImGuiCol_WindowShadow] = ImVec4(0.22f, 0.18f, 0.55f, 0.45f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.60f, 0.60f, 0.66f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.32f, 0.32f, 0.36f, 1.00f);
        colors[ImGuiCol_Border] = hairline;
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonActive] = accent_soft;
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_Header] = accent_soft;
        colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        colors[ImGuiCol_HeaderActive] = accent_soft;
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        colors[ImGuiCol_TabSelected] = accent_soft;
        colors[ImGuiCol_TabSelectedOverline] = accent;
        colors[ImGuiCol_TabDimmed] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TabDimmedSelected] = accent_soft;
        colors[ImGuiCol_TextLink] = accent;
        colors[ImGuiCol_TextSelectedBg] = accent_soft;
        colors[ImGuiCol_NavHighlight] = accent;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.26f, 0.32f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = accent;

        style.WindowShadowSize = 100;


        ImFontConfig config;

        ImFontConfig config2;
        config.MergeMode = false;

        config2.FontBuilderFlags = ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_Bold;

        config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_LightHinting;
        config.OversampleH = 25;
        config.OversampleV = 25;
        config.PixelSnapH = true;

        io.Fonts->AddFontFromMemoryTTF(&NunitoMedium, sizeof NunitoMedium, 16, &config, io.Fonts->GetGlyphRangesCyrillic());

        ShowWindow(window_config.hwnd, SW_SHOWDEFAULT);
        UpdateWindow(window_config.hwnd);

        return true;
    }

    void Shutdown() {
        should_exit = true;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        OverlayManager::Shutdown();
        CleanupGPUTiming();
        CleanupDeviceD3D();

        if (window_config.hwnd) {
            ::DestroyWindow(window_config.hwnd);
            ::UnregisterClassW(window_config.wc.lpszClassName, window_config.wc.hInstance);
        }

        window_config.Reset();
        d3d_resources.Reset();
    }
    void ForceOverlayOnTop() {
        HWND roblox = FindWindowA(nullptr, "Roblox");
        if (!roblox || !window_config.hwnd) return;

        HWND windowAboveRoblox = GetWindow(roblox, GW_HWNDPREV);

        // Already sitting directly above Roblox, so reordering every frame would only
        // make the window flicker.
        if (windowAboveRoblox == window_config.hwnd) return;

        if (windowAboveRoblox) {
            SetWindowPos(window_config.hwnd, windowAboveRoblox, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        else {
            SetWindowPos(window_config.hwnd, roblox, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }


    void overlay::dex() {
        if (!globals::explorer) return;

        static roblox::instance selected_instance;
        static std::unordered_map<uint64_t, std::vector<roblox::instance>> node_cache;
        static std::unordered_map<uint64_t, std::string> name_cache;
        static std::unordered_map<uint64_t, std::string> class_cache;
        static std::unordered_map<uint64_t, std::string> path_cache;
        static char search_query[128] = "";
        static std::vector<roblox::instance> search_results;
        static bool show_search_results = false;
        static bool cache_initialized = false;
        static auto last_cache_refresh = std::chrono::steady_clock::now();
        static ImVec2 dex_pos = ImVec2(GetSystemMetrics(SM_CXSCREEN) - 550, 80);
        static bool first_time = true;

        static char edit_buffer[512] = "";
        static float float_value = 0.0f;
        static int int_value = 0;
        static bool bool_value = false;
        static Vec3 vec3_value = Vec3::zero();
        static int selected_tab = 0;

        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;
        float old_rounding = style.WindowRounding;
        style.WindowRounding = 0;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings;
        if (!overlay::visible) {
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }

        if (first_time || !overlay::visible) {
            // Keep the default placement on-screen now that the window can be wider.
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            dex_pos.x = ImClamp(dex_pos.x, 0.0f, ImMax(screen.x - 570.0f, 0.0f));
            dex_pos.y = ImClamp(dex_pos.y, 0.0f, ImMax(screen.y - 120.0f, 0.0f));

            ImGui::SetNextWindowPos(dex_pos, ImGuiCond_Always);
            first_time = false;
        }

        // The explorer used to print "[?]" for every class the old shortlist missed. There is no
        // texture pipeline or image assets in the project, so the icons are drawn as vectors:
        // one shape per class family, so every row gets a real icon and nothing falls back to a
        // question mark.
        enum DexIconKind { DexService, DexFolder, DexModel, DexPart, DexScript, DexPlayer, DexValue, DexGui, DexOther };

        auto dex_icon_kind = [](const std::string& cls) -> DexIconKind {
            const bool is_service = cls.size() > 7 && cls.compare(cls.size() - 7, 7, "Service") == 0;

            if (cls == "Workspace" || cls == "Players" || cls == "Lighting" || cls == "Chat" ||
                cls == "ReplicatedStorage" || cls == "ReplicatedFirst" || cls == "ServerStorage" ||
                cls == "ServerScriptService" || cls == "StarterGui" || cls == "StarterPack" ||
                cls == "StarterPlayer" || cls == "RunService" || cls == "UserInputService" ||
                cls == "TweenService" || cls == "TeleportService" || cls == "TestService" ||
                is_service) {
                return DexService;
            }
            if (cls == "Player" || cls == "Humanoid" || cls == "HumanoidDescription") return DexPlayer;
            if (cls == "Folder" || cls == "Configuration") return DexFolder;
            if (cls == "Model" || cls == "MeshPart" || cls == "Accessory" || cls == "UnionOperation" ||
                cls == "NegateOperation") {
                return DexModel;
            }
            if (cls == "Part" || cls == "WedgePart" || cls == "SpawnLocation" || cls == "Terrain" ||
                cls == "Seat" || cls == "TrussPart") {
                return DexPart;
            }
            if (cls.find("Script") != std::string::npos) return DexScript;
            if (cls.find("Value") != std::string::npos) return DexValue;
            if (cls == "Frame" || cls == "ScreenGui" || cls == "TextLabel" || cls == "TextButton" ||
                cls == "Textbox" || cls == "ImageLabel" || cls == "ImageButton" || cls == "BillboardGui" ||
                cls == "SurfaceGui" || cls == "Camera" || cls == "Animation") {
                return DexGui;
            }

            return DexOther;
        };

        auto draw_dex_icon = [&](const std::string& class_name, ImU32 tint) {
            const DexIconKind kind = dex_icon_kind(class_name);
            const float size = 16.0f;

            // Each family gets its own colour as well as its own shape, so a folder, a model
            // and a script are told apart by the picture alone at a glance.
            ImU32 main = tint;
            switch (kind) {
            case DexFolder:  main = IM_COL32(245, 196, 81, 255);  break;
            case DexModel:   main = IM_COL32(120, 220, 140, 255); break;
            case DexPart:    main = IM_COL32(240, 130, 120, 255); break;
            case DexScript:  main = IM_COL32(240, 220, 100, 255); break;
            case DexPlayer:  main = IM_COL32(120, 180, 255, 255); break;
            case DexValue:   main = IM_COL32(120, 220, 220, 255); break;
            case DexGui:     main = IM_COL32(220, 140, 220, 255); break;
            case DexService: main = IM_COL32(150, 150, 240, 255); break;
            default:         main = IM_COL32(190, 190, 200, 255); break;
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();

            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float top = origin.y + (ImGui::GetTextLineHeight() - size) * 0.5f;
            const ImVec2 a(origin.x, top);
            const ImVec2 b(origin.x + size, top + size);
            const float w = b.x - a.x;
            const float h = b.y - a.y;
            const ImU32 mid = (main & 0x00FFFFFFu) | (150u << 24);
            const ImU32 dark = (main & 0x00FFFFFFu) | (100u << 24);
            const ImU32 ink = IM_COL32(15, 15, 20, 190);

            switch (kind) {
            case DexFolder: {
                // Tab on top, open body below: unmistakably a folder.
                dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 2.0f), ImVec2(a.x + 7.0f, a.y + 5.5f), main, 1.5f);
                dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 4.0f), ImVec2(b.x - 1.0f, b.y - 1.5f), main, 2.5f);
                dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 7.0f), ImVec2(b.x - 1.0f, b.y - 1.5f), dark, 2.0f);
                break;
            }
            case DexModel: {
                // Isometric cube: three shaded faces reads as a 3D model.
                const ImVec2 t(a.x + w * 0.5f, a.y + 1.0f);
                const ImVec2 l(a.x + 1.0f, a.y + h * 0.30f);
                const ImVec2 r(b.x - 1.0f, a.y + h * 0.30f);
                const ImVec2 m(a.x + w * 0.5f, a.y + h * 0.57f);
                const ImVec2 bl(a.x + 1.0f, b.y - 1.0f);
                const ImVec2 br(b.x - 1.0f, b.y - 1.0f);

                dl->AddQuadFilled(t, r, m, l, main);
                dl->AddQuadFilled(l, m, ImVec2(a.x + w * 0.5f, b.y - 1.0f), bl, dark);
                dl->AddQuadFilled(m, r, br, ImVec2(a.x + w * 0.5f, b.y - 1.0f), mid);
                break;
            }
            case DexPart: {
                // Solid brick with a lit top edge.
                dl->AddRectFilled(ImVec2(a.x + 2.0f, a.y + 4.0f), ImVec2(b.x - 2.0f, b.y - 1.0f), main, 2.0f);
                dl->AddRectFilled(ImVec2(a.x + 3.0f, a.y + 1.5f), ImVec2(b.x - 1.0f, a.y + 5.0f), mid, 1.5f);
                break;
            }
            case DexScript: {
                // Sheet of paper with a folded corner and code lines.
                dl->AddRectFilled(ImVec2(a.x + 3.0f, a.y + 1.0f), ImVec2(b.x - 2.0f, b.y - 1.0f), main, 1.5f);
                dl->AddTriangleFilled(ImVec2(b.x - 2.0f, a.y + 1.0f), ImVec2(b.x - 2.0f, a.y + 6.0f), ImVec2(b.x - 7.0f, a.y + 1.0f), dark);

                for (int line = 0; line < 3; ++line) {
                    const float ly = a.y + 6.5f + line * 3.0f;
                    dl->AddLine(ImVec2(a.x + 5.0f, ly), ImVec2(b.x - 5.5f + line * 1.5f, ly), ink, 1.0f);
                }
                break;
            }
            case DexPlayer: {
                // Head and shoulders silhouette.
                dl->AddCircleFilled(ImVec2(a.x + w * 0.5f, a.y + 5.0f), 4.0f, main, 14);
                dl->AddRectFilled(ImVec2(a.x + 3.0f, a.y + 10.0f), ImVec2(b.x - 3.0f, b.y - 1.0f), main, 3.0f);
                break;
            }
            case DexValue: {
                dl->AddNgonFilled(ImVec2(a.x + w * 0.5f, a.y + h * 0.5f), 6.5f, main, 4);
                break;
            }
            case DexGui: {
                // Window with a filled title bar.
                dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 2.0f), ImVec2(b.x - 1.0f, b.y - 2.0f), dark, 2.0f);
                dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 2.0f), ImVec2(b.x - 1.0f, a.y + 6.5f), main, 2.0f);
                break;
            }
            case DexService: {
                // Badge with a hole: a service sitting in the DataModel.
                const ImVec2 c(a.x + w * 0.5f, a.y + h * 0.5f);
                dl->AddCircleFilled(c, 6.2f, main, 16);
                dl->AddCircleFilled(c, 2.6f, IM_COL32(18, 18, 24, 255), 12);
                break;
            }
            default: {
                // Generic instance.
                dl->AddRect(ImVec2(a.x + 2.5f, a.y + 2.5f), ImVec2(b.x - 2.5f, b.y - 2.5f), main, 2.0f, 0, 1.4f);
                dl->AddCircleFilled(ImVec2(a.x + w * 0.5f, a.y + h * 0.5f), 2.4f, main, 10);
                break;
            }
            }

            ImGui::Dummy(ImVec2(size + 5.0f, ImGui::GetTextLineHeight()));
            ImGui::SameLine();
        };

        auto get_code_path = [&](roblox::instance& inst) -> std::string {
            std::vector<std::string> path_parts;
            roblox::instance current = inst;

            while (current.address != 0 && mem::is_readable(current.address)) {
                std::string name = current.get_name();
                if (!name.empty() && name != "DataModel") {
                    path_parts.push_back(name);
                }
                current = current.read_parent();
            }

            if (path_parts.empty()) return "";

            std::reverse(path_parts.begin(), path_parts.end());

            std::string result;
            if (path_parts[0] == "Workspace") {
                result = "globals::workspace";
                for (size_t i = 1; i < path_parts.size(); i++) {
                    result += ".findfirstchild(\"" + path_parts[i] + "\")";
                }
            }
            else if (path_parts[0] == "Players") {
                result = "globals::players";
                if (path_parts.size() > 1) {
                    if (path_parts[1] == globals::lp.name) {
                        result = "globals::lp";
                        for (size_t i = 2; i < path_parts.size(); i++) {
                            result += ".findfirstchild(\"" + path_parts[i] + "\")";
                        }
                    }
                    else {
                        result = "globals::cachedplayer";
                        for (size_t i = 1; i < path_parts.size(); i++) {
                            result += ".findfirstchild(\"" + path_parts[i] + "\")";
                        }
                    }
                }
            }
            else {
                result = "globals::datamodel";
                for (const auto& part : path_parts) {
                    result += ".findfirstchild(\"" + part + "\")";
                }
            }

            return result;
            };

        auto cache_node = [&](roblox::instance& node) {
            // Every visible node calls this each frame, so answer from the cache first. The
            // readability probe below is a VirtualQueryEx against the game process; putting it
            // on the hot path made the menu stutter with the explorer open.
            if (node_cache.find(node.address) != node_cache.end())
                return;

            // Refuse to read anything whose pointer does not point at committed memory.
            if (!mem::is_readable(node.address)) {
                node_cache[node.address] = {};
                name_cache[node.address] = "Invalid";
                class_cache[node.address] = "Unknown";
                path_cache[node.address] = "Instance is no longer readable";
                return;
            }

            {
                try {
                    node_cache[node.address] = node.get_children();
                    name_cache[node.address] = node.get_name();
                    class_cache[node.address] = node.get_class_name();
                    path_cache[node.address] = get_code_path(node);
                }
                catch (...) {
                    node_cache[node.address] = {};
                    name_cache[node.address] = "Error";
                    class_cache[node.address] = "Unknown";
                    path_cache[node.address] = "Error";
                }
            }
            };

        try {
            auto& datamodel = globals::datamodel;
            roblox::instance root(datamodel.address);

            // Rebuilding the cache walks the whole tree, so it costs a frame. Only do it when
            // the mouse is up, otherwise the hitch lands in the middle of a drag.
            auto now = std::chrono::steady_clock::now();
            if (!ImGui::IsAnyMouseDown() &&
                std::chrono::duration_cast<std::chrono::seconds>(now - last_cache_refresh).count() >= 5) {
                node_cache.clear();
                name_cache.clear();
                class_cache.clear();
                path_cache.clear();
                cache_initialized = false;
                last_cache_refresh = now;
            }

            if (!cache_initialized) {
                cache_node(root);
                cache_initialized = true;
            }

            const ImVec2 display = ImGui::GetIO().DisplaySize;
            const ImVec2 dex_size(
                ImMin(560.0f, display.x - 16.0f),
                ImMin(680.0f, display.y - 16.0f));

            ImGui::SetNextWindowSize(dex_size, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 360.0f), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::Begin("DEX##Main", nullptr, flags);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 win_pos = ImGui::GetWindowPos();
            ImVec2 win_size = ImGui::GetWindowSize();

            if (overlay::visible) {
                dex_pos = win_pos;
            }

            ImU32 header_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
            draw->AddRectFilled(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + 2), header_color);
            draw->AddText(ImVec2(win_pos.x + 8, win_pos.y + 6), IM_COL32(255, 255, 255, 255), "DEX Explorer");

            ImGui::SetCursorPos(ImVec2(8, 25));

            // Leave room for Clear and Refresh so the row cannot overflow the window.
            const float button_row = 45.0f + 50.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
            ImGui::PushItemWidth(ImMax(120.0f, ImGui::GetContentRegionAvail().x - button_row));

            bool search_changed = ImGui::InputTextWithHint("##Search", "Search...", search_query, sizeof(search_query));
            ImGui::PopItemWidth();

            if (search_changed) {
                search_results.clear();
                show_search_results = strlen(search_query) > 0;

                if (show_search_results && strlen(search_query) >= 2) {
                    std::string query = search_query;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                    std::function<void(roblox::instance&)> search_recursive = [&](roblox::instance& inst) {
                        if (search_results.size() >= 30) return;

                        cache_node(inst);
                        std::string name = name_cache[inst.address];
                        std::string class_name = class_cache[inst.address];

                        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                        std::transform(class_name.begin(), class_name.end(), class_name.begin(), ::tolower);

                        if (name.find(query) != std::string::npos || class_name.find(query) != std::string::npos) {
                            search_results.push_back(inst);
                        }

                        try {
                            for (auto& child : inst.get_children()) {
                                search_recursive(child);
                            }
                        }
                        catch (...) {}
                        };

                    try {
                        if (auto workspace = root.findfirstchild("Workspace"); workspace.address != 0) {
                            search_recursive(workspace);
                        }
                        if (globals::players.address != 0) {
                            for (auto& player : globals::players.get_children()) {
                                search_recursive(player);
                            }
                        }
                    }
                    catch (...) {}
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear", ImVec2(45, 0))) {
                memset(search_query, 0, sizeof(search_query));
                search_results.clear();
                show_search_results = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(50, 0))) {
                node_cache.clear();
                name_cache.clear();
                class_cache.clear();
                path_cache.clear();
                cache_initialized = false;
            }

            // Both panes size themselves from what is left, so nothing is clipped when the
            // window is resized or the display is short.
            const ImGuiStyle& dex_style = ImGui::GetStyle();
            const float dex_width = ImGui::GetContentRegionAvail().x;
            const float dex_remaining = ImGui::GetContentRegionAvail().y;

            // Reserve the "Properties" header and the footer line as well, or the panes push
            // the node count off the bottom of the window.
            const float props_header = ImGui::GetTextLineHeightWithSpacing() + dex_style.ItemSpacing.y;
            const float footer_line = ImGui::GetTextLineHeightWithSpacing() + dex_style.ItemSpacing.y;
            const float panes_height = ImMax(160.0f, dex_remaining - props_header - footer_line);
            const float tree_height = ImMax(80.0f, panes_height * 0.55f);
            const float props_height = ImMax(80.0f, panes_height - tree_height);

            ImGui::BeginChild("Tree", ImVec2(dex_width, tree_height), true);

            if (show_search_results && strlen(search_query) > 0) {
                ImGui::Text("Results (%d):", (int)search_results.size());
                ImGui::Separator();

                for (auto& result : search_results) {
                    if (!result.address) continue;

                    cache_node(result);
                    ImGui::PushID((void*)result.address);

                    std::string name = name_cache[result.address];
                    std::string class_name = class_cache[result.address];
                    ImVec4 color = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
                    if (class_name == "Part") color = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
                    else if (class_name == "Model") color = ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
                    else if (class_name.find("Script") != std::string::npos) color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);

                    draw_dex_icon(class_name, ImGui::ColorConvertFloat4ToU32(color));

                    bool is_selected = (selected_instance.address == result.address);
                    if (ImGui::Selectable((name + " [" + class_name + "]").c_str(), is_selected)) {
                        selected_instance = result;
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Copy Address")) {
                            std::string addr = "0x" + std::to_string(result.address);
                            ImGui::SetClipboardText(addr.c_str());
                        }
                        if (ImGui::MenuItem("Copy Code Path")) {
                            ImGui::SetClipboardText(path_cache[result.address].c_str());
                        }
                        if (ImGui::MenuItem("Teleport To")) {
                            try {
                                Vec3 pos = result.get_pos();
                                pos.y += 5.0f;
                                globals::lp.hrp.write_position(pos);
                            }
                            catch (...) {}
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }
            else {
                std::function<void(roblox::instance&)> render_tree;
                render_tree = [&](roblox::instance& node) {
                    cache_node(node);

                    ImGui::PushID((void*)node.address);

                    std::string name = name_cache[node.address];
                    std::string class_name = class_cache[node.address];
                    ImVec4 color = ImVec4(0.7f, 0.7f, 1.0f, 1.0f);
                    if (class_name == "Part") color = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
                    else if (class_name == "Model") color = ImVec4(0.6f, 1.0f, 0.6f, 1.0f);
                    else if (class_name.find("Script") != std::string::npos) color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);

                    auto& children = node_cache[node.address];
                    bool has_children = !children.empty();

                    ImGuiTreeNodeFlags tree_flags = has_children ? 0 : ImGuiTreeNodeFlags_Leaf;
                    tree_flags |= ImGuiTreeNodeFlags_OpenOnArrow;

                    if (selected_instance.address == node.address) {
                        tree_flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    draw_dex_icon(class_name, ImGui::ColorConvertFloat4ToU32(color));

                    bool is_open = ImGui::TreeNodeEx((name + "##" + std::to_string(node.address)).c_str(), tree_flags);

                    if (ImGui::IsItemClicked()) {
                        selected_instance = node;
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Copy Address")) {
                            std::string addr = "0x" + std::to_string(node.address);
                            ImGui::SetClipboardText(addr.c_str());
                        }
                        if (ImGui::MenuItem("Copy Code Path")) {
                            ImGui::SetClipboardText(path_cache[node.address].c_str());
                        }
                        if (ImGui::MenuItem("Teleport To")) {
                            try {
                                Vec3 pos = node.get_pos();
                                pos.y += 5.0f;
                                globals::lp.hrp.write_position(pos);
                            }
                            catch (...) {}
                        }
                        if (ImGui::MenuItem("Toggle Collision")) {
                            try {
                                bool current = node.get_cancollide();
                                node.write_cancollide(!current);
                            }
                            catch (...) {}
                        }
                        ImGui::EndPopup();
                    }

                    if (is_open) {
                        for (auto& child : children) {
                            render_tree(child);
                        }
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                    };

                try {
                    for (auto& child : node_cache[root.address]) {
                        render_tree(child);
                    }
                }
                catch (...) {
                    ImGui::Text("Error rendering tree");
                }
            }

            ImGui::EndChild();

            ImGui::Text("Properties");
            ImGui::Separator();

            ImGui::BeginChild("Props", ImVec2(dex_width, props_height), true);

            // Drop a selection that has since become unreadable instead of faulting on it.
            if (selected_instance.address != 0 && !mem::is_readable(selected_instance.address))
                selected_instance = roblox::instance();

            if (selected_instance.address != 0) {
                const char* tabs[] = { "Basic", "Transform", "Values", "Advanced" };
                for (int i = 0; i < 4; i++) {
                    if (i > 0) ImGui::SameLine();
                    if (ImGui::Button(tabs[i], ImVec2(90, 20))) {
                        selected_tab = i;
                    }
                }
                ImGui::Separator();

                std::string name = name_cache[selected_instance.address];
                std::string class_name = class_cache[selected_instance.address];

                if (selected_tab == 0) {
                    ImGui::Text("Name: %s", name.c_str());
                    ImGui::Text("Class: %s", class_name.c_str());
                    ImGui::Text("Address: 0x%llX", selected_instance.address);
                    if (ImGui::IsItemClicked()) {
                        std::string addr = "0x" + std::to_string(selected_instance.address);
                        ImGui::SetClipboardText(addr.c_str());
                    }
                    ImGui::Text("Code Path:");
                    ImGui::TextWrapped("%s", path_cache[selected_instance.address].c_str());
                }
                else if (selected_tab == 1) {
                    try {
                        Vec3 pos = selected_instance.get_pos();
                        ImGui::Text("Position:");
                        vec3_value = pos;
                        if (ImGui::DragFloat3("##pos", &vec3_value.x, 0.1f)) {
                            selected_instance.write_position(vec3_value);
                        }
                    }
                    catch (...) {
                        ImGui::Text("Position: N/A");
                    }

                    try {
                        Vec3 size = selected_instance.get_part_size();
                        ImGui::Text("Size:");
                        vec3_value = size;
                        if (ImGui::DragFloat3("##size", &vec3_value.x, 0.1f, 0.1f)) {
                            selected_instance.write_part_size(vec3_value);
                        }
                    }
                    catch (...) {}

                    try {
                        bool can_collide = selected_instance.get_cancollide();
                        bool_value = can_collide;
                        if (ImGui::Checkbox("CanCollide", &bool_value)) {
                            selected_instance.write_cancollide(bool_value);
                        }
                    }
                    catch (...) {}

                    if (ImGui::Button("Teleport", ImVec2(80, 20))) {
                        try {
                            Vec3 pos = selected_instance.get_pos();
                            pos.y += 5.0f;
                            globals::lp.hrp.write_position(pos);
                        }
                        catch (...) {}
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Anchor", ImVec2(60, 20))) {
                        try { selected_instance.setAnchored(true); }
                        catch (...) {}
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Unanchor", ImVec2(70, 20))) {
                        try { selected_instance.setAnchored(false); }
                        catch (...) {}
                    }
                }
                else if (selected_tab == 2) {
                    if (class_name.find("Value") != std::string::npos) {
                        if (class_name == "Vector3Value") {
                            try {
                                Vec3 value = selected_instance.read_vector3_value();
                                ImGui::Text("Value:");
                                vec3_value = value;
                                if (ImGui::DragFloat3("##vec3val", &vec3_value.x, 0.1f)) {
                                    selected_instance.write_vector3_value(vec3_value);
                                }
                            }
                            catch (...) {}
                        }
                        else if (class_name == "IntValue") {
                            try {
                                int value = selected_instance.read_int_value();
                                ImGui::Text("Value:");
                                int_value = value;
                                if (ImGui::DragInt("##intval", &int_value)) {
                                    selected_instance.write_int_value(int_value);
                                }
                            }
                            catch (...) {}
                        }
                        else if (class_name == "NumberValue") {
                            try {
                                float value = selected_instance.read_float_value();
                                ImGui::Text("Value:");
                                float_value = value;
                                if (ImGui::DragFloat("##numval", &float_value)) {
                                    selected_instance.write_float_value(float_value);
                                }
                            }
                            catch (...) {}
                        }
                        else if (class_name == "BoolValue") {
                            try {
                                bool value = selected_instance.read_bool_value();
                                bool_value = value;
                                if (ImGui::Checkbox("Value", &bool_value)) {
                                    selected_instance.write_bool_value(bool_value);
                                }
                            }
                            catch (...) {}
                        }
                    }
                    else {
                        ImGui::Text("No value properties");
                    }
                }
                else if (selected_tab == 3) {
                    if (class_name == "Humanoid") {
                        try {
                            float health = selected_instance.read_health();
                            ImGui::Text("Health:");
                            float_value = health;
                            if (ImGui::DragFloat("##health", &float_value, 1.0f, 0.0f, 1000.0f)) {
                                selected_instance.write_health(float_value);
                            }
                        }
                        catch (...) {}

                        try {
                            float speed = selected_instance.read_walkspeed();
                            ImGui::Text("WalkSpeed:");
                            float_value = speed;
                            if (ImGui::DragFloat("##speed", &float_value, 1.0f, 0.0f, 100.0f)) {
                                selected_instance.write_walkspeed(float_value);
                            }
                        }
                        catch (...) {}
                    }
                    else if (class_name == "Animation") {
                        try {
                            std::string anim_id = selected_instance.read_animationid();
                            ImGui::Text("AnimationId:");
                            strcpy_s(edit_buffer, anim_id.c_str());
                            if (ImGui::InputText("##animid", edit_buffer, sizeof(edit_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                                selected_instance.write_animationid(std::string(edit_buffer));
                            }
                        }
                        catch (...) {}
                    }
                    else if (class_name == "Part" || class_name == "MeshPart") {
                        ImGui::Text("Transparency:");
                        if (ImGui::SliderFloat("##trans", &float_value, 0.0f, 1.0f)) {
                            try { selected_instance.write_transparency(float_value); }
                            catch (...) {}
                        }

                        if (ImGui::Button("Invisible", ImVec2(70, 20))) {
                            try { selected_instance.write_transparency(1.0f); }
                            catch (...) {}
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Visible", ImVec2(60, 20))) {
                            try { selected_instance.write_transparency(0.0f); }
                            catch (...) {}
                        }
                    }
                    else {
                        ImGui::Text("No advanced properties");
                    }
                }
            }
            else {
                ImGui::Text("No instance selected");
            }

            ImGui::EndChild();

            ImGui::Text("Nodes: %d | Selected: %s", (int)node_cache.size(),
                selected_instance.address ? name_cache[selected_instance.address].c_str() : "None");

            ImGui::End();
            style.WindowRounding = old_rounding;

        }
        catch (...) {
            ImGui::Begin("DEX Error", nullptr, flags);
            ImGui::Text("DEX Explorer Error");
            ImGui::End();
        }
    }

    static bool IsMouseButton(int key) {
        return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON ||
            key == VK_XBUTTON1 || key == VK_XBUTTON2;
    }

    bool loaded = false;
    void LoadInterface() {
        MSG msg;






        while (!should_exit) {
            while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    should_exit = true;
                    break;
                }
            }
            if (should_exit) break;
            HandleResize();
            HWND roblox = FindWindowA(nullptr, "Roblox");
            HWND foreground = GetForegroundWindow();
            OverlayManager::BeginFrame();

            if (foreground == roblox && visible) {
                SetOverlayTransparency(!visible);
            }
            else if (foreground == window_config.hwnd) {
                SetOverlayTransparency(!visible);
            }
            else {
                SetOverlayTransparency(true);
            }
            if (foreground != window_config.hwnd && foreground != roblox) {
                SetOverlayTransparency(true);
            }

            // Keep the bar usable while the menu is closed: the overlay is click-through in
            // that state, so take the cursor position from the OS and re-enable input while
            // it is over the bar.
            POINT cursor{};
            if (GetCursorPos(&cursor)) {
                const auto inside = [&cursor](const RECT& rect) {
                    return cursor.x >= rect.left && cursor.x <= rect.right &&
                           cursor.y >= rect.top && cursor.y <= rect.bottom;
                };

                const bool over_bar = topbar_rect_valid && inside(topbar_rect);
                const bool over_hud = (watermark_rect_valid && inside(watermark_rect)) ||
                                      (keybind_rect_valid && inside(keybind_rect));

                if (over_bar || over_hud)
                    SetOverlayTransparency(false);
            }

            if (foreground != roblox || visible) {
                globals::menuactive = true;
            }
            else {
                globals::menuactive = false;
            }

            // The menu bind must never be a mouse button, otherwise every click in the
            // menu toggles it shut. Repair a mouse bound from an older config and ignore
            // one chosen while binding.
            if (IsMouseButton(globals::menubind.key)) {
                globals::menubind.key = VK_INSERT;
                globals::menubind.waiting_for_input = false;
            }

            static bool menu_key_was_down = false;
            const bool menu_key_down = globals::menubind.key != 0 &&
                !globals::menubind.waiting_for_input &&
                (GetAsyncKeyState(globals::menubind.key) & 0x8000) != 0;
            if (menu_key_down && !menu_key_was_down) {
                visible = !visible;
                topbar_visible = visible;
                SetOverlayTransparency(!visible);
            }
            menu_key_was_down = menu_key_down;

            UpdateWindowPosition(window_config.hwnd);

            ForceOverlayOnTop();
            BeginRender();
            RenderFrame();
            EndRender();
            PresentFrame();
            if (!loaded) {
                SetOverlayTransparency(false);
                visible = true;
                loaded = true;
            }
        }
    }

    static HANDLE overlay_thread_handle = nullptr;
    std::atomic<bool> overlay_running = false;

    DWORD WINAPI OverlayThread(LPVOID) {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
            return 1;
        }

        // 1 ms timer resolution: without it every sleep lands on a ~15 ms tick, which makes the
        // frame limiter stutter and window dragging feel laggy.
        timeBeginPeriod(1);

        if (!Initialize()) {
            CleanupDeviceD3D();
            CoUninitialize();
            return 1;
        }

        LoadInterface();

        Shutdown();
        timeEndPeriod(1);
        CoUninitialize();
        return 0;
    }

    bool InitializeAndRun() {
        if (overlay_running) return true;

        overlay_running = true;
        overlay_thread_handle = CreateThread(
            nullptr,
            0,
            OverlayThread,
            nullptr,
            0,
            nullptr
        );

        return overlay_thread_handle != nullptr;
    }

    // ---- Player headshots ----
    // Roblox headshots are fetched from the thumbnail API, decoded with WIC and uploaded as a DX11
    // texture, then cached per user id: the smallest path from a user id to something AddImage can
    // draw, since the project has no texture pipeline of its own.

    // Two panes: the roster and the selected player. Shared by the floating window and the
    // Player tab, so both stay in step.
    void player_panes() {
        static char search[128] = "";
        static uintptr_t selected = 0;

        // Teleport is re-applied for a few frames; a single position write is usually overridden
        // by the character controller before it lands.
        static Vec3 teleport_target{};
        static int teleport_frames = 0;

        // Copy under the cache lock: the player cache replaces its vector from another thread, so
        // holding pointers into it gave the actions stale addresses.
        const std::vector<roblox::player> players = player_access::get_cached_players();

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float list_width = ImMax(220.0f, avail.x * 0.52f - style.ItemSpacing.x * 0.5f);

        std::string query = search;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        auto matches = [&](const roblox::player& player) {
            if (query.empty()) return true;

            std::string name = player.name;
            std::string display = player.displayname;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::transform(display.begin(), display.end(), display.begin(), ::tolower);

            return name.find(query) != std::string::npos || display.find(query) != std::string::npos;
        };

        auto status_for = [&](const roblox::player& player, const char*& text) -> ImVec4 {
            text = "";

            if (!globals::lp.name.empty() && player.name == globals::lp.name) {
                text = "Client";
                return ImVec4(0.55f, 0.58f, 1.0f, 1.0f);
            }

            const int relation = player_relations::get(player.name);

            if (relation == 1) {
                text = "Friendly";
                return ImVec4(0.35f, 0.80f, 0.45f, 1.0f);
            }
            if (relation == 2) {
                text = "Enemy";
                return ImVec4(0.92f, 0.38f, 0.38f, 1.0f);
            }

            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        // ---------- roster ----------
        ImGui::BeginChild("##pl_roster", ImVec2(list_width, avail.y), true);

        int listed = 0;
        for (auto& player : players) {
            if (player.name.empty() || !player.is_valid() || !matches(player)) continue;
            ++listed;
        }

        ImGui::Text("Players [%d]", listed);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##pl_search", "Search player...", search, sizeof(search));

        ImGui::Spacing();

        const float roster_right = ImGui::GetWindowContentRegionMax().x;

        ImGui::TextDisabled("Name");
        ImGui::SameLine(ImMax(0.0f, roster_right - 48.0f));
        ImGui::TextDisabled("Status");
        ImGui::Separator();

        for (auto& player : players) {
            if (player.name.empty() || !player.is_valid() || !matches(player)) continue;

            ImGui::PushID(player.name.c_str());

            const bool is_selected = (selected == player.address);
            const char* label = player.displayname.empty() ? player.name.c_str() : player.displayname.c_str();

            if (ImGui::Selectable(label, is_selected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, 18.0f)))
                selected = player.address;

            const char* status_text = "";
            const ImVec4 status_color = status_for(player, status_text);

            if (status_text[0] != '\0') {
                const float text_width = ImGui::CalcTextSize(status_text).x;
                ImGui::SameLine(ImMax(0.0f, roster_right - text_width - 6.0f));
                ImGui::TextColored(status_color, "%s", status_text);
            }

            ImGui::PopID();
        }

        if (listed == 0) {
            ImGui::TextDisabled(players.empty()
                ? "No players cached yet."
                : "No player matches that search.");
        }

        ImGui::EndChild();
        ImGui::SameLine();

        // ---------- selected player ----------
        ImGui::BeginChild("##pl_detail", ImVec2(0.0f, avail.y), true);

        roblox::player target;
        bool have_target = false;

        for (auto& player : players) {
            if (player.address == selected && player.is_valid()) {
                target = player;
                have_target = true;
                break;
            }
        }

        if (!have_target) {
            ImGui::TextDisabled("Selected Player");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Pick a player on the left.");
        }
        else {
            const float pane_width = ImGui::GetContentRegionAvail().x;

            const float avatar_radius = 26.0f;
            const ImVec2 avatar_origin = ImGui::GetCursorScreenPos();
            const ImVec2 centre(avatar_origin.x + pane_width * 0.5f, avatar_origin.y + avatar_radius + 4.0f);

            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddCircleFilled(centre, avatar_radius, IM_COL32(34, 34, 48, 255), 32);

            // Placeholder silhouette until a real model is supplied.
            dl->AddCircleFilled(ImVec2(centre.x, centre.y - 7.0f), 7.0f, IM_COL32(196, 196, 216, 255), 16);
            dl->AddRectFilled(ImVec2(centre.x - 12.0f, centre.y + 3.0f),
                ImVec2(centre.x + 12.0f, centre.y + 19.0f), IM_COL32(196, 196, 216, 255), 7.0f);

            dl->AddCircle(centre, avatar_radius, IM_COL32(96, 90, 170, 255), 32, 1.6f);

            ImGui::Dummy(ImVec2(1.0f, avatar_radius * 2.0f + 10.0f));

            const std::string shown = target.displayname.empty() ? target.name : target.displayname;
            const std::string sub = target.displayname.empty() ? std::string() : "(" + target.name + ")";

            const float name_width = ImGui::CalcTextSize(shown.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (pane_width - name_width) * 0.5f));
            ImGui::TextUnformatted(shown.c_str());

            if (!sub.empty()) {
                const float sub_width = ImGui::CalcTextSize(sub.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, (pane_width - sub_width) * 0.5f));
                ImGui::TextDisabled("%s", sub.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("Status");

            int relation = player_relations::get(target.name);
            const char* statuses[] = { "None", "Friendly", "Enemy" };

            ImGui::SetNextItemWidth(-1.0f);
            SDK::Gui::ComboBox("##pl_status", &relation, statuses, 3);

            player_relations::set(target.name, relation);

            ImGui::Spacing();
            ImGui::Spacing();

            const float button_w = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f;
            const ImVec2 button_size(button_w, 24.0f);

            if (ImGui::Button("Teleport", button_size)) {
                try {
                    if (target.get_position().x != 0.0f || target.get_position().y != 0.0f) {
                        Vec3 landing = target.get_position();
                        landing.y += 3.0f;

                        teleport_target = landing;
                        teleport_frames = 15;
                    }
                }
                catch (...) {}
            }

            ImGui::SameLine();

            if (ImGui::Button("Spectate", button_size)) {
                try {
                    if (target.humanoid.is_valid())
                        target.humanoid.spectate(target.humanoid.address);
                }
                catch (...) {}
            }

            if (ImGui::Button("Unspectate", button_size)) {
                try {
                    if (globals::lp.humanoid.is_valid())
                        globals::lp.humanoid.unspectate();
                }
                catch (...) {}
            }

            ImGui::SameLine();

            if (ImGui::Button("Fling", button_size)) {
                try {
                    if (target.hrp.is_valid())
                        target.hrp.write_velocity(Vec3(120000.0f, 120000.0f, 120000.0f));
                }
                catch (...) {}
            }
        }

        if (teleport_frames > 0) {
            try {
                if (globals::lp.hrp.is_valid())
                    globals::lp.hrp.write_position(teleport_target);
            }
            catch (...) {}

            --teleport_frames;
        }

    }

    void RenderPlayerTab() {
        const float child_width = (ImGui::GetContentRegionAvail().x / 2.0f) - 4.0f;

        // Self controls: these four cards all change your own character.
        ImGui::BeginGroup();
        {
                 ImAdd::BeginChild("Speed", ImVec2(child_width, 145));
                 {
                     SDK::Gui::Checkbox("Enabled  ", &features::playermodifications::speedenabled);

                     std::string current_name = features::playermodifications::speedkeybind.get_key_name();
                     if (features::playermodifications::speedkeybind.waiting_for_input) {
                         current_name = "[...]";
                     }
                     float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                     float bind_width = ImMax(60.0f, text_width + 16.0f);

                     SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                     ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                     OverlayManager::OptimizedKeybind(&features::playermodifications::speedkeybind, ImVec2(bind_width, 18.0f));

                     const char* speedmodes[] = { "Velocity", "Walkspeed", "Position" };
                     SDK::Gui::ComboBox("Speed Mode", &features::playermodifications::speedtype, speedmodes, 3);

                     SDK::Gui::SliderFloat("Speed", &features::playermodifications::speed, 0.1f, 1250);
                 }
                 ImAdd::EndChild();
                 ImAdd::BeginChild("JumpPower", ImVec2(child_width, 95));
                 {
                     SDK::Gui::Checkbox("Enabled   ", &features::playermodifications::jumpenabled);

                     std::string current_name = features::playermodifications::jumpkeybind.get_key_name();
                     if (features::playermodifications::jumpkeybind.waiting_for_input) {
                         current_name = "[...]";
                     }
                     float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                     float bind_width = ImMax(60.0f, text_width + 16.0f);

                     SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                     ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                     OverlayManager::OptimizedKeybind(&features::playermodifications::jumpkeybind, ImVec2(bind_width, 18.0f));
                     SDK::Gui::SliderFloat("Power", &features::playermodifications::jumppower, 0.1f, 1250);
                 }
                 ImAdd::EndChild();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
                 ImAdd::BeginChild("Flight", ImVec2(child_width, 145));
                 {
                     SDK::Gui::Checkbox("Enabled     ", &features::playermodifications::flightenabled);

                     std::string current_name = features::playermodifications::flightkeybind.get_key_name();
                     if (features::playermodifications::flightkeybind.waiting_for_input) {
                         current_name = "[...]";
                     }
                     float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                     float bind_width = ImMax(60.0f, text_width + 16.0f);

                     SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                     ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                     OverlayManager::OptimizedKeybind(&features::playermodifications::flightkeybind, ImVec2(bind_width, 18.0f));

                     const char* flightmodes[] = { "Position", "Velocity" };
                     SDK::Gui::ComboBox("Flight Mode", &features::playermodifications::flighttype, flightmodes, 2);

                     SDK::Gui::SliderFloat("Flight Speed", &features::playermodifications::flightspeed, 0.1f, 1250);
                 }
                 ImAdd::EndChild();

                 ImAdd::BeginChild("Freecam", ImVec2(child_width, 95));
                 {
                     SDK::Gui::Checkbox("Enabled   ", &features::playermodifications::freecamenabled);

                     std::string current_name = features::playermodifications::freecamkeybind.get_key_name();
                     if (features::playermodifications::freecamkeybind.waiting_for_input) {
                         current_name = "[...]";
                     }
                     float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                     float bind_width = ImMax(60.0f, text_width + 16.0f);

                     SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                     ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                     OverlayManager::OptimizedKeybind(&features::playermodifications::freecamkeybind, ImVec2(bind_width, 18.0f));
                     SDK::Gui::SliderFloat("Power", &features::playermodifications::freecamspeed, 0.1f, 10);
                 }
                 ImAdd::EndChild();
        }
        ImGui::EndGroup();
    }

    void playerlist() {
        if (!globals::player_list) return;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

        if (!visible)
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowSize(ImVec2(640.0f, 440.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(160.0f, 140.0f), ImGuiCond_FirstUseEver);

        ImGui::Begin("Player List##purge", nullptr, flags);
        player_panes();
        ImGui::End();
    }


    // Album art for the panel. The worker decodes it to RGBA; the texture is created here
    // because the D3D device belongs to the render thread. One texture, replaced per track.
    struct album_texture_t {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* view = nullptr;
        unsigned revision = 0;
    };

    static ImTextureID album_texture_for(const music::now_playing_t& track) {
        static album_texture_t art;

        if (track.art_revision == art.revision)
            return (ImTextureID)art.view;

        if (art.view) {
            art.view->Release();
            art.view = nullptr;
        }

        if (art.texture) {
            art.texture->Release();
            art.texture = nullptr;
        }

        art.revision = track.art_revision;

        if (!d3d_resources.device || !track.art || track.art->empty()) return nullptr;
        if (track.art_width <= 0 || track.art_height <= 0) return nullptr;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = (UINT)track.art_width;
        desc.Height = (UINT)track.art_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = track.art->data();
        data.SysMemPitch = (UINT)track.art_width * 4;

        if (FAILED(d3d_resources.device->CreateTexture2D(&desc, &data, &art.texture)))
            return nullptr;

        D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {};
        view_desc.Format = desc.Format;
        view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view_desc.Texture2D.MipLevels = 1;

        if (FAILED(d3d_resources.device->CreateShaderResourceView(art.texture, &view_desc, &art.view))) {
            art.texture->Release();
            art.texture = nullptr;
            return nullptr;
        }

        return (ImTextureID)art.view;
    }

    // The panel is just the cover and the track: no title bar, no app/status footer, on solid
    // black. With no title bar there is no handle either, so pressing anywhere on it drags it.
    static void RenderMusicPanel() {
        static ImVec2 panel_pos(-1.0f, 48.0f);
        static bool dragging = false;

        if (!globals::music_player) return;

        const music::now_playing_t track = music::current();
        ImGuiIO& io = ImGui::GetIO();

        constexpr float panel_width = 214.0f;
        constexpr float panel_height = 90.0f;
        constexpr float art_size = 40.0f;
        constexpr float pad = 10.0f;
        constexpr float gap = 8.0f;

        if (panel_pos.x < 0.0f && io.DisplaySize.x > panel_width)
            panel_pos = ImVec2((io.DisplaySize.x - panel_width) * 0.5f, 48.0f);

        ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panel_width, panel_height), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

        if (ImGui::Begin("##music_panel", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav)) {
            // The control row is a strip of buttons, so a press there must not start a drag.
            const ImVec2 window_pos = ImGui::GetWindowPos();
            const float controls_top = window_pos.y + pad + art_size + 4.0f;
            const bool over_controls = ImGui::IsMouseHoveringRect(ImVec2(window_pos.x, controls_top),
                ImVec2(window_pos.x + panel_width, controls_top + 28.0f));

            if (ImGui::IsWindowHovered() && !over_controls && !dragging &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                dragging = true;

            if (dragging) {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    dragging = false;
                }
                else if (io.DisplaySize.x > panel_width && io.DisplaySize.y > panel_height) {
                    panel_pos.x = ImClamp(panel_pos.x + io.MouseDelta.x, 0.0f, io.DisplaySize.x - panel_width);
                    panel_pos.y = ImClamp(panel_pos.y + io.MouseDelta.y, 0.0f, io.DisplaySize.y - panel_height);
                }
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();

            const ImTextureID art = album_texture_for(track);
            const ImVec2 art_pos = ImGui::GetCursorScreenPos();

            if (art) {
                ImGui::Image(art, ImVec2(art_size, art_size));
            }
            else {
                // Same footprint when a track has no artwork, so the panel never jumps around.
                ImGui::Dummy(ImVec2(art_size, art_size));

                draw->AddRectFilled(art_pos, ImVec2(art_pos.x + art_size, art_pos.y + art_size),
                    IM_COL32(16, 16, 20, 255), 4.0f);

                const ImU32 note = IM_COL32(104, 104, 118, 255);
                const float cx = art_pos.x + art_size * 0.5f;
                const float cy = art_pos.y + art_size * 0.5f;

                draw->AddLine(ImVec2(cx + 1.5f, cy - 6.5f), ImVec2(cx + 1.5f, cy + 4.0f), note, 1.5f);
                draw->AddLine(ImVec2(cx + 1.5f, cy - 6.5f), ImVec2(cx + 6.5f, cy - 3.0f), note, 1.5f);
                draw->AddCircleFilled(ImVec2(cx - 1.0f, cy + 4.5f), 3.4f, note);
            }

            ImGui::SameLine(0.0f, gap);

            const float line_h = ImGui::GetTextLineHeightWithSpacing();
            const bool two_lines = track.has_track && !track.artist.empty();
            const float text_h = two_lines ? line_h * 2.0f : line_h;

            const float text_x = ImGui::GetCursorScreenPos().x;
            const float text_top = art_pos.y + (art_size - text_h) * 0.5f;
            const float text_right = art_pos.x + panel_width - pad;

            // One line each, clipped: at this size a wrapped title would push the artist out.
            draw->PushClipRect(ImVec2(text_x, art_pos.y), ImVec2(text_right, art_pos.y + art_size), true);

            if (track.has_track) {
                draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_x, text_top),
                    IM_COL32(238, 238, 244, 255), track.title.c_str());

                if (two_lines)
                    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(text_x, text_top + line_h),
                        IM_COL32(138, 138, 156, 255), track.artist.c_str());
            }
            else {
                draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(text_x, text_top),
                    IM_COL32(128, 128, 140, 255), "Nothing playing");
            }

            draw->PopClipRect();

            // Transport controls, under the cover and the track. Pressing one queues it for the
            // media thread; the panel redraws from the next snapshot, so the button follows state.
            const float row_top = art_pos.y + art_size + 6.0f;
            const float row_mid = row_top + 12.0f;
            const float controls_width = 20.0f + 14.0f + 24.0f + 14.0f + 20.0f;
            float control_x = art_pos.x + (panel_width - pad * 2.0f - controls_width) * 0.5f;

            const ImU32 idle_color = IM_COL32(146, 146, 162, 255);
            const ImU32 hover_color = IM_COL32(238, 238, 244, 255);

            {
                ImGui::SetCursorScreenPos(ImVec2(control_x, row_top));
                ImGui::InvisibleButton("##music_prev", ImVec2(20.0f, 24.0f));

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    music::send(music::control_previous);

                const ImU32 color = ImGui::IsItemHovered() ? hover_color : idle_color;

                draw->AddRectFilled(ImVec2(control_x + 2.0f, row_mid - 6.0f),
                    ImVec2(control_x + 4.0f, row_mid + 6.0f), color);
                draw->AddTriangleFilled(ImVec2(control_x + 16.0f, row_mid - 6.0f),
                    ImVec2(control_x + 16.0f, row_mid + 6.0f), ImVec2(control_x + 5.5f, row_mid), color);
            }

            control_x += 34.0f;

            {
                ImGui::SetCursorScreenPos(ImVec2(control_x, row_top));
                ImGui::InvisibleButton("##music_play", ImVec2(24.0f, 24.0f));

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    music::send(music::control_play_pause);

                const ImVec2 center(control_x + 12.0f, row_mid);
                const ImU32 disc = ImGui::IsItemHovered() ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(224, 224, 232, 255);

                draw->AddCircleFilled(center, 11.0f, disc);

                // Playing shows the two pause bars, paused shows the play triangle.
                if (track.playing) {
                    draw->AddRectFilled(ImVec2(center.x - 4.5f, center.y - 5.0f),
                        ImVec2(center.x - 1.5f, center.y + 5.0f), IM_COL32(8, 8, 12, 255));
                    draw->AddRectFilled(ImVec2(center.x + 1.5f, center.y - 5.0f),
                        ImVec2(center.x + 4.5f, center.y + 5.0f), IM_COL32(8, 8, 12, 255));
                }
                else {
                    draw->AddTriangleFilled(ImVec2(center.x - 3.5f, center.y - 5.5f),
                        ImVec2(center.x - 3.5f, center.y + 5.5f), ImVec2(center.x + 5.5f, center.y),
                        IM_COL32(8, 8, 12, 255));
                }
            }

            control_x += 38.0f;

            {
                ImGui::SetCursorScreenPos(ImVec2(control_x, row_top));
                ImGui::InvisibleButton("##music_next", ImVec2(20.0f, 24.0f));

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    music::send(music::control_next);

                const ImU32 color = ImGui::IsItemHovered() ? hover_color : idle_color;

                draw->AddTriangleFilled(ImVec2(control_x + 4.0f, row_mid - 6.0f),
                    ImVec2(control_x + 4.0f, row_mid + 6.0f), ImVec2(control_x + 14.5f, row_mid), color);
                draw->AddRectFilled(ImVec2(control_x + 15.0f, row_mid - 6.0f),
                    ImVec2(control_x + 17.0f, row_mid + 6.0f), color);
            }
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    enum bar_icon {
        bar_icon_menu,
        bar_icon_eye,
        bar_icon_globe,
        bar_icon_person,
        bar_icon_note
    };

    // Bar icons are drawn from primitives: the bundled ProggyClean atlas has no icon glyphs, and
    // a second font just for five symbols is not worth the atlas.
    static void draw_bar_icon(ImDrawList* draw, bar_icon icon, ImVec2 c, ImU32 color) {
        switch (icon) {
        case bar_icon_menu: {
            // A sheet of lines with a folded corner.
            const ImVec2 min(c.x - 4.5f, c.y - 6.0f);
            const ImVec2 max(c.x + 4.5f, c.y + 6.0f);

            draw->AddRect(min, max, color, 1.5f, 0, 1.2f);
            draw->AddLine(ImVec2(min.x + 5.0f, min.y), ImVec2(min.x + 5.0f, min.y + 3.5f), color, 1.2f);
            draw->AddLine(ImVec2(min.x + 5.0f, min.y + 3.5f), ImVec2(max.x, min.y + 3.5f), color, 1.2f);

            for (int i = 0; i < 3; ++i) {
                const float y = min.y + 6.0f + i * 2.0f;
                draw->AddLine(ImVec2(min.x + 1.8f, y),
                    ImVec2(max.x - (i == 2 ? 3.0f : 1.8f), y), color, 1.0f);
            }

            break;
        }
        case bar_icon_eye:
            draw->AddBezierQuadratic(ImVec2(c.x - 6.5f, c.y), ImVec2(c.x, c.y - 6.0f),
                ImVec2(c.x + 6.5f, c.y), color, 1.3f);
            draw->AddBezierQuadratic(ImVec2(c.x - 6.5f, c.y), ImVec2(c.x, c.y + 6.0f),
                ImVec2(c.x + 6.5f, c.y), color, 1.3f);
            draw->AddCircle(ImVec2(c.x, c.y), 2.4f, color, 14, 1.3f);
            break;
        case bar_icon_globe:
            draw->AddCircle(c, 6.0f, color, 20, 1.3f);
            draw->AddBezierQuadratic(ImVec2(c.x, c.y - 6.0f), ImVec2(c.x + 4.2f, c.y),
                ImVec2(c.x, c.y + 6.0f), color, 1.1f);
            draw->AddBezierQuadratic(ImVec2(c.x, c.y - 6.0f), ImVec2(c.x - 4.2f, c.y),
                ImVec2(c.x, c.y + 6.0f), color, 1.1f);
            draw->AddLine(ImVec2(c.x - 6.0f, c.y), ImVec2(c.x + 6.0f, c.y), color, 1.1f);
            break;
        case bar_icon_person:
            draw->AddCircle(ImVec2(c.x, c.y - 3.4f), 2.7f, color, 14, 1.3f);
            draw->PathClear();
            draw->PathArcTo(ImVec2(c.x, c.y + 6.2f), 5.0f, IM_PI, IM_PI * 2.0f, 16);
            draw->PathStroke(color, 0, 1.3f);
            break;
        case bar_icon_note:
            draw->AddLine(ImVec2(c.x + 1.0f, c.y - 5.5f), ImVec2(c.x + 1.0f, c.y + 3.5f), color, 1.5f);
            draw->AddLine(ImVec2(c.x + 1.0f, c.y - 5.5f), ImVec2(c.x + 5.0f, c.y - 3.0f), color, 1.5f);
            draw->AddCircleFilled(ImVec2(c.x - 1.5f, c.y + 4.0f), 2.5f, color);
            break;
        }
    }

    // Small floating bar, independent of the menu window: drag it anywhere, and its button
    // opens and closes the menu.
    void RenderTopBar() {
        static ImVec2 bar_pos(-1.0f, 10.0f);
        static bool dragging = false;
        static bool centred = false;

        ImGuiIO& io = ImGui::GetIO();

        // The overlay window is shrunk to nothing while it tracks the game or loses focus, so
        // the display size can be zero for a frame. Centring or clamping against that would
        // throw the bar to the origin and leave it there.
        const bool display_usable = io.DisplaySize.x > 160.0f && io.DisplaySize.y > 60.0f;

        if (bar_pos.x < 0.0f && display_usable)
            bar_pos = ImVec2((io.DisplaySize.x - 132.0f) * 0.5f, 10.0f);

        ImGui::SetNextWindowPos(bar_pos, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.075f, 0.96f));

        ImGui::Begin("##chrome_topbar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);

        ImDrawList* draw = ImGui::GetWindowDrawList();

        const ImVec2 win_min = ImGui::GetWindowPos();
        const ImVec2 win_size = ImGui::GetWindowSize();
        const ImVec2 win_max(win_min.x + win_size.x, win_min.y + win_size.y);

        draw->AddRect(win_min, win_max, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)), 10.0f, 0, 1.0f);

        // The bar auto-sizes, so its real width is only known once it has laid out: centre it on
        // the measured width the first time instead of on the old fixed estimate, which the now
        // playing line made too narrow.
        if (!centred && display_usable && win_size.x > 1.0f) {
            bar_pos = ImVec2((io.DisplaySize.x - win_size.x) * 0.5f, 10.0f);
            centred = true;
        }

        POINT bar_origin{ 0, 0 };

        if (window_config.hwnd && ClientToScreen(window_config.hwnd, &bar_origin)) {
            topbar_rect.left = bar_origin.x + static_cast<LONG>(win_min.x);
            topbar_rect.top = bar_origin.y + static_cast<LONG>(win_min.y);
            topbar_rect.right = bar_origin.x + static_cast<LONG>(win_max.x);
            topbar_rect.bottom = bar_origin.y + static_cast<LONG>(win_max.y);
            topbar_rect_valid = true;
        }

        // The name sits on the left end of the same plate as the toggles; the menu window itself
        // stays unbranded.
        {

            const ImVec2 brand_pos = ImGui::GetCursorScreenPos();
            const ImVec2 brand_size = ImGui::CalcTextSize("chrome.wtf");
            const ImVec2 brand_head = ImGui::CalcTextSize("chrome");

            ImGui::Dummy(ImVec2(brand_size.x, 20.0f));

            const float brand_y = brand_pos.y + (20.0f - ImGui::GetFontSize()) * 0.5f;

            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(brand_pos.x, brand_y),
                ImGui::GetColorU32(ImVec4(0.86f, 0.86f, 0.91f, 1.0f)), "chrome");
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(brand_pos.x + brand_head.x, brand_y),
                ImGui::GetColorU32(ImVec4(0.57f, 0.50f, 1.0f, 1.0f)), ".wtf");
        }

        ImGui::SameLine();

        // Invisible grab strip, kept on the left of the toggles; the box beside it toggles the
        // ESP preview.
        const ImVec2 grip_pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("##bar_grip", ImVec2(14.0f, 20.0f));
        const bool grip_active = ImGui::IsItemActive();

        draw->AddRectFilled(ImVec2(grip_pos.x + 6.0f, grip_pos.y + 5.0f),
            ImVec2(grip_pos.x + 8.0f, grip_pos.y + 15.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f)), 1.0f);

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

        constexpr float pill_height = 20.0f;

        const ImVec4 pill_active(0.42f, 0.36f, 0.91f, 0.85f);
        const ImVec4 pill_idle(0.12f, 0.12f, 0.16f, 1.0f);

        // Icon-only toggles: the bar carries the name, the boxes carry the icons, and the label
        // for each one is the tooltip.
        auto pill = [&](const char* id, bool active, const char* label, bar_icon icon) {
            constexpr float width = 20.0f;

            ImGui::PushStyleColor(ImGuiCol_Button, active ? pill_active : pill_idle);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                active ? ImVec4(0.48f, 0.42f, 0.98f, 0.95f) : ImVec4(0.18f, 0.18f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                active ? ImVec4(0.38f, 0.32f, 0.85f, 1.0f) : ImVec4(0.22f, 0.22f, 0.30f, 1.0f));

            const bool pressed = ImGui::Button(id, ImVec2(width, pill_height));

            const ImVec2 min = ImGui::GetItemRectMin();
            const ImU32 fg = ImGui::GetColorU32(active ? ImVec4(0.96f, 0.96f, 1.0f, 1.0f)
                                                       : ImVec4(0.72f, 0.72f, 0.79f, 1.0f));

            draw_bar_icon(draw, icon, ImVec2(min.x + width * 0.5f, min.y + pill_height * 0.5f), fg);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", label);

            ImGui::PopStyleColor(3);

            return pressed;
        };

        if (pill("##menu_toggle", visible, "Menu", bar_icon_menu))
            visible = !visible;

        ImGui::SameLine();

        if (pill("##esp_preview_toggle", globals::esp_preview, "ESP", bar_icon_eye))
            globals::esp_preview = !globals::esp_preview;

        ImGui::SameLine();

        if (pill("##dex_toggle", globals::explorer, "DEX", bar_icon_globe))
            globals::explorer = !globals::explorer;

        ImGui::SameLine();

        if (pill("##player_list_toggle", globals::player_list, "Players", bar_icon_person))
            globals::player_list = !globals::player_list;

        ImGui::SameLine();

        if (pill("##music_toggle", globals::music_player, "Music", bar_icon_note))
            globals::music_player = !globals::music_player;

        ImGui::PopStyleVar();

        if (grip_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            bar_pos = ImVec2(bar_pos.x + io.MouseDelta.x, bar_pos.y + io.MouseDelta.y);

        // Any click on the bar that is not on the button drags it.
        const bool over_bar = ImGui::IsMouseHoveringRect(win_min, win_max);

        if (!dragging && over_bar && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
            dragging = true;

        if (dragging) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                dragging = false;
            else if (display_usable)
                bar_pos = ImVec2(bar_pos.x + io.MouseDelta.x, bar_pos.y + io.MouseDelta.y);
        }

        // Only keep it on-screen when the viewport can actually contain it.
        if (display_usable && io.DisplaySize.x > win_size.x && io.DisplaySize.y > win_size.y) {
            bar_pos.x = ImClamp(bar_pos.x, 0.0f, io.DisplaySize.x - win_size.x);
            bar_pos.y = ImClamp(bar_pos.y, 0.0f, io.DisplaySize.y - win_size.y);
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void RenderFrame() {
        std::string fade_id = "overlay_fade";
        float fade_alpha = OverlayManager::UpdateAnimation(fade_id, visible ? 1.0f : 0.0f, 8.5f, 2);

        // Snow fades with the menu and sits behind its windows, so it never covers the UI.
        if (globals::snowfall && fade_alpha > 0.01f)
            snowfall::draw();

        if (fade_alpha > 0.01f) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_alpha);
            RenderMainOverlay();
            ImGui::PopStyleVar();
        }

        // The explorer stays on screen while the menu is closed, but the menu keybind hides it
        // along with the bar. It drops input in that state, so it cannot swallow clicks meant
        // for the game.
        if (topbar_visible) {
            overlay::dex();
            overlay::playerlist();
            RenderMusicPanel();
        }

        RenderESPOverlay();

        // The watermark stays on screen even when the menu is closed; only its drag goes away.
        if (globals::watermark) {
            RenderWatermark();
        }
        else {
            watermark_rect_valid = false;
        }

        // The keybind hides the bar along with the menu; the bar's menu box only closes the
        // menu, so the bar stays put and can reopen it.
        if (topbar_visible)
            RenderTopBar();
        else
            topbar_rect_valid = false;
    }

    void RenderTabs(int& selected_tab) {
        static std::vector<float> tab_hover_anim(tab_count, 0.0f);
        static float active_glide = 0.0f;

        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();

        const float bar_height = 30.0f;
        const float bar_pad = 3.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 start = ImGui::GetCursorScreenPos();

        // Rounded track the segments sit in.
        const ImVec2 track_min = start;
        const ImVec2 track_max = ImVec2(start.x + avail, start.y + bar_height);
        draw_list->AddRectFilled(track_min, track_max,
            ImGui::GetColorU32(ImVec4(0.051f, 0.051f, 0.067f, 1.0f)), 8.0f);
        draw_list->AddRect(track_min, track_max,
            ImGui::GetColorU32(ImGuiCol_Separator), 8.0f, 0, 1.0f);

        const float tab_width = (avail - bar_pad * 2.0f) / static_cast<float>(tab_count);

        // The selection highlight glides between segments instead of jumping.
        active_glide = ImLerp(active_glide, static_cast<float>(selected_tab), io.DeltaTime * 12.0f);
        const float pill_x = start.x + bar_pad + active_glide * tab_width;

        const ImVec2 pill_min = ImVec2(pill_x, start.y + bar_pad);
        const ImVec2 pill_max = ImVec2(pill_x + tab_width, start.y + bar_height - bar_pad);
        draw_list->AddRectFilled(pill_min, pill_max,
            ImGui::GetColorU32(ImVec4(0.42f, 0.36f, 0.91f, 0.18f)), 6.0f);
        draw_list->AddRect(pill_min, pill_max,
            ImGui::GetColorU32(ImVec4(0.42f, 0.36f, 0.91f, 0.45f)), 6.0f, 0, 1.0f);

        for (int i = 0; i < tab_count; i++) {
            const ImVec2 tab_min = ImVec2(start.x + bar_pad + i * tab_width, start.y + bar_pad);
            const ImVec2 tab_max = ImVec2(tab_min.x + tab_width, start.y + bar_height - bar_pad);

            const bool is_hovered = ImGui::IsMouseHoveringRect(tab_min, tab_max);
            const bool is_current = (selected_tab == i);

            if (is_hovered && ImGui::IsMouseClicked(0))
                selected_tab = i;

            tab_hover_anim[i] += io.DeltaTime * (is_hovered ? 8.0f : -8.0f);
            tab_hover_anim[i] = ImClamp(tab_hover_anim[i], 0.0f, 1.0f);

            if (is_hovered && !is_current) {
                draw_list->AddRectFilled(tab_min, tab_max,
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.04f * tab_hover_anim[i])), 6.0f);
            }

            const ImU32 text_color = is_current
                ? ImGui::GetColorU32(ImVec4(0.92f, 0.91f, 1.00f, 1.00f))
                : ImGui::GetColorU32(ImVec4(0.38f, 0.38f, 0.44f, 1.00f));

            const ImVec2 text_size = ImGui::CalcTextSize(tab_names[i]);
            const ImVec2 text_pos = ImVec2(
                tab_min.x + (tab_width - text_size.x) * 0.5f,
                tab_min.y + ((tab_max.y - tab_min.y) - text_size.y) * 0.5f);

            draw_list->AddText(text_pos, text_color, tab_names[i]);
        }

        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + bar_height));
        (void)style;
    }


    std::vector<std::pair<keybind*, std::string>> active_keybinds_with_names;

    void AddKeybind(keybind* kb, const std::string& custom_name = "") {
        std::string display_name = custom_name.empty() ? kb->get_name() : custom_name;
        active_keybinds_with_names.push_back(std::make_pair(kb, display_name));
    }

    void RenderKeybinds() {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));

        for (auto& pair : active_keybinds_with_names) {
            keybind* kb = pair.first;
            std::string display_name = pair.second;

            if (kb->enabled) {
                ImGui::SetCursorPosX(8);
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", display_name.c_str());

                ImGui::SameLine();
                std::string key_name = kb->get_key_name();
                std::string type_text = "[" + kb->get_type() + "]";

                float key_width = ImGui::CalcTextSize(key_name.c_str()).x;
                float type_width = ImGui::CalcTextSize(type_text.c_str()).x;
                float total_right_width = key_width + type_width + 4;

                ImGui::SetCursorPosX(ImGui::GetWindowSize().x - total_right_width - 8);
                ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "%s", key_name.c_str());
                ImGui::SameLine(0, 4);
                ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "%s", type_text.c_str());
            }
        }

        ImGui::PopStyleVar();
    }



    int GetActiveKeybindCount() {
        int count = 0;
        for (auto& pair : active_keybinds_with_names) {
            if (pair.first->enabled) {
                count++;
            }
        }
        return count;
    }

    static bool firstime = false;
    void RenderMainOverlay() {
        std::string fade_id = "overlay_fade";
        float fade_alpha = OverlayManager::UpdateAnimation(fade_id, visible ? 1.0f : 0.0f, 0.5f, 2);
        if (fade_alpha < 0.01f) return;

        static int selected_tab = 0;
        ImVec2 menu_pos(0.0f, 0.0f);
        ImVec2 menu_size(0.0f, 0.0f);

        // Taller menu: the tabs now carry enough rows that 650 cut things off.
        ImGui::SetNextWindowSize(ImVec2(525, 720), ImGuiCond_Once);
        ImGui::SetNextWindowSizeConstraints(ImVec2(525, 720), ImVec2(FLT_MAX, FLT_MAX));

        if (!firstime) {
            AddKeybind(&features::aimbot::aimbotbind, "Aimbot");
            AddKeybind(&features::silentaim::silentaimkeybind, "Silent Aim");
            AddKeybind(&features::triggerbot::triggerbotbind, "Triggerbot");
            AddKeybind(&features::playermodifications::speedkeybind, "Speed");
            AddKeybind(&features::playermodifications::jumpkeybind, "Jump Power");
            AddKeybind(&features::playermodifications::flightkeybind, "Flight");
            AddKeybind(&features::playermodifications::freecamkeybind, "Freecam");
            firstime = true;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade_alpha);
        if (ImGui::Begin("chrome.wtf", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse)) {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowShadowSize = 0;
            menu_pos = ImGui::GetWindowPos();
            menu_size = ImGui::GetWindowSize();
            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::BeginGroup();

            // No wordmark in the menu: the bar carries the branding, so this collapses to a
            // hairline rule with the content starting right under it.
            ImDrawList* menu_draw = ImGui::GetWindowDrawList();
            const ImVec2 menu_at = ImGui::GetWindowPos();
            const float menu_w = ImGui::GetWindowSize().x;

            menu_draw->AddLine(ImVec2(menu_at.x + 10.0f, menu_at.y + 12.0f),
                ImVec2(menu_at.x + menu_w - 10.0f, menu_at.y + 12.0f),
                ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

            ImGui::SetCursorPos(ImVec2(8, 20));
            ImGui::BeginChild("MainContainer", ImVec2(0, 0), true);
            RenderTabs(selected_tab);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            auto realbg = style.Colors[ImGuiCol_ChildBg];
            ImGui::BeginChild("Children-Container", ImVec2(0, 0), true);
            auto childbg2 = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
            style.Colors[ImGuiCol_ChildBg] = childbg2;
            static int last_tab = selected_tab;
            static float content_fade = 1.0f;
            if (last_tab != selected_tab) {
                content_fade = 0.0f;
                last_tab = selected_tab;
            }
            content_fade += ImGui::GetIO().DeltaTime * 8.0f;
            content_fade = ImClamp(content_fade, 0.0f, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_fade * style.Alpha);
            switch (selected_tab) {
            case 0: RenderAimbotTab(); break;
            case 1: RenderVisualsTab(); break;
            case 2: RenderWorldTab(); break;
            case 3: RenderPlayerTab(); break;
            case 4: RenderSettingsTab(); break;
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
            style.Colors[ImGuiCol_ChildBg] = realbg;
            ImGui::EndChild();
            ImGui::EndGroup();
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // Detached ESP preview, parked to the right of the menu while the Visuals tab is open.
        if (globals::esp_preview && selected_tab == 1) {
            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 preview_size(230.0f, 340.0f);
            const float gap = 10.0f;

            float preview_x = menu_pos.x + menu_size.x + gap;
            if (preview_x + preview_size.x > io.DisplaySize.x - 4.0f) {
                preview_x = ImMax(menu_pos.x - preview_size.x - gap, 4.0f);
            }
            const float preview_y = ImClamp(menu_pos.y, 4.0f,
                ImMax(io.DisplaySize.y - preview_size.y - 4.0f, 4.0f));

            ImGui::SetNextWindowPos(ImVec2(preview_x, preview_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(preview_size, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Begin("esp_preview_window", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings)) {
                RenderESPPreview();
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
        }
    }

    void draw_dynamic_mouse(ImDrawList* drawlist, ImVec2 mousepos) {
        if (!features::aimbot::dynamic_mouse) return;

        static ImVec2 smooth_mouse_pos = mousepos;
        static bool initialized = false;

        if (!initialized) {
            smooth_mouse_pos = mousepos;
            initialized = true;
        }

        ImVec2 target_pos = mousepos;

        if (features::aimbot::mouse_smooth) {
            float delta_time = ImGui::GetIO().DeltaTime;
            float lerp_factor = 1.0f - exp(-features::aimbot::mouse_smooth_speed * delta_time);

            smooth_mouse_pos.x += (target_pos.x - smooth_mouse_pos.x) * lerp_factor;
            smooth_mouse_pos.y += (target_pos.y - smooth_mouse_pos.y) * lerp_factor;

            target_pos = smooth_mouse_pos;
        }

        static float animation_time = 0.0f;
        if (features::aimbot::mouse_animate) {
            animation_time += ImGui::GetIO().DeltaTime * features::aimbot::mouse_animation_speed;
        }

        float animated_size = features::aimbot::mouse_size;
        if (features::aimbot::mouse_animate) {
            animated_size += sin(animation_time) * (features::aimbot::mouse_size * 0.3f);
        }

        ImU32 color = ImU32(ImColor(features::aimbot::mouse_color[0], features::aimbot::mouse_color[1], features::aimbot::mouse_color[2], features::aimbot::mouse_color[3]));
        ImU32 outline_color = ImU32(features::aimbot::mouse_outline_color);

        switch (features::aimbot::mouse_style) {
        case 0: {
            float gap = features::aimbot::mouse_gap;
            if (features::aimbot::mouse_outline) {
                drawlist->AddLine(
                    ImVec2(target_pos.x - animated_size - features::aimbot::mouse_outline_thickness, target_pos.y),
                    ImVec2(target_pos.x - gap - features::aimbot::mouse_outline_thickness, target_pos.y),
                    outline_color, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2
                );
                drawlist->AddLine(
                    ImVec2(target_pos.x + gap + features::aimbot::mouse_outline_thickness, target_pos.y),
                    ImVec2(target_pos.x + animated_size + features::aimbot::mouse_outline_thickness, target_pos.y),
                    outline_color, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2
                );
                drawlist->AddLine(
                    ImVec2(target_pos.x, target_pos.y - animated_size - features::aimbot::mouse_outline_thickness),
                    ImVec2(target_pos.x, target_pos.y - gap - features::aimbot::mouse_outline_thickness),
                    outline_color, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2
                );
                drawlist->AddLine(
                    ImVec2(target_pos.x, target_pos.y + gap + features::aimbot::mouse_outline_thickness),
                    ImVec2(target_pos.x, target_pos.y + animated_size + features::aimbot::mouse_outline_thickness),
                    outline_color, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2
                );
            }
            drawlist->AddLine(
                ImVec2(target_pos.x - animated_size, target_pos.y),
                ImVec2(target_pos.x - gap, target_pos.y),
                color, features::aimbot::mouse_thickness
            );
            drawlist->AddLine(
                ImVec2(target_pos.x + gap, target_pos.y),
                ImVec2(target_pos.x + animated_size, target_pos.y),
                color, features::aimbot::mouse_thickness
            );
            drawlist->AddLine(
                ImVec2(target_pos.x, target_pos.y - animated_size),
                ImVec2(target_pos.x, target_pos.y - gap),
                color, features::aimbot::mouse_thickness
            );
            drawlist->AddLine(
                ImVec2(target_pos.x, target_pos.y + gap),
                ImVec2(target_pos.x, target_pos.y + animated_size),
                color, features::aimbot::mouse_thickness
            );
            break;
        }
        case 1: { 
            if (features::aimbot::mouse_outline) {
                drawlist->AddCircle(target_pos, animated_size + features::aimbot::mouse_outline_thickness, outline_color, 32, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2);
            }
            drawlist->AddCircle(target_pos, animated_size, color, 32, features::aimbot::mouse_thickness);
            break;
        }
        case 2: {
            if (features::aimbot::mouse_outline) {
                drawlist->AddCircleFilled(target_pos, animated_size + features::aimbot::mouse_outline_thickness, outline_color);
            }
            drawlist->AddCircleFilled(target_pos, animated_size, color);
            break;
        }
        case 3: { 
            ImVec2 square_min = ImVec2(target_pos.x - animated_size, target_pos.y - animated_size);
            ImVec2 square_max = ImVec2(target_pos.x + animated_size, target_pos.y + animated_size);

            if (features::aimbot::mouse_outline) {
                ImVec2 outline_min = ImVec2(square_min.x - features::aimbot::mouse_outline_thickness, square_min.y - features::aimbot::mouse_outline_thickness);
                ImVec2 outline_max = ImVec2(square_max.x + features::aimbot::mouse_outline_thickness, square_max.y + features::aimbot::mouse_outline_thickness);
                drawlist->AddRect(outline_min, outline_max, outline_color, 0.0f, 0, features::aimbot::mouse_thickness + features::aimbot::mouse_outline_thickness * 2);
            }
            drawlist->AddRect(square_min, square_max, color, 0.0f, 0, features::aimbot::mouse_thickness);
            break;
        }
        }
    }



    void render_keybindlist() {
        // Cleared first so an empty list cannot leave a stale drag area over the game.
        keybind_rect_valid = false;

        const int count = GetActiveKeybindCount();
        if (count <= 0) return;

        // Native font size everywhere: the atlas only bakes ProggyClean at 13 px, so drawing any
        // smaller makes ImGui rescale the glyphs and the text turns to mush.
        const float font_size = ImGui::GetFontSize();
        const float pad = 10.0f;
        const float row_height = 20.0f;
        const float header_height = 26.0f;
        const float key_height = 16.0f;

        float card_width = 168.0f;

        for (auto& pair : active_keybinds_with_names) {
            if (!pair.first->enabled) continue;

            const float name_w = ImGui::CalcTextSize(pair.second.c_str()).x;
            const float mode_w = ImGui::CalcTextSize(pair.first->get_type().c_str()).x;
            const float key_w = ImGui::CalcTextSize(pair.first->get_key_name().c_str()).x + 12.0f;

            const float needed = pad + name_w + 12.0f + mode_w + 8.0f + key_w + pad;

            if (needed > card_width) card_width = needed;
        }

        const float card_height = header_height + row_height * static_cast<float>(count) + 6.0f;

        // Starts under the watermark, then stays wherever it is dragged: like the watermark it
        // has no title bar, so the card itself is the handle.
        static ImVec2 card_pos(12.0f, 56.0f);
        static bool card_dragging = false;

        // The card grows with the number of entries, and the window has no auto-resize: without
        // this it keeps its first frame's size and clips every row past the first.
        ImGui::SetNextWindowPos(card_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(card_width, card_height), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

        ImGui::Begin("keybinds", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);

        const ImGuiIO& card_io = ImGui::GetIO();

        if (ImGui::IsWindowHovered() && !card_dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            card_dragging = true;

        if (card_dragging) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                card_dragging = false;
            }
            else {
                card_pos.x = ImClamp(card_pos.x + card_io.MouseDelta.x, 0.0f,
                    ImMax(card_io.DisplaySize.x - card_width, 0.0f));
                card_pos.y = ImClamp(card_pos.y + card_io.MouseDelta.y, 0.0f,
                    ImMax(card_io.DisplaySize.y - card_height, 0.0f));
            }
        }

        ImGui::Dummy(ImVec2(card_width, card_height));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();

        const ImVec2 at = ImGui::GetWindowPos();
        const ImVec2 card_max(at.x + card_width, at.y + card_height);

        POINT card_origin{ 0, 0 };

        if (window_config.hwnd && ClientToScreen(window_config.hwnd, &card_origin)) {
            keybind_rect.left = card_origin.x + static_cast<LONG>(at.x);
            keybind_rect.top = card_origin.y + static_cast<LONG>(at.y);
            keybind_rect.right = card_origin.x + static_cast<LONG>(card_max.x);
            keybind_rect.bottom = card_origin.y + static_cast<LONG>(card_max.y);
            keybind_rect_valid = true;
        }

        // Soft drop shadow, then the plate.
        dl->AddRectFilled(ImVec2(at.x + 2.0f, at.y + 3.0f),
            ImVec2(card_max.x + 2.0f, card_max.y + 3.0f), IM_COL32(0, 0, 0, 90), 8.0f);
        dl->AddRectFilled(at, card_max, IM_COL32(8, 8, 10, 255), 8.0f);

        dl->AddText(font, font_size, ImVec2(at.x + pad, at.y + 6.0f), IM_COL32(150, 150, 172, 255), "KEYBINDS");
        dl->AddLine(ImVec2(at.x + pad, at.y + header_height - 7.0f),
            ImVec2(card_max.x - pad, at.y + header_height - 7.0f), IM_COL32(64, 64, 88, 190), 1.0f);

        int row = 0;

        for (auto& pair : active_keybinds_with_names) {
            keybind* kb = pair.first;

            if (!kb->enabled) continue;

            const float row_y = at.y + header_height + row_height * static_cast<float>(row);
            const float text_y = row_y + (row_height - font_size) * 0.5f;

            dl->AddText(font, font_size, ImVec2(at.x + pad, text_y), IM_COL32(219, 219, 231, 255),
                pair.second.c_str());

            const std::string key_name = kb->get_key_name();
            const float key_w = ImGui::CalcTextSize(key_name.c_str()).x + 12.0f;
            const float key_x = card_max.x - pad - key_w;

            // Keycap chip on the right edge.
            const ImVec2 key_min(key_x, row_y + (row_height - key_height) * 0.5f);
            const ImVec2 key_max(key_x + key_w, key_min.y + key_height);

            dl->AddRectFilled(key_min, key_max, IM_COL32(34, 31, 50, 255), 4.0f);
            dl->AddRect(key_min, key_max, IM_COL32(108, 100, 180, 210), 4.0f, 0, 1.0f);
            dl->AddText(font, font_size, ImVec2(key_min.x + 6.0f, key_min.y + (key_height - font_size) * 0.5f),
                IM_COL32(232, 230, 245, 255), key_name.c_str());

            // Mode hint, right-aligned before the keycap.
            const std::string mode = kb->get_type();
            const float mode_w = ImGui::CalcTextSize(mode.c_str()).x;

            dl->AddText(font, font_size, ImVec2(key_x - mode_w - 8.0f, text_y),
                IM_COL32(130, 130, 152, 255), mode.c_str());

            ++row;
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void render_watermark() {
        static float fps_value = 0.0f;
        static auto last_fps_update = std::chrono::steady_clock::now();
        static int frame_count = 0;

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_update);
        if (duration.count() >= 1000) {
            fps_value = frame_count / (duration.count() / 1000.0f);
            frame_count = 0;
            last_fps_update = now;
        }

        auto time_now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(time_now);
        struct tm tm_info;
        localtime_s(&tm_info, &time_t);

        char clock_buffer[16];
        std::strftime(clock_buffer, sizeof(clock_buffer), "%H:%M", &tm_info);

        char fps_buffer[16];
        sprintf_s(fps_buffer, "%.0f fps", fps_value);

        const float font_size = ImGui::GetFontSize();
        const float pad = 14.0f;
        const float gap = 10.0f;
        const float height = 36.0f;

        const ImVec2 brand_size = ImGui::CalcTextSize("chrome");
        const ImVec2 suffix_size = ImGui::CalcTextSize(".wtf");
        const ImVec2 clock_size = ImGui::CalcTextSize(clock_buffer);
        const ImVec2 fps_size = ImGui::CalcTextSize(fps_buffer);

        const float width = pad + brand_size.x + suffix_size.x + gap + 4.0f + gap +
            clock_size.x + gap + fps_size.x + pad;

        // Draggable: the plate has no title bar, so pressing it anywhere moves it.
        static ImVec2 watermark_pos(12.0f, 12.0f);
        static bool watermark_dragging = false;

        ImGui::SetNextWindowPos(watermark_pos, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

        ImGui::Begin("watermark", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoNav);

        const ImGuiIO& hud_io = ImGui::GetIO();

        // Not draggable while the menu is hidden: the overlay is click-through in that state, so
        // the plate is only read, never grabbed.
        if (!visible)
            watermark_dragging = false;

        if (visible && ImGui::IsWindowHovered() && !watermark_dragging &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            watermark_dragging = true;

        if (watermark_dragging) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                watermark_dragging = false;
            }
            else {
                watermark_pos.x = ImClamp(watermark_pos.x + hud_io.MouseDelta.x, 0.0f,
                    ImMax(hud_io.DisplaySize.x - width, 0.0f));
                watermark_pos.y = ImClamp(watermark_pos.y + hud_io.MouseDelta.y, 0.0f,
                    ImMax(hud_io.DisplaySize.y - height, 0.0f));
            }
        }

        ImGui::Dummy(ImVec2(width, height));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();

        const ImVec2 at = ImGui::GetWindowPos();

        POINT hud_origin{ 0, 0 };

        watermark_rect_valid = false;

        if (visible && window_config.hwnd && ClientToScreen(window_config.hwnd, &hud_origin)) {
            watermark_rect.left = hud_origin.x + static_cast<LONG>(at.x);
            watermark_rect.top = hud_origin.y + static_cast<LONG>(at.y);
            watermark_rect.right = hud_origin.x + static_cast<LONG>(at.x + width);
            watermark_rect.bottom = hud_origin.y + static_cast<LONG>(at.y + height);
            watermark_rect_valid = true;
        }

        dl->AddRectFilled(ImVec2(at.x + 2.0f, at.y + 3.0f), ImVec2(at.x + width + 2.0f, at.y + height + 3.0f),
            IM_COL32(0, 0, 0, 90), 9.0f);
        dl->AddRectFilled(at, ImVec2(at.x + width, at.y + height), IM_COL32(8, 8, 10, 255), 9.0f);

        float x = at.x + pad;
        const float text_y = at.y + (height - font_size) * 0.5f;

        dl->AddText(font, font_size, ImVec2(x, text_y), IM_COL32(219, 219, 231, 255), "chrome");
        x += brand_size.x;
        dl->AddText(font, font_size, ImVec2(x, text_y), IM_COL32(146, 128, 255, 255), ".wtf");
        x += suffix_size.x + gap;

        dl->AddCircleFilled(ImVec2(x + 2.0f, at.y + height * 0.5f), 2.0f, IM_COL32(126, 126, 148, 220));
        x += 4.0f + gap;

        dl->AddText(font, font_size, ImVec2(x, text_y), IM_COL32(158, 158, 176, 255), clock_buffer);
        x += clock_size.x + gap;

        dl->AddText(font, font_size, ImVec2(x, text_y), IM_COL32(128, 128, 148, 255), fps_buffer);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void RenderESPOverlay() {

        static HWND robloxWindow = FindWindowA(0, "Roblox");
        robloxWindow = FindWindowA(0, "Roblox");

        auto drawbglist = ImGui::GetBackgroundDrawList();
        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        ScreenToClient(robloxWindow, &cursor_pos);

        ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);

        if (features::aimbot::drawfov) {
            const ImU32 fov_color = ImU32(ImColor(features::aimbot::fovcolor[0], features::aimbot::fovcolor[1], features::aimbot::fovcolor[2], features::aimbot::fovcolor[3]));
            drawbglist->AddCircle(mousepos, features::aimbot::fieldofviewsize - 1, IM_COL32(0, 0, 0, 255));
            drawbglist->AddCircle(mousepos, features::aimbot::fieldofviewsize, fov_color);
            drawbglist->AddCircle(mousepos, features::aimbot::fieldofviewsize + 1, IM_COL32(0, 0, 0, 255));
        }
        if (features::silentaim::drawfov) {
            const ImU32 fov_color = ImU32(ImColor(features::silentaim::fovcolor[0], features::silentaim::fovcolor[1], features::silentaim::fovcolor[2], features::silentaim::fovcolor[3]));
            drawbglist->AddCircle(mousepos, features::silentaim::fieldofview - 1, IM_COL32(0, 0, 0, 255));
            drawbglist->AddCircle(mousepos, features::silentaim::fieldofview, fov_color);
            drawbglist->AddCircle(mousepos, features::silentaim::fieldofview + 1, IM_COL32(0, 0, 0, 255));
        }
        draw_dynamic_mouse(drawbglist, mousepos);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);


        ImGui::Begin("ESP", nullptr,
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoMouseInputs |
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove);

        globals::draw = ImGui::GetBackgroundDrawList();
        bullet_tracer::render_tracers();
        visuals::run_esp();

        ImGui::End();

        if (globals::keybindlist) {
            render_keybindlist();
        }
        else {
            keybind_rect_valid = false;
        }

    }

    void RenderPerformanceMetrics() {
        ImVec2 metrics_pos(10, 50);
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

        auto metrics = OverlayManager::GetPerformanceMetrics();

        std::vector<std::string> metric_texts = {
            "FPS: " + std::to_string(static_cast<int>(metrics.fps)),
            "Frame Time: " + std::to_string(metrics.frame_time * 1000.0f) + "ms",
            "GPU Time: " + std::to_string(metrics.gpu_time * 1000.0f) + "ms",
            "CPU Usage: " + std::to_string(static_cast<int>(metrics.cpu_usage)) + "%",
            "Memory: " + Utils::FormatBytes(metrics.memory_usage)
        };

        ImU32 text_color = IM_COL32(255, 255, 255, 200);
        ImU32 shadow_color = IM_COL32(0, 0, 0, 150);

        for (const auto& text : metric_texts) {
            draw_list->AddText(ImVec2(metrics_pos.x + 1, metrics_pos.y + 1), shadow_color, text.c_str());
            draw_list->AddText(metrics_pos, text_color, text.c_str());
            metrics_pos.y += 20;
        }
    }

    void RenderWatermark() {
        render_watermark();
    }

    // Marker for an option whose name does not explain itself: a white "?" at the far end of the
    // row, with the explanation on hover. The row is measured from the control just drawn, because
    // the slider and the combo fill the width and leave no room directly after them.
    static void help_marker(const char* explanation) {
        const ImVec2 row_min = ImGui::GetItemRectMin();
        const ImVec2 cursor_after = ImGui::GetCursorScreenPos();
        const float row_width = ImGui::GetContentRegionAvail().x;

        ImGui::SetCursorScreenPos(ImVec2(
            row_min.x + row_width - ImGui::CalcTextSize("?").x - 2.0f, row_min.y));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "?");

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
            ImGui::TextUnformatted(explanation);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::SetCursorScreenPos(cursor_after);
    }

    void RenderAimbotTab() {
        float child_width = (ImGui::GetContentRegionAvail().x / 2) - 4;

        ImGui::BeginGroup();
        {
            ImAdd::BeginChild("Aimbot", ImVec2(child_width, 280));
            {
                SDK::Gui::Checkbox("Enabled", &features::aimbot::enabled);

                std::string current_name = features::aimbot::aimbotbind.get_key_name();
                if (features::aimbot::aimbotbind.waiting_for_input) {
                    current_name = "[...]";
                }
                float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                float bind_width = ImMax(60.0f, text_width + 16.0f);

                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                OverlayManager::OptimizedKeybind(&features::aimbot::aimbotbind, ImVec2(bind_width, 18.0f));

                SDK::Gui::Checkbox("Sticky Aim", &features::aimbot::stickyaim);
            help_marker("Holds the current target while it stays valid instead of re-picking the closest one to your cursor every frame, so aim does not snap between two players near each other.");
              //  ImGui::Spacing();
                const char* aimtypes[] = { "Memory", "Mouse" };
                SDK::Gui::ComboBox("Aiming Mode", &features::aimbot::aimtype, aimtypes, 2);

                SDK::Gui::Checkbox("Draw FOV", &features::aimbot::drawfov);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##AimbotFovColor", features::aimbot::fovcolor);
                SDK::Gui::SliderFloat("Field Of View", &features::aimbot::fieldofviewsize, 0, 1000, "%.0f");
                SDK::Gui::Checkbox("Team Check", &features::aimbot::teamcheck);

                ImGui::Spacing();


                ImGui::Spacing();


                ImGui::Spacing();
                const char* aimbot_parts[] = { "Head", "HRP", "Torso", "Closest" };
                SDK::Gui::ComboBox("Hit Part", &features::aimbot::hitpart, aimbot_parts, 4);

            }
            ImAdd::EndChild();
            ImAdd::BeginChild("Smoothing", ImVec2(child_width, 100));
            {
                SDK::Gui::Checkbox("Smoothing", &features::aimbot::smoothing);
                SDK::Gui::SliderFloat("Smoothing Value", &features::aimbot::smoothingamount, 0, 100, "%.1f");
            }
            ImAdd::EndChild();
            ImAdd::BeginChild("Predictions", ImVec2(child_width, 0));
            {
                SDK::Gui::Checkbox("Prediction", &features::aimbot::predictions);
            help_marker("Leads the target by its movement before converting to screen space, so a running player is aimed at where they are about to be rather than where they were.");
                SDK::Gui::SliderFloat("Prediction X", &features::aimbot::predictionsx, 0, 50, "%.1f");
                SDK::Gui::SliderFloat("Prediction Y", &features::aimbot::predictionsy, 0, 50, "%.1f");
            }
            ImAdd::EndChild();

        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
             ImAdd::BeginChild("Silent Aim", ImVec2(child_width, 380));
            {
                 SDK::Gui::Checkbox("Enabled                ", &features::silentaim::enabled);

                 std::string current_name = features::silentaim::silentaimkeybind.get_key_name();
                 if (features::silentaim::silentaimkeybind.waiting_for_input) {
                     current_name = "[...]";
                 }
                 float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                 float bind_width = ImMax(60.0f, text_width + 16.0f);

                 SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                 ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                 OverlayManager::OptimizedKeybind(&features::silentaim::silentaimkeybind, ImVec2(bind_width, 18.0f));

                 SDK::Gui::Checkbox("StickyAim", &features::silentaim::stickyaim);
                 help_marker("Holds the current target while it stays valid instead of re-picking the closest one to your cursor every frame, so hits do not snap between two players near each other.");
                 SDK::Gui::Checkbox("SpoofMouse", &features::silentaim::spoofmouse);
                 help_marker("Sends the hit through MouseService but leaves the Aim frame at your real cursor position, so the shot lands on the target without the cursor visibly jumping to it.");

                 SDK::Gui::Checkbox("Draw FOV", &features::silentaim::drawfov);
                 SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                 SDK::Gui::ColorEdit4("##SilentFovColor", features::silentaim::fovcolor);
                 SDK::Gui::SliderFloat("Field Of View", &features::silentaim::fieldofview, 0, 1000, "%.0f");
                 SDK::Gui::Checkbox("Team Check", &features::silentaim::teamcheck);


                 SDK::Gui::SliderFloat("HitChance", &features::silentaim::hitchance, 0, 100, "%.0f");
                 help_marker("Chance per shot of actually going for the target. Below 100 a share of shots are left alone, which reads less robotically than hitting every single one.");
                 ImGui::Spacing();
                 const char* aimbot_parts[] = { "Head", "HRP", "Torso", "Closest" };
                 SDK::Gui::ComboBox("Hit Part", &features::aimbot::hitpart, aimbot_parts, 4);
            }
             ImAdd::EndChild();



             ImAdd::BeginChild("Triggerbot", ImVec2(child_width, 0));
            {
                 SDK::Gui::Checkbox("Enabled                            ", &features::triggerbot::enabled);

                 std::string current_name = features::triggerbot::triggerbotbind.get_key_name();
                 if (features::triggerbot::triggerbotbind.waiting_for_input) {
                     current_name = "[...]";
                 }
                 float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                 float bind_width = ImMax(60.0f, text_width + 16.0f);

                 SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                 ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                 OverlayManager::OptimizedKeybind(&features::triggerbot::triggerbotbind, ImVec2(bind_width, 18.0f));


                 SDK::Gui::Checkbox("Wallcheck", &features::triggerbot::wallcheck);
                 SDK::Gui::SliderFloat("Reaction Time", &features::triggerbot::reactiontime, 0, 100, "%.0f");

            }
             ImAdd::EndChild();
        }
        ImGui::EndGroup();
    }

    void RenderESPPreview() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 40.0f || avail.y < 70.0f) return;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImFont* font = ImGui::GetFont();

        auto rgb = [](const float* c) {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], 1.0f));
        };
        auto rgba = [](const float* c) {
            return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
        };

        const float name_size = ImGui::GetFontSize();
        const float info_size = ImGui::GetFontSize();
        const float name_h = name_size + 2.0f;
        const float info_h = info_size + 2.0f;

        const bool show_name = visuals::settings::nameesp;
        const bool show_dist = visuals::settings::distanceesp;
        const bool show_tool = visuals::settings::toolesp;

        // The box keeps a fixed footprint no matter which edges the labels are dragged to: the
        // labels float outside it and never resize or shift it.
        const float reserved =
            (show_name ? name_h : 0.0f) +
            (show_dist ? info_h : 0.0f) +
            (show_tool ? info_h : 0.0f) + 6.0f;

        // The box bounds the model itself: height is the avatar's height plus a margin, and width
        // is the avatar's width at the *current* spin angle. Seen side-on the silhouette is narrow,
        // so the box narrows with it, exactly as a live box does around a player seen side-on.
        constexpr float box_margin = 3.0f;
        constexpr float avatar_pitch = 0.0f;
        constexpr float avatar_spin_speed = 0.55f;
        constexpr float avatar_spin_hz = 30.0f;

        const int avatar_spin_step = (int)(ImGui::GetTime() * (double)avatar_spin_hz);
        const float avatar_yaw = 0.42f + (float)avatar_spin_step / avatar_spin_hz * avatar_spin_speed;
        const float avatar_cos = cosf(avatar_yaw);
        const float avatar_sin = sinf(avatar_yaw);

        // Bounding follows the silhouette: the orthographic width of the rotated model box.
        // Static is the fixed humanoid footprint the game uses, so the preview box keeps one
        // width through the whole spin instead of narrowing when the avatar turns side-on.
        const bool static_sizing = visuals::settings::boxsizing == 1;
        const float avatar_half_width = static_sizing
            ? render::k_static_half_width / render::k_static_height
            : avatar_model::kHalfWidth * fabsf(avatar_cos) + avatar_model::kHalfDepth * fabsf(avatar_sin);

        auto box_width_for = [&](float height) {
            const float model_h = ImMax(height - box_margin * 2.0f, 1.0f);
            return 2.0f * avatar_half_width * model_h + box_margin * 2.0f;
        };

        float box_h = ImMin(avail.y - reserved - 8.0f, 170.0f);
        float box_w = box_width_for(box_h);
        const float max_w = avail.x - 42.0f;
        if (box_w > max_w) {
            const float model_h = (max_w - box_margin * 2.0f) / ImMax(2.0f * avatar_half_width, 0.001f);
            box_h = model_h + box_margin * 2.0f;
            box_w = box_width_for(box_h);
        }
        if (box_h < 10.0f || box_w < 6.0f) return;

        const float stack_h = reserved + box_h;
        float top = origin.y + ImMax((avail.y - stack_h) * 0.5f, 0.0f);
        const float box_top = top + (show_name ? name_h : 0.0f);

        const ImVec2 tl(origin.x + avail.x * 0.5f - box_w * 0.5f, box_top);
        const ImVec2 br(tl.x + box_w, box_top + box_h);
        const float center_x = (tl.x + br.x) * 0.5f;

        auto text_width = [&](const std::string& text, float size) {
            return font->CalcTextSizeA(size, 99999.0f, 0.0f, text.c_str()).x;
        };


        auto draw_text = [&](const std::string& text, const ImVec2& pos, ImU32 color, float size) {
            const ImU32 outline = IM_COL32(0, 0, 0, 255);
            dl->AddText(font, size, pos + ImVec2(1, 1), outline, text.c_str());
            dl->AddText(font, size, pos + ImVec2(-1, 1), outline, text.c_str());
            dl->AddText(font, size, pos + ImVec2(1, -1), outline, text.c_str());
            dl->AddText(font, size, pos + ImVec2(-1, -1), outline, text.c_str());
            dl->AddText(font, size, pos, color, text.c_str());
        };

        if (visuals::settings::tracers) {
            dl->AddLine(ImVec2(origin.x + avail.x * 0.5f, origin.y + avail.y - 1.0f),
                ImVec2(center_x, br.y), rgb(visuals::settings::linecolor), 2.0f);
        }

        // Baked avatar (assets/roblox-r6.glb), pre-triangulated at build time. It goes through a
        // small software rasterizer into per-pixel spans rather than thousands of sub-pixel ImGui
        // triangles: at this size those read as jagged noise, while supersampled spans stay smooth.
        // Drawn before the box so its outline and the labels sit on top. The box is sized from
        // this model, so everything it draws falls inside it.
        if (avatar_model::kTriangleCount > 0) {
            constexpr int supersample = 3;
            constexpr float light_x = -0.35f, light_y = 0.55f, light_z = 0.75f;

            // The same angle the box was sized from, so the box always bounds the silhouette.
            const float cos_yaw = avatar_cos, sin_yaw = avatar_sin;
            const int spin_step = avatar_spin_step;
            const float cos_pitch = cosf(avatar_pitch), sin_pitch = sinf(avatar_pitch);

            const int box_x0 = (int)ImFloor(tl.x);
            const int box_y0 = (int)ImFloor(tl.y);
            const int box_px_w = (int)ImClamp(ImFloor(br.x) - (float)box_x0, 0.0f, 1024.0f);
            const int box_px_h = (int)ImClamp(ImFloor(br.y) - (float)box_y0, 0.0f, 1024.0f);

            // A couple of pixels of slack for edge antialiasing; the arms now fit inside the box.
            const int pad_x = 2;
            const int pad_y = 1;

            if (box_px_w > 4 && box_px_h > 4) {
                const int px_w = box_px_w + pad_x * 2;
                const int px_h = box_px_h + pad_y * 2;

                struct avatar_span {
                    float x0, y0, x1, y1;
                    ImU32 color;
                };

                // Cached against the spin step and the box size: rebuilt ~spin_hz times a second.
                static std::vector<avatar_span> cached_spans;
                static int cached_w = -1;
                static int cached_h = -1;
                static int cached_step = -1;

                if (cached_w != px_w || cached_h != px_h || cached_step != spin_step) {
                    cached_w = px_w;
                    cached_h = px_h;
                    cached_step = spin_step;
                    cached_spans.clear();

                    const int ss_w = px_w * supersample;
                    const int ss_h = px_h * supersample;
                    const size_t ss_count = (size_t)ss_w * (size_t)ss_h;

                    static std::vector<float> depth;
                    static std::vector<int> owner;
                    static std::vector<float> shade;
                    depth.assign(ss_count, -1e30f);
                    owner.assign(ss_count, -1);
                    shade.resize((size_t)avatar_model::kTriangleCount * 3u);

                    const float model_scale = (float)box_px_h - box_margin * 2.0f;
                    const float mid_px = (float)pad_x + (float)box_px_w * 0.5f;
                    const float foot_px = (float)pad_y + (float)box_px_h - box_margin;

                    for (int i = 0; i < avatar_model::kTriangleCount; ++i) {
                        const avatar_model::tri& t = avatar_model::kTriangles[i];

                        float sx[3];
                        float sy[3];
                        float sz[3];

                        for (int v = 0; v < 3; ++v) {
                            const float px = t.p[v * 3 + 0];
                            const float py = t.p[v * 3 + 1];
                            const float pz = t.p[v * 3 + 2];

                            const float yaw_x = px * cos_yaw + pz * sin_yaw;
                            const float yaw_z = -px * sin_yaw + pz * cos_yaw;

                            const float pitch_y = py * cos_pitch - yaw_z * sin_pitch;
                            const float pitch_z = py * sin_pitch + yaw_z * cos_pitch;

                            sx[v] = ((float)mid_px + yaw_x * model_scale) * (float)supersample;
                            sy[v] = ((float)foot_px - pitch_y * model_scale) * (float)supersample;
                            sz[v] = pitch_z;
                        }

                        // Rotate the normal the same way, so the shading follows the view.
                        const float n_yaw_x = t.n[0] * cos_yaw + t.n[2] * sin_yaw;
                        const float n_yaw_z = -t.n[0] * sin_yaw + t.n[2] * cos_yaw;
                        const float n_pitch_y = t.n[1] * cos_pitch - n_yaw_z * sin_pitch;
                        const float n_pitch_z = t.n[1] * sin_pitch + n_yaw_z * cos_pitch;

                        const float facing = fabsf(n_yaw_x * light_x + n_pitch_y * light_y + n_pitch_z * light_z);
                        const float lit = 0.58f + 0.42f * facing;
                        shade[(size_t)i * 3u + 0u] = 0.60f * lit;
                        shade[(size_t)i * 3u + 1u] = 0.63f * lit;
                        shade[(size_t)i * 3u + 2u] = 0.80f * lit;

                        const float area = (sy[1] - sy[2]) * (sx[0] - sx[2]) + (sx[2] - sx[1]) * (sy[0] - sy[2]);
                        if (fabsf(area) < 1e-6f) continue;

                        const int min_x = (int)ImClamp(ImFloor(ImMin(sx[0], ImMin(sx[1], sx[2]))), 0.0f, (float)(ss_w - 1));
                        const int max_x = (int)ImClamp(ImFloor(ImMax(sx[0], ImMax(sx[1], sx[2]))) + 1.0f, 0.0f, (float)(ss_w - 1));
                        const int min_y = (int)ImClamp(ImFloor(ImMin(sy[0], ImMin(sy[1], sy[2]))), 0.0f, (float)(ss_h - 1));
                        const int max_y = (int)ImClamp(ImFloor(ImMax(sy[0], ImMax(sy[1], sy[2]))) + 1.0f, 0.0f, (float)(ss_h - 1));
                        if (min_x > max_x || min_y > max_y) continue;

                        const float inv_area = 1.0f / area;

                        for (int y = min_y; y <= max_y; ++y) {
                            const float sample_y = (float)y + 0.5f;
                            const size_t row = (size_t)y * (size_t)ss_w;

                            for (int x = min_x; x <= max_x; ++x) {
                                const float sample_x = (float)x + 0.5f;

                                const float b0 = ((sy[1] - sy[2]) * (sample_x - sx[2]) + (sx[2] - sx[1]) * (sample_y - sy[2])) * inv_area;
                                const float b1 = ((sy[2] - sy[0]) * (sample_x - sx[2]) + (sx[0] - sx[2]) * (sample_y - sy[2])) * inv_area;
                                const float b2 = 1.0f - b0 - b1;
                                if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;

                                const float sample_z = b0 * sz[0] + b1 * sz[1] + b2 * sz[2];
                                const size_t index = row + (size_t)x;
                                if (sample_z <= depth[index]) continue;

                                depth[index] = sample_z;
                                owner[index] = i;
                            }
                        }
                    }

                    const float samples = (float)(supersample * supersample);

                    for (int y = 0; y < px_h; ++y) {
                        ImU32 run_color = 0;
                        int run_start = 0;

                        for (int x = 0; x <= px_w; ++x) {
                            ImU32 color = 0;

                            if (x < px_w) {
                                float r = 0.0f, g = 0.0f, b = 0.0f;
                                int covered = 0;

                                for (int dy = 0; dy < supersample; ++dy) {
                                    const size_t row = ((size_t)(y * supersample + dy) * (size_t)ss_w)
                                        + (size_t)(x * supersample);
                                    for (int dx = 0; dx < supersample; ++dx) {
                                        const int index = owner[row + (size_t)dx];
                                        if (index < 0) continue;
                                        r += shade[(size_t)index * 3u + 0u];
                                        g += shade[(size_t)index * 3u + 1u];
                                        b += shade[(size_t)index * 3u + 2u];
                                        ++covered;
                                    }
                                }

                                if (covered > 0) {
                                    // Coarse quantisation on purpose: neighbouring pixels then share
                                    // a colour, so a row collapses into a handful of spans instead
                                    // of one quad per pixel.
                                    const float inv = 1.0f / (float)covered;
                                    const float alpha = (float)covered / samples;
                                    const int a8 = (int)(alpha * 15.0f + 0.5f) * 17;
                                    const int r8 = (int)(r * inv * 31.0f + 0.5f) * 8;
                                    const int g8 = (int)(g * inv * 31.0f + 0.5f) * 8;
                                    const int b8 = (int)(b * inv * 31.0f + 0.5f) * 8;
                                    if (a8 > 0)
                                        color = IM_COL32(r8, g8, b8, a8);
                                }
                            }

                            if (color != run_color) {
                                if (run_color != 0) {
                                    cached_spans.push_back({
                                        (float)run_start, (float)y, (float)x, (float)y + 1.0f, run_color
                                    });
                                }
                                run_color = color;
                                run_start = x;
                            }
                        }
                    }
                }

                const ImVec2 panel_min((float)(box_x0 - pad_x), (float)(box_y0 - pad_y));

                for (const avatar_span& span : cached_spans) {
                    dl->AddRectFilled(
                        ImVec2(panel_min.x + span.x0, panel_min.y + span.y0),
                        ImVec2(panel_min.x + span.x1, panel_min.y + span.y1),
                        span.color);
                }
            }
        }

        if (visuals::settings::boxesp) {
            const ImU32 box_color = rgb(visuals::settings::boxcolor);
            if (visuals::settings::boxtype == 0) {
                if (visuals::settings::boxglow && visuals::settings::box_glow_thickness > 0.0f) {
                    dl->AddShadowRect(tl, br, rgb(visuals::settings::boxglowcolor),
                        visuals::settings::box_glow_thickness, ImVec2(0, 0),
                        ImDrawFlags_ShadowCutOutShapeBackground, 0.0f);
                }
                if (visuals::settings::boxfilled) {
                    const ImU32 fill_top = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        visuals::settings::boxfilledcolor[0], visuals::settings::boxfilledcolor[1],
                        visuals::settings::boxfilledcolor[2], visuals::settings::boxfilledcolor[3]));

                    if (visuals::settings::boxfillstyle == 0) {
                        dl->AddRectFilled(tl, br, fill_top);
                    }
                    else {
                        const ImU32 fill_bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(
                            visuals::settings::boxfilledcolor2[0], visuals::settings::boxfilledcolor2[1],
                            visuals::settings::boxfilledcolor2[2], visuals::settings::boxfilledcolor2[3]));

                        dl->AddRectFilledMultiColor(tl, br, fill_top, fill_top, fill_bottom, fill_bottom);
                    }
                }
                if (visuals::settings::boxoutline) {
                    dl->AddRect(tl - ImVec2(1, 1), br + ImVec2(1, 1), IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
                }
                dl->AddRect(tl, br, box_color, 0.0f, 0, 1.5f);
            }
            else if (visuals::settings::boxtype == 2) {
                // Preview stand-in for the 3D style: front face plus a depth offset.
                const ImVec2 depth(box_w * 0.22f, box_h * 0.10f);
                const ImVec2 back_tl(tl.x + depth.x, tl.y - depth.y);
                const ImVec2 back_br(br.x + depth.x, br.y - depth.y);

                // No fill here on purpose: the 3D style is outlines only.

                if (visuals::settings::boxoutline) {
                    dl->AddRect(back_tl, back_br, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
                    dl->AddRect(tl, br, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
                }

                dl->AddRect(back_tl, back_br, box_color, 0.0f, 0, 1.5f);
                dl->AddLine(tl, back_tl, box_color, 1.5f);
                dl->AddLine(ImVec2(br.x, tl.y), ImVec2(back_br.x, back_tl.y), box_color, 1.5f);
                dl->AddLine(ImVec2(tl.x, br.y), ImVec2(back_tl.x, back_br.y), box_color, 1.5f);
                dl->AddLine(br, back_br, box_color, 1.5f);
                dl->AddRect(tl, br, box_color, 0.0f, 0, 1.5f);
            }
            else {
                const float len_w = box_w * 0.28f;
                const float len_h = box_h * 0.18f;
                const std::pair<ImVec2, ImVec2> corners[] = {
                    { ImVec2(tl.x, tl.y), ImVec2(tl.x, tl.y + len_h) },
                    { ImVec2(tl.x, tl.y), ImVec2(tl.x + len_w, tl.y) },
                    { ImVec2(br.x - len_w, tl.y), ImVec2(br.x, tl.y) },
                    { ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + len_h) },
                    { ImVec2(tl.x, br.y - len_h), ImVec2(tl.x, br.y) },
                    { ImVec2(tl.x, br.y), ImVec2(tl.x + len_w, br.y) },
                    { ImVec2(br.x - len_w, br.y), ImVec2(br.x, br.y) },
                    { ImVec2(br.x, br.y - len_h), ImVec2(br.x, br.y) },
                };
                if (visuals::settings::boxoutline) {
                    for (const auto& corner : corners) {
                        dl->AddLine(corner.first, corner.second, IM_COL32(0, 0, 0, 255), 5.5f);
                    }
                }
                for (const auto& corner : corners) {
                    dl->AddLine(corner.first, corner.second, box_color, 1.5f);
                }
            }
        }

        if (visuals::settings::healthbars) {
            const float bar_w = 3.0f;
            const float bar_off = 3.0f;

            // Animate the bar so it visibly drains and refills like a live target.
            static float preview_health_phase = 0.0f;
            preview_health_phase += ImGui::GetIO().DeltaTime * 0.35f;
            if (preview_health_phase > 2.0f) preview_health_phase -= 2.0f;
            const float ratio = 0.08f + 0.92f * (0.5f - 0.5f * cosf(preview_health_phase * 3.14159265f));

            // Same four positions as the live ESP, so the preview shows the chosen side.
            const int side = visuals::settings::healthbarside;
            const bool vertical = (side == 0 || side == 1);

            ImVec2 bar_min;
            ImVec2 bar_max;

            if (side == 1) {
                bar_min = ImVec2(br.x + bar_off, tl.y);
                bar_max = ImVec2(bar_min.x + bar_w, br.y);
            }
            else if (side == 2) {
                bar_min = ImVec2(tl.x, tl.y - bar_off - bar_w);
                bar_max = ImVec2(br.x, bar_min.y + bar_w);
            }
            else if (side == 3) {
                bar_min = ImVec2(tl.x, br.y + bar_off);
                bar_max = ImVec2(br.x, bar_min.y + bar_w);
            }
            else {
                bar_min = ImVec2(tl.x - bar_off - bar_w, tl.y);
                bar_max = ImVec2(bar_min.x + bar_w, br.y);
            }

            dl->AddRectFilled(bar_min, bar_max, IM_COL32(30, 30, 30, 200));

            const float inner_len = (vertical ? (bar_max.y - bar_min.y) : (bar_max.x - bar_min.x)) - 2.0f;

            if (inner_len > 2.0f) {
                ImVec2 fill_min;
                ImVec2 fill_max;

                if (vertical) {
                    fill_min = ImVec2(bar_min.x + 1.0f, bar_max.y - 1.0f - inner_len * ratio);
                    fill_max = ImVec2(bar_max.x - 1.0f, bar_max.y - 1.0f);
                }
                else {
                    fill_min = ImVec2(bar_min.x + 1.0f, bar_min.y + 1.0f);
                    fill_max = ImVec2(bar_min.x + 1.0f + inner_len * ratio, bar_max.y - 1.0f);
                }

                const ImU32 fill_top = rgb(visuals::settings::healthbarsglowcolor);
                const ImU32 fill_bottom = rgb(visuals::settings::healthbarcolor2);

                dl->AddRectFilledMultiColor(fill_min, fill_max, fill_top, fill_top, fill_bottom, fill_bottom);
            }

            dl->AddRect(bar_min, bar_max, IM_COL32(0, 0, 0, 255));

            const std::string pct = std::to_string(static_cast<int>(ratio * 100.0f));
            const ImVec2 pct_size = ImGui::CalcTextSize(pct.c_str());

            ImVec2 pct_pos;

            if (side == 1) {
                pct_pos = ImVec2(bar_max.x + 2.0f, tl.y + (br.y - tl.y - pct_size.y) * 0.5f);
            }
            else if (side == 2) {
                pct_pos = ImVec2(bar_min.x + (bar_max.x - bar_min.x - pct_size.x) * 0.5f, bar_min.y - pct_size.y - 1.0f);
            }
            else if (side == 3) {
                pct_pos = ImVec2(bar_min.x + (bar_max.x - bar_min.x - pct_size.x) * 0.5f, bar_max.y + 1.0f);
            }
            else {
                pct_pos = ImVec2(bar_min.x - pct_size.x - 2.0f, tl.y + (br.y - tl.y - pct_size.y) * 0.5f);
            }

            if (pct_pos.x > origin.x + 1.0f) {
                dl->AddText(font, ImGui::GetFontSize(), pct_pos, IM_COL32(255, 255, 255, 255), pct.c_str());
            }

            // Drag the bar itself to any side of the box: it snaps to the nearest edge and locks
            // there, which replaces the old dropdown.
            static bool dragging_side = false;

            const ImVec2 grab_min(bar_min.x - 4.0f, bar_min.y - 4.0f);
            const ImVec2 grab_max(bar_max.x + 4.0f, bar_max.y + 4.0f);
            const bool over_bar = ImGui::IsMouseHoveringRect(grab_min, grab_max, false);

            if (over_bar && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                dragging_side = true;

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                dragging_side = false;

            if (dragging_side) {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const float half_w = ImMax(1.0f, (br.x - tl.x) * 0.5f);
                const float half_h = ImMax(1.0f, (br.y - tl.y) * 0.5f);
                const float dx = (mouse.x - (tl.x + br.x) * 0.5f) / half_w;
                const float dy = (mouse.y - (tl.y + br.y) * 0.5f) / half_h;

                visuals::settings::healthbarside =
                    (fabsf(dx) > fabsf(dy)) ? (dx < 0.0f ? 0 : 1) : (dy < 0.0f ? 2 : 3);
            }

        }

        if (visuals::settings::skeletons) {
            const ImU32 skel_color = rgba(visuals::settings::skeletonscolor);
            const ImU32 skel_outline = IM_COL32(0, 0, 0, 255);

            const ImVec2 neck(center_x, tl.y + box_h * 0.10f);
            const ImVec2 hrp(center_x, tl.y + box_h * 0.38f);
            const ImVec2 lhand(tl.x + box_w * 0.10f, hrp.y + box_h * 0.16f);
            const ImVec2 rhand(br.x - box_w * 0.10f, hrp.y + box_h * 0.16f);
            const ImVec2 lfoot(tl.x + box_w * 0.30f, br.y - box_h * 0.02f);
            const ImVec2 rfoot(br.x - box_w * 0.30f, br.y - box_h * 0.02f);

            const std::pair<ImVec2, ImVec2> bones[] = {
                { neck, hrp }, { hrp, lhand }, { hrp, rhand }, { hrp, lfoot }, { hrp, rfoot },
            };
            for (const auto& bone : bones) {
                dl->AddLine(bone.first, bone.second, skel_outline, 3.5f);
                dl->AddLine(bone.first, bone.second, skel_color, 2.0f);
            }
        }

        static bool name_drag = false;
        static bool tool_drag = false;
        static bool dist_drag = false;

        // Drag a label and it snaps to the closest edge of the box, exactly like the health bar.
        auto drag_to_side = [&](bool& dragging, int* side_setting) {
            if (!dragging) return;

            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const float half_w = ImMax(1.0f, (br.x - tl.x) * 0.5f);
            const float half_h = ImMax(1.0f, (br.y - tl.y) * 0.5f);
            const float dx = (mouse.x - (tl.x + br.x) * 0.5f) / half_w;
            const float dy = (mouse.y - (tl.y + br.y) * 0.5f) / half_h;

            *side_setting = (fabsf(dx) > fabsf(dy)) ? (dx < 0.0f ? 0 : 1) : (dy < 0.0f ? 2 : 3);
        };

        auto draw_label = [&](bool& dragging, int* side_setting, const std::string& text, float size,
            ImU32 color, int stack) {
            const ImVec2 text_size(text_width(text, size), size);
            const ImVec2 pos = render::label_position(*side_setting,
                Vector2(tl.x, tl.y), Vector2(br.x, br.y), text_size, stack);

            draw_text(text, pos, color, size);

            const ImVec2 grab_min(pos.x - 2.0f, pos.y - 2.0f);
            const ImVec2 grab_max(pos.x + text_size.x + 2.0f, pos.y + size + 2.0f);
            const bool over = ImGui::IsMouseHoveringRect(grab_min, grab_max, false);

            if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) dragging = true;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging = false;


            drag_to_side(dragging, side_setting);
        };

        if (show_name) {
            draw_label(name_drag, &visuals::settings::nameside, "Player",
                name_size, rgb(visuals::settings::namecolor), 0);
        }

        if (show_tool) {
            const int stack =
                (show_name && visuals::settings::nameside == visuals::settings::toolside) ? 1 : 0;

            draw_label(tool_drag, &visuals::settings::toolside, "[None]", info_size,
                IM_COL32(255, 255, 255, 255), stack);
        }

        if (show_dist) {
            const int stack =
                ((show_name && visuals::settings::nameside == visuals::settings::distanceside) ? 1 : 0) +
                ((show_tool && visuals::settings::toolside == visuals::settings::distanceside) ? 1 : 0);

            draw_label(dist_drag, &visuals::settings::distanceside, "42m", info_size,
                rgb(visuals::settings::distancecolor), stack);
        }

    }

    void RenderVisualsTab() {
        float child_width = (ImGui::GetContentRegionAvail().x / 2) - 4;

        ImGui::BeginGroup();
        {
            ImAdd::BeginChild("ESP", ImVec2(child_width, 0));
            {
                SDK::Gui::Checkbox("Enabled", &visuals::settings::espenabled);

                SDK::Gui::Checkbox("Team Check", &visuals::settings::espteamcheck);

                SDK::Gui::Checkbox("Team Color", &visuals::settings::esp_teamcolor);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##TeamColor", visuals::settings::esp_teamcolorvalue);

                SDK::Gui::Checkbox("Boxes", &visuals::settings::boxesp);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##BoxColor", visuals::settings::boxcolor);

                if (visuals::settings::boxesp) {
                    SDK::Gui::Checkbox("Filled", &visuals::settings::boxfilled);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 74);
                    SDK::Gui::ColorEdit4("##BoxFillColor", visuals::settings::boxfilledcolor);
                    SDK::Gui::SameLine(0, 4);
                    SDK::Gui::ColorEdit4("##BoxFillBlend", visuals::settings::boxfilledcolor2);

                    const char* fill_styles[] = { "Solid", "Gradient" };
                    SDK::Gui::ComboBox("Fill Style", &visuals::settings::boxfillstyle, fill_styles, 2);

                    const char* box_styles[] = { "Normal", "Corner", "3D" };
                    SDK::Gui::ComboBox("Box Style", &visuals::settings::boxtype, box_styles, 3);

                    const char* sizing_modes[] = { "Bounding", "Static" };
                    SDK::Gui::ComboBox("Box Size", &visuals::settings::boxsizing, sizing_modes, 2);
                    help_marker("Bounding resizes the box to the avatar's outline as it turns. Static keeps one fixed size for every player regardless of facing.");
                }

                SDK::Gui::Checkbox("Names", &visuals::settings::nameesp);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##NameColor", visuals::settings::namecolor);

                SDK::Gui::Checkbox("Chams", &visuals::settings::chams);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##ChamsColor", visuals::settings::chamscolor);

                if (visuals::settings::chams) {
                    SDK::Gui::Checkbox("Chams Outline", &visuals::settings::chamsoutline);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::ColorEdit4("##ChamsOutlineColor", visuals::settings::chamsoutlinecolor);

                    SDK::Gui::Checkbox("Chams Glow", &visuals::settings::chamsglow);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::ColorEdit4("##ChamsGlowColor", visuals::settings::chamsglowcolor);

                    SDK::Gui::Checkbox("Chams Wave", &visuals::settings::chamswave);

                    SDK::Gui::Checkbox("Chams Rainbow", &visuals::settings::chamsrainbpw);
                }

                SDK::Gui::Checkbox("Health Bars", &visuals::settings::healthbars);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 74);
                SDK::Gui::ColorEdit4("##HealthBarColorTop", visuals::settings::healthbarsglowcolor);
                SDK::Gui::SameLine(0, 4);
                SDK::Gui::ColorEdit4("##HealthBarColorBottom", visuals::settings::healthbarcolor2);


                SDK::Gui::Checkbox("Tracers", &visuals::settings::tracers);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##LineColor", visuals::settings::linecolor);

                SDK::Gui::Checkbox("Skeletons", &visuals::settings::skeletons);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##SkeletonsColor", visuals::settings::skeletonscolor);

                SDK::Gui::Checkbox("Distance", &visuals::settings::distanceesp);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##DistanceColor", visuals::settings::distancecolor);

                SDK::Gui::Checkbox("Tool Name", &visuals::settings::toolesp);

                SDK::Gui::Checkbox("China Hats", &visuals::settings::chinahats);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##ChinaHatColor", visuals::settings::chinahatcolor);


                SDK::Gui::Checkbox("Draw LocalPlayer", &visuals::settings::localesp);


                ImAdd::EndChild();

            }
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            {
                ImAdd::BeginChild("Extra", ImVec2(child_width, 0));
                {
                    SDK::Gui::Checkbox("Bullet Tracers", &bullet_tracer::settings::enabled);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::ColorEdit4("##BulletTracerColor", bullet_tracer::settings::tracer_color);

                    if (bullet_tracer::settings::enabled) {
                        SDK::Gui::SliderFloat("Tracer Duration", &bullet_tracer::settings::tracer_duration, 0.5f, 5.0f, "%.1fs");
                        SDK::Gui::SliderFloat("Tracer Thickness", &bullet_tracer::settings::tracer_thickness, 1.0f, 5.0f, "%.1f");
                        SDK::Gui::SliderFloat("Detection Range", &bullet_tracer::settings::detection_range, 100.0f, 2000.0f, "%.0f");
                        SDK::Gui::SliderFloat("Health Threshold", &bullet_tracer::settings::health_threshold, 1.0f, 20.0f, "%.1f");
                        SDK::Gui::Checkbox("Rainbow Tracers", &bullet_tracer::settings::rainbow_tracers);
                        SDK::Gui::Checkbox("Fade Out", &bullet_tracer::settings::fade_out);
                    }
                    SDK::Gui::Checkbox("Trail", &player_trail::settings::enabled);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::ColorEdit4("##TrailColor", player_trail::settings::trail_color);

                    SDK::Gui::Checkbox("Enemy Trails", &player_trail::settings::enemy_enabled);
                    SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                    SDK::Gui::ColorEdit4("##EnemyTrailColor", player_trail::settings::enemy_color);


                    if (globals::gamename == "Fallen") {

                        SDK::Gui::Checkbox("Use NPC's", &features::visuals::soldier_esp_enabled);


                        SDK::Gui::Checkbox("FullBright", &visuals::settings::fullbright);

                        SDK::Gui::Checkbox("Resource ESP", &features::visuals::resource_esp_enabled);

                        SDK::Gui::Checkbox("Stone ESP", &features::visuals::stoneesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##StoneColor", visuals::settings::stoneespcolor);

                        SDK::Gui::Checkbox("Phosphate ESP", &features::visuals::phosphateesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##PhosphateColor", visuals::settings::phosphateespcolor);

                        SDK::Gui::Checkbox("Metal ESP", &features::visuals::metalesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##MetalColor", visuals::settings::metalespcolor);


                        SDK::Gui::Checkbox("Corn ESP", &features::visuals::cornesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##CornColor", visuals::settings::cornespcolor);

                        SDK::Gui::Checkbox("Wool ESP", &features::visuals::woolesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##WoolColor", visuals::settings::woolespcolor);

                        SDK::Gui::Checkbox("Pumpkin ESP", &features::visuals::pumpkinesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##PumpkinColor", visuals::settings::pumpkinespcolor);

                        SDK::Gui::Checkbox("Tomato ESP", &features::visuals::tomatoesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##TomatoColor", visuals::settings::tomatoespcolor);

                        SDK::Gui::Checkbox("Lemon ESP", &features::visuals::lemonesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##LemonColor", visuals::settings::lemonespcolor);

                        SDK::Gui::Checkbox("Blueberry ESP", &features::visuals::blueberryesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##BlueberryColor", visuals::settings::blueberryespcolor);

                        SDK::Gui::Checkbox("Raspberry ESP", &features::visuals::raspberryesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##RaspberryColor", visuals::settings::raspberryespcolor);

                        
                        ImGui::Spacing();
           
                        ImGui::Spacing();
                        SDK::Gui::Checkbox("Animal ESP", &features::visuals::animal_esp_enabled);

                        SDK::Gui::Checkbox("Deer ESP", &features::visuals::deeresp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##DeerColor", visuals::settings::deercolor);

                        SDK::Gui::Checkbox("Wild Boar ESP", &features::visuals::wildboaresp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##WildBoarColor", visuals::settings::wildboarcolor);

                        SDK::Gui::Checkbox("Wolf ESP", &features::visuals::wolfesp);
                        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                        SDK::Gui::ColorEdit4("##WolfColor", visuals::settings::wolfcolor);
                    }
                   

                }
                ImAdd::EndChild();
            }
            ImGui::EndGroup();
        }
    }

    void RenderWorldTab() {
        float child_width = (ImGui::GetContentRegionAvail().x / 2) - 4;

        ImGui::BeginGroup();
        {
            ImAdd::BeginChild("LocalPlayer Modifications", ImVec2(child_width, 0));
            {
                SDK::Gui::Checkbox("Desync", &globals::misc::desync_enabled);


                std::string current_name = globals::misc::desyncbind.get_key_name();
                if (globals::misc::desyncbind.waiting_for_input) {
                    current_name = "[...]";
                }
                float text_width = ImGui::CalcTextSize(current_name.c_str()).x;
                float bind_width = ImMax(60.0f, text_width + 16.0f);

                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - bind_width);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
                OverlayManager::OptimizedKeybind(&globals::misc::desyncbind, ImVec2(bind_width, 18.0f));

                SDK::Gui::Checkbox("Desync interval", &globals::misc::desync_interval_enabled);
                SDK::Gui::SliderInt("Desync Duration (ms)", &globals::misc::desync_pulse_interval_ms, 0, 100, "%d ms");
         //       ImAdd::CheckBox("Desync Visualise (ENABLE VISUALS)", &globals::visuals::desyncvisual);
          //      ImGui::Text("Desync Chams Color");
           //     ImGui::SameLine();
         


                SDK::Gui::Checkbox("Headless", &features::playermodifications::headless);
                SDK::Gui::Checkbox("Korblox", &features::playermodifications::korblox);
                SDK::Gui::Checkbox("ForceField Chams", &features::playermodifications::localplayerforcefieldchams);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##color1", features::playermodifications::color1);

                SDK::Gui::Checkbox("ForceField Tool Chams", &features::playermodifications::localplayerforcefieldtoolchams);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit4("##color2", features::playermodifications::color2);

                SDK::Gui::Checkbox("Animation Changer", &features::playermodifications::animationchanger);

                const char* animations[] = {
            "cartoon", "zombie", "adidas_comm", "adidas_sport", "elder", "vampire",
            "catwalk_glam", "oldschool", "bubbly", "stylish", "ninja", "mage",
            "wicked", "levitation", "superhero", "toy", "robot", "werewolf",
            "astronaut", "knight", "pirate", "nfl", "no_boundaries", "bold", "rthro","dolphin_dive","floss"
                };

                SDK::Gui::ComboBox("Run Animation", &features::playermodifications::runanimation, animations, 27);
                SDK::Gui::ComboBox("Walk Animation", &features::playermodifications::walkanimation, animations, 27);
                SDK::Gui::ComboBox("Jump Animation", &features::playermodifications::jumpanimation, animations, 27);
                SDK::Gui::ComboBox("Fall Animation", &features::playermodifications::fallanimation, animations, 27);
                SDK::Gui::ComboBox("Idle Animation", &features::playermodifications::idleanimation, animations, 27);
                SDK::Gui::ComboBox("Climb Animation", &features::playermodifications::climbanimation, animations, 27);
                SDK::Gui::ComboBox("Swim Animation", &features::playermodifications::swimanimation, animations, 27);

            }
            ImAdd::EndChild();

        }
        ImGui::EndGroup();

        ImGui::SameLine();

        // Lighting lives with the other world/environment controls rather than in Settings.
        ImGui::BeginGroup();
        {
            ImAdd::BeginChild("Lighting", ImVec2(child_width, 0));
            {
                SDK::Gui::Checkbox("Ambiance", &globals::ambianceon);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit3("##AmbianceColor", globals::aimbiancecolor);

                SDK::Gui::Checkbox("Fog", &globals::fogon);
                SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
                SDK::Gui::ColorEdit3("##FogColor", globals::fogcolor);

                SDK::Gui::SliderFloat("Fog Start", &globals::fogstart, 0.0f, 100.0f);
            }
            ImAdd::EndChild();
        }
        ImGui::EndGroup();
    }

    void RenderSettingsTab() {
        float childWidth = (ImGui::GetContentRegionAvail().x / 2) - 4;

        ImGui::BeginGroup();
        ImAdd::BeginChild("UI Settings", ImVec2(childWidth, 300));
        ImGui::Separator();

        SDK::Gui::Checkbox("Show Watermark", &globals::watermark);
        SDK::Gui::Checkbox("Show Keybinds", &globals::keybindlist);

        SDK::Gui::Checkbox("Snowfall", &globals::snowfall);

        SDK::Gui::Checkbox("V-Sync", &overlay::ENABLE_VSYNC);

        SDK::Gui::Checkbox("Crosshair", &features::aimbot::dynamic_mouse);
        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 40);
        SDK::Gui::ColorEdit4("##MouseColor", features::aimbot::mouse_color);

        ImGui::Text("Menu Keybind");

        std::string menu_key_name = globals::menubind.waiting_for_input ? "[...]" : globals::menubind.get_key_name();
        float menu_key_width = ImMax(60.0f, ImGui::CalcTextSize(menu_key_name.c_str()).x + 16.0f);

        SDK::Gui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - menu_key_width);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
        OverlayManager::OptimizedKeybind(&globals::menubind, ImVec2(menu_key_width, 18.0f));


        ImAdd::EndChild();

        ImAdd::BeginChild("Aimbot Flags", ImVec2(childWidth, 0));
        {
            std::vector<const char*> flag_items = { "Team", "Knocked", "Range", "Health", "WallCheck", "Grabbed", "Forcefield" };
            SDK::Gui::MultiComboBox2("Flags", features::aimbot::flags, flag_items);
            SDK::Gui::Checkbox("Auto-Switch On Condition", &features::aimbot::autoswitch);
            SDK::Gui::SliderFloat("Range", &features::aimbot::range, 1.0f, 10000.0f);
            SDK::Gui::SliderFloat("Health", &features::aimbot::healththreshhold, 0.0f, 100.0f);
        }
        ImAdd::EndChild();

        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImAdd::BeginChild("Configs", ImVec2(childWidth, 360));
        {
            float width = (ImGui::GetContentRegionAvail().x / 2) - 4;
            const std::vector<std::string>& configlist = ConfigSystem::GetConfigList();
            static char configNameBuffer[128] = "";
            static int selectedConfig = 0;

            SDK::Gui::InputText("Config Name", configNameBuffer, IM_ARRAYSIZE(configNameBuffer));

            std::vector<const char*> configNames;
            for (const auto& config : configlist) {
                configNames.push_back(config.c_str());
            }

            if (!configNames.empty()) {
                SDK::Gui::ComboBox("Config List", &selectedConfig, configNames.data(), configNames.size());
                if (selectedConfig >= configNames.size()) {
                    selectedConfig = 0;
                }
            }
            else {
                SDK::Gui::Text("No configs found");
                selectedConfig = 0;
            }
            if (SDK::Gui::Button("Create Config", ImVec2(width, 20))) {
                if (strlen(configNameBuffer) > 0) {
                    if (ConfigSystem::CreateConfig(configNameBuffer)) {
                        memset(configNameBuffer, 0, sizeof(configNameBuffer));
                    }
                }
            }

            SDK::Gui::SameLine();
            if (SDK::Gui::Button("Load Config", ImVec2(width, 20))) {
                if (!configlist.empty() && selectedConfig < configlist.size()) {
                    ConfigSystem::LoadConfig(configlist[selectedConfig]);
                }
            }
            if (SDK::Gui::Button("Overwrite Config", ImVec2(width * 2 + 8, 20))) {
                if (!configlist.empty() && selectedConfig < configlist.size()) {
                    ConfigSystem::SaveConfig(configlist[selectedConfig]);
                }
            }
            if (SDK::Gui::Button("Refresh List", ImVec2(width * 2 + 8, 20))) {
                ConfigSystem::RefreshConfigList();
                const auto& newList = ConfigSystem::GetConfigList();
                if (selectedConfig >= newList.size() && !newList.empty()) {
                    selectedConfig = 0;
                }
            }
            if (SDK::Gui::Button("Set As Autoload", ImVec2(width * 2 + 8, 20))) {
                if (!configlist.empty() && selectedConfig < configlist.size()) {
                    ConfigSystem::SetAutoloadConfig(configlist[selectedConfig]);
                }
            }
            std::string autoconfig = "Autoload Config: " + ConfigSystem::GetAutoloadConfig();
            SDK::Gui::Text(autoconfig.c_str());

        }
        ImAdd::EndChild();
        ImGui::EndGroup();
    }

    bool CreateDeviceD3D(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = SWAP_CHAIN_BUFFERS;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };

        HRESULT res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            ARRAYSIZE(featureLevelArray),
            D3D11_SDK_VERSION,
            &sd,
            &d3d_resources.swapchain,
            &d3d_resources.device,
            &featureLevel,
            &d3d_resources.context
        );

        if (FAILED(res)) {
            res = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                createDeviceFlags,
                featureLevelArray,
                ARRAYSIZE(featureLevelArray),
                D3D11_SDK_VERSION,
                &sd,
                &d3d_resources.swapchain,
                &d3d_resources.device,
                &featureLevel,
                &d3d_resources.context
            );
        }

        if (FAILED(res)) return false;

        CreateRenderTarget();
        d3d_resources.initialized = true;
        return true;
    }

    void CleanupDeviceD3D() {
        CleanupRenderTarget();

        if (d3d_resources.swapchain) {
            d3d_resources.swapchain->Release();
            d3d_resources.swapchain = nullptr;
        }
        if (d3d_resources.context) {
            d3d_resources.context->Release();
            d3d_resources.context = nullptr;
        }
        if (d3d_resources.device) {
            d3d_resources.device->Release();
            d3d_resources.device = nullptr;
        }

        d3d_resources.initialized = false;
    }

    void CreateRenderTarget() {
        ID3D11Texture2D* back_buffer = nullptr;
        d3d_resources.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
        if (back_buffer) {
            d3d_resources.device->CreateRenderTargetView(back_buffer, nullptr, &d3d_resources.render_target);
            back_buffer->Release();
        }
    }

    void CleanupRenderTarget() {
        if (d3d_resources.render_target) {
            d3d_resources.render_target->Release();
            d3d_resources.render_target = nullptr;
        }
    }

    bool SetupGPUTiming() {
        if (!d3d_resources.device) return false;

        D3D11_QUERY_DESC query_desc = {};

        query_desc.Query = D3D11_QUERY_TIMESTAMP;
        if (FAILED(d3d_resources.device->CreateQuery(&query_desc, &d3d_resources.timestamp_start))) return false;
        if (FAILED(d3d_resources.device->CreateQuery(&query_desc, &d3d_resources.timestamp_end))) return false;

        query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (FAILED(d3d_resources.device->CreateQuery(&query_desc, &d3d_resources.disjoint_query))) return false;

        d3d_resources.gpu_timing_available = true;
        return true;
    }

    void CleanupGPUTiming() {
        if (d3d_resources.timestamp_start) {
            d3d_resources.timestamp_start->Release();
            d3d_resources.timestamp_start = nullptr;
        }
        if (d3d_resources.timestamp_end) {
            d3d_resources.timestamp_end->Release();
            d3d_resources.timestamp_end = nullptr;
        }
        if (d3d_resources.disjoint_query) {
            d3d_resources.disjoint_query->Release();
            d3d_resources.disjoint_query = nullptr;
        }
        d3d_resources.gpu_timing_available = false;
    }

    void UpdateGPUTimings() {
        if (!d3d_resources.gpu_timing_available) return;

        UINT64 start_time, end_time;
        if (SUCCEEDED(d3d_resources.context->GetData(d3d_resources.timestamp_start, &start_time, sizeof(UINT64), 0)) &&
            SUCCEEDED(d3d_resources.context->GetData(d3d_resources.timestamp_end, &end_time, sizeof(UINT64), 0)) &&
            SUCCEEDED(d3d_resources.context->GetData(d3d_resources.disjoint_query, &d3d_resources.disjoint_data, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0))) {

            if (!d3d_resources.disjoint_data.Disjoint) {
                float gpu_time = static_cast<float>(end_time - start_time) / static_cast<float>(d3d_resources.disjoint_data.Frequency);
                OverlayManager::SetGPUTime(gpu_time);
            }
        }
    }

    HWND CreateOverlayWindow() {
        ImGui_ImplWin32_EnableDpiAwareness();

        WNDCLASSEXW wc = {
            sizeof(WNDCLASSEXW),
            CS_CLASSDC,
            WndProc,
            0L,
            0L,
            GetModuleHandle(nullptr),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            L"ChromeWtfOverlay",
            nullptr
        };

        window_config.wc = wc;

        if (!::RegisterClassExW(&window_config.wc)) {
            return nullptr;
        }

        HWND hwnd = ::CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            window_config.wc.lpszClassName,
            L"chrome.wtf Overlay",
            WS_POPUP,
            0, 0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            nullptr,
            nullptr,
            window_config.wc.hInstance,
            nullptr
        );

  

        if (!hwnd) {
            return nullptr;
        }
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

        MARGINS margin = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margin);

        return hwnd;
    }

    void UpdateWindowPosition(HWND overlay_hwnd) {
        HWND target = FindWindowA(0, "Roblox");
        HWND foregroundWindow = GetForegroundWindow();

        static bool collapsed = false;
        static int unfocused_frames = 0;
        static RECT last_rect = { 0, 0, 0, 0 };
        static int last_width = 0;
        static int last_height = 0;

        if (target != foregroundWindow && overlay_hwnd != foregroundWindow)
        {
            // A click that activates the overlay can leave the window unfocused for a
            // frame or two. Collapsing straight away made the menu vanish and reappear,
            // so only hide it once the focus loss actually sticks.
            if (++unfocused_frames < 12) return;
            if (collapsed) return;

            collapsed = true;
            last_rect = { 0, 0, 0, 0 };
            last_width = last_height = 0;
            MoveWindow(overlay_hwnd, 0, 0, 0, 0, true);
            return;
        }

        unfocused_frames = 0;

        if (!target) return;

        RECT rect;
        if (!GetWindowRect(target, &rect)) return;

        int rsize_x = rect.right - rect.left;
        int rsize_y = rect.bottom - rect.top;

        if (IsTargetWindowFullscreen(target))
        {
            rsize_x += 16;
            rect.right -= 16;
        }
        else
        {
            rsize_y -= 39;
            rect.left += 8;
            rect.top += 31;
            rect.right -= 16;
        }
        rsize_x -= 16;

        if (rsize_x <= 0 || rsize_y <= 0) return;

        if (!collapsed &&
            rect.left == last_rect.left && rect.top == last_rect.top &&
            rsize_x == last_width && rsize_y == last_height)
            return;

        collapsed = false;
        last_rect = rect;
        last_width = rsize_x;
        last_height = rsize_y;
        MoveWindow(overlay_hwnd, rect.left, rect.top, rsize_x, rsize_y, TRUE);
    }

    bool IsTargetWindowActive() {
        HWND target = FindWindowA(nullptr, "Roblox");
        if (!target) return false;

        HWND foreground = GetForegroundWindow();
        return (foreground == target || foreground == window_config.hwnd);
    }

    bool IsTargetWindowFullscreen(HWND target_hwnd) {
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(MonitorFromWindow(target_hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
        {
            RECT rect;
            if (GetWindowRect(target_hwnd, &rect))
            {
                return rect.left == monitorInfo.rcMonitor.left
                    && rect.right == monitorInfo.rcMonitor.right
                    && rect.top == monitorInfo.rcMonitor.top
                    && rect.bottom == monitorInfo.rcMonitor.bottom;
            }
        }
    }

    void SetOverlayTransparency(bool transparent) {
        if (!window_config.hwnd) return;
        if (window_config.is_transparent == transparent) return;

        LONG ex_style = GetWindowLong(window_config.hwnd, GWL_EXSTYLE);

        if (transparent) {
            ex_style |= WS_EX_TRANSPARENT;
        }
        else {
            ex_style &= ~WS_EX_TRANSPARENT;
        }

        SetWindowLong(window_config.hwnd, GWL_EXSTYLE, ex_style);
        window_config.is_transparent = transparent;
    }

    void ToggleVisibility() {
        visible = !visible;
        topbar_visible = visible;
        OverlayManager::SetVisible(visible);
        OverlayManager::MarkNeedsRedraw();
    }

    void HandleResize() {
        if (d3d_resources.resize_width != 0 && d3d_resources.resize_height != 0) {
            CleanupRenderTarget();
            d3d_resources.swapchain->ResizeBuffers(
                0,
                d3d_resources.resize_width,
                d3d_resources.resize_height,
                DXGI_FORMAT_UNKNOWN,
                0
            );
            d3d_resources.resize_width = d3d_resources.resize_height = 0;
            CreateRenderTarget();
        }
    }

    void BeginRender() {
        if (!d3d_resources.initialized) return;

        if (d3d_resources.gpu_timing_available) {
            d3d_resources.context->Begin(d3d_resources.disjoint_query);
            d3d_resources.context->End(d3d_resources.timestamp_start);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EndRender() {
        if (!d3d_resources.initialized) return;

        if (d3d_resources.gpu_timing_available) {
            d3d_resources.context->End(d3d_resources.timestamp_end);
            d3d_resources.context->End(d3d_resources.disjoint_query);
        }

        ImGui::Render();

        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        d3d_resources.context->OMSetRenderTargets(1, &d3d_resources.render_target, nullptr);
        d3d_resources.context->ClearRenderTargetView(d3d_resources.render_target, clear_color);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void PresentFrame() {
        if (!d3d_resources.initialized) return;

        UINT sync_interval = ENABLE_VSYNC ? 1 : 0;
        UINT present_flags = 0;

        HRESULT hr = d3d_resources.swapchain->Present(sync_interval, present_flags);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            CleanupDeviceD3D();
            CreateDeviceD3D(window_config.hwnd);
        }

        UpdateGPUTimings();
    }

    void AdaptiveFrameLimit() {
        if (!visible && OverlayManager::IsIdleMode()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return;
        }

        static auto last_frame_end = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();

        float target_frame_time = 1.0f / TARGET_FPS;
        float actual_frame_time = std::chrono::duration<float>(now - last_frame_end).count();

        if (actual_frame_time < target_frame_time) {
            const float remaining = target_frame_time - actual_frame_time;

            // sleep_for rounds up to the system timer tick (~15 ms by default), which turns a
            // 144 fps cap into uneven frames that show up as judder while dragging windows.
            // Sleep the bulk of the wait, then spin out the last couple of milliseconds.
            constexpr float spin_time = 0.002f;

            if (remaining > spin_time) {
                const auto sleep_time = std::chrono::microseconds(
                    static_cast<long long>((remaining - spin_time) * 1000000));
                std::this_thread::sleep_for(sleep_time);
            }

            while (std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - now).count() < remaining) {
                YieldProcessor();
            }
        }

        last_frame_end = std::chrono::high_resolution_clock::now();
    }

    void ProcessMessages() {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                should_exit = true;
                break;
            }
        }
    }

    void UpdateFrameStats() {
        static auto last_update = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();

        float delta = std::chrono::duration<float>(now - last_update).count();
        if (delta >= PERFORMANCE_UPDATE_INTERVAL) {
            last_update = now;
            OverlayManager::MarkNeedsRedraw();
        }
    }

    namespace Utils {
        std::string FormatBytes(size_t bytes) {
            const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
            int suffix_index = 0;
            double size = static_cast<double>(bytes);

            while (size >= 1024 && suffix_index < 4) {
                size /= 1024;
                suffix_index++;
            }

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << size << " " << suffixes[suffix_index];
            return oss.str();
        }

        ImVec4 HexToColor(uint32_t hex, float alpha) {
            return ImVec4(
                ((hex >> 16) & 0xFF) / 255.0f,
                ((hex >> 8) & 0xFF) / 255.0f,
                (hex & 0xFF) / 255.0f,
                alpha
            );
        }

        uint32_t ColorToHex(const ImVec4& color) {
            return ((static_cast<uint32_t>(color.x * 255) << 16) |
                (static_cast<uint32_t>(color.y * 255) << 8) |
                (static_cast<uint32_t>(color.z * 255)));
        }

        bool IsPointInRect(const ImVec2& point, const ImVec2& rect_min, const ImVec2& rect_max) {
            return point.x >= rect_min.x && point.x <= rect_max.x &&
                point.y >= rect_min.y && point.y <= rect_max.y;
        }

        float GetDeltaTime() {
            return ImGui::GetIO().DeltaTime;
        }

        void OptimizeImGuiStyle(ImGuiStyle& style) {
            style.WindowRounding = 0.0f;
            style.ChildRounding = 0.0f;
            style.FrameRounding = 0.0f;
            style.PopupRounding = 0.0f;
            style.ScrollbarRounding = 0.0f;
            style.GrabRounding = 0.0f;
            style.TabRounding = 0.0f;
            style.WindowShadowSize = 10.0f;

            ImVec4* colors = style.Colors;
            colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
            colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

            ImVec4 accent = ImVec4(0.36f, 0.25f, 0.83f, 1.00f);
            colors[ImGuiCol_CheckMark] = accent;
            colors[ImGuiCol_SliderGrab] = accent;
            colors[ImGuiCol_SliderGrabActive] = accent;
            colors[ImGuiCol_Button] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
        }

        size_t GetProcessMemoryUsage() {
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                return pmc.WorkingSetSize;
            }
            return 0;
        }

        float GetCPUUsage() {
            return 0.0f;
        }
    }

    namespace Easing {
        float Linear(float t) { return t; }
        float EaseIn(float t) { return t * t; }
        float EaseOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
        float EaseInOut(float t) { return t * t * (3.0f - 2.0f * t); }
        float EaseInQuad(float t) { return t * t; }
        float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
        float EaseInOutQuad(float t) {
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        }
        float EaseInCubic(float t) { return t * t * t; }
        float EaseOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }
        float EaseInOutCubic(float t) {
            return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        }
        float EaseInQuart(float t) { return t * t * t * t; }
        float EaseOutQuart(float t) { return 1.0f - std::pow(1.0f - t, 4.0f); }
        float EaseInOutQuart(float t) {
            return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
        }
        float EaseInBounce(float t) { return 1.0f - EaseOutBounce(1.0f - t); }
        float EaseOutBounce(float t) {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;

            if (t < 1.0f / d1) {
                return n1 * t * t;
            }
            else if (t < 2.0f / d1) {
                return n1 * (t -= 1.5f / d1) * t + 0.75f;
            }
            else if (t < 2.5f / d1) {
                return n1 * (t -= 2.25f / d1) * t + 0.9375f;
            }
            else {
                return n1 * (t -= 2.625f / d1) * t + 0.984375f;
            }
        }
        float EaseInOutBounce(float t) {
            return t < 0.5f ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) / 2.0f
                : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
        }
        float EaseInElastic(float t) {
            const float c4 = (2.0f * 3.14159f) / 3.0f;
            return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f
                : -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
        }
        float EaseOutElastic(float t) {
            const float c4 = (2.0f * 3.14159f) / 3.0f;
            return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f
                : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
        }
        float EaseInOutElastic(float t) {
            const float c5 = (2.0f * 3.14159f) / 4.5f;
            return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f : t < 0.5f
                ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f
                : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
        }
        float EaseInBack(float t) {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return c3 * t * t * t - c1 * t * t;
        }
        float EaseOutBack(float t) {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1.0f;
            return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
        }
        float EaseInOutBack(float t) {
            return 0.0f;
        }
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    try {
        switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            overlay::d3d_resources.resize_width = (UINT)LOWORD(lParam);
            overlay::d3d_resources.resize_height = (UINT)HIWORD(lParam);
            overlay::d3d_resources.needs_resize = true;
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
    }
    catch (...) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}