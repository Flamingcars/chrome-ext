#include "../modules.h"
#include "../../../globals.h"
#include "../../../backend/usermode/usermode.h"
#include "../../../includes.h"

void runs::runs_voidhide(bool firstrecieved) {
	while (firstrecieved) {
		if (firstrecieved) {
            Sleep(50);
		}
		else
			return;
	}
}


void write_bandwidth(int value) {
    write<int>(base_address + offsets::PhysicsSenderMaxBandwidthBps, value);
    write<int>(base_address + offsets::PhysicsSenderMaxBandwidthBpsScaling, value);
}
void runs::desync(bool firstrecieved) {
    static bool was_enabled = false;
    static bool cycle_active = false;
    static std::chrono::steady_clock::time_point last_change_time;
    static bool is_currently_lagging = false;

    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(currentThread, 1);

    static LARGE_INTEGER frequency;
    static LARGE_INTEGER lastTime;
    static bool timeInitialized = false;

    if (!timeInitialized) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);
        timeBeginPeriod(1);
        timeInitialized = true;
    }

    while (firstrecieved) {
        if (firstrecieved) {
            globals::misc::desyncbind.update();
            bool is_now_pressed = globals::misc::desyncbind.enabled;

            bool normal_desync_active = globals::misc::desync_enabled && !globals::misc::desync_interval_enabled;
            bool interval_desync_active = globals::misc::desync_enabled && globals::misc::desync_interval_enabled;

            bool is_desynced_now = false;

            if (normal_desync_active) {
                if (!was_enabled && is_now_pressed) {
                    write_bandwidth(0);
                    is_desynced_now = true;
                }
                else if (was_enabled && is_now_pressed) {
                    is_desynced_now = true;
                }

                if (was_enabled && !is_now_pressed) {
                    write_bandwidth(1000);
                    is_desynced_now = false;
                }
            }
            else if (interval_desync_active) {
                if (!was_enabled && is_now_pressed) {
                    write_bandwidth(0);
                    cycle_active = true;
                    is_currently_lagging = true;
                    last_change_time = std::chrono::steady_clock::now();
                    is_desynced_now = true;

                    globals::misc::desync_interval_is_lagging = true;
                    int hold_time_lag = (globals::misc::desync_pulse_interval_ms * 15 > 50) ? globals::misc::desync_pulse_interval_ms * 15 : 50;
                    globals::misc::desync_interval_total_time = hold_time_lag / 1000.0f;
                    globals::misc::desync_interval_time_left = globals::misc::desync_interval_total_time;
                }
                else if (is_now_pressed && cycle_active) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change_time).count();

                    if (is_currently_lagging) {
                        int hold_time_lag = (globals::misc::desync_pulse_interval_ms * 15 > 50) ? globals::misc::desync_pulse_interval_ms * 15 : 50;
                        globals::misc::desync_interval_is_lagging = true;
                        globals::misc::desync_interval_total_time = hold_time_lag / 1000.0f;
                        globals::misc::desync_interval_time_left = fmax(0.0f, (hold_time_lag - elapsed_ms) / 1000.0f);
                        is_desynced_now = true;

                        if (elapsed_ms >= hold_time_lag) {
                            write_bandwidth(1000);
                            is_currently_lagging = false;
                            last_change_time = now;
                            is_desynced_now = false;

                            globals::misc::desync_interval_is_lagging = false;
                            int hold_time_normal = (globals::misc::desync_pulse_interval_ms * 20 > 100) ? globals::misc::desync_pulse_interval_ms * 20 : 100;
                            globals::misc::desync_interval_total_time = hold_time_normal / 1000.0f;
                            globals::misc::desync_interval_time_left = hold_time_normal / 1000.0f;
                        }
                    }
                    else {
                        int hold_time_normal = (globals::misc::desync_pulse_interval_ms * 20 > 100) ? globals::misc::desync_pulse_interval_ms * 20 : 100;
                        globals::misc::desync_interval_is_lagging = false;
                        globals::misc::desync_interval_total_time = hold_time_normal / 1000.0f;
                        globals::misc::desync_interval_time_left = fmax(0.0f, (hold_time_normal - elapsed_ms) / 1000.0f);
                        is_desynced_now = false;

                        if (elapsed_ms >= hold_time_normal) {
                            write_bandwidth(0);
                            is_currently_lagging = true;
                            last_change_time = now;
                            is_desynced_now = true;

                            globals::misc::desync_interval_is_lagging = true;
                            int hold_time_lag = (globals::misc::desync_pulse_interval_ms * 15 > 50) ? globals::misc::desync_pulse_interval_ms * 15 : 50;
                            globals::misc::desync_interval_total_time = hold_time_lag / 1000.0f;
                            globals::misc::desync_interval_time_left = hold_time_lag / 1000.0f;
                        }
                    }
                }

                if (was_enabled && !is_now_pressed) {
                    write_bandwidth(1000);
                    cycle_active = false;
                    is_currently_lagging = false;
                    is_desynced_now = false;

                    globals::misc::desync_interval_is_lagging = false;
                    globals::misc::desync_interval_time_left = 0.0f;
                    globals::misc::desync_interval_total_time = 0.0f;
                }
            }
            else {
                write_bandwidth(1000);
                cycle_active = false;
                is_currently_lagging = false;
                is_desynced_now = false;

                globals::misc::desync_interval_is_lagging = false;
                globals::misc::desync_interval_time_left = 0.0f;
                globals::misc::desync_interval_total_time = 0.0f;
            }

            globals::currentlydesynced = is_desynced_now;

            was_enabled = is_now_pressed;

            const double targetFrameTime = 1.0 / 60;

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

            SwitchToThread();
        }
        else
            return;
    }
}


