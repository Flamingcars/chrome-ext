#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <type_traits>
#include <chrono>
#include "functions.h"
#include "../overlay/overlay.h"
#include "../../globals.h"
#include "wallcheck/wallcheck.h"
#include "cache/visual2ndcachefile.h"

class WorkerPool {
public:
    WorkerPool(size_t threads = 2) : stop(false), active_tasks(0) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait_for(lock, std::chrono::milliseconds(100), [this] {
                            return this->stop || !this->tasks.empty();
                            });

                        if (this->stop && this->tasks.empty())
                            return;

                        if (this->tasks.empty()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            continue;
                        }

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        active_tasks++;
                    }

                    task();
                    active_tasks--;
                }
                });
        }
    }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) return res;

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    void enqueue_low_priority(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop) return;

        low_priority_tasks.emplace(std::move(task));
        condition.notify_one();
    }

    ~WorkerPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers)
            worker.join();
    }

    size_t thread_count() const { return workers.size(); }
    size_t task_count() const {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size() + low_priority_tasks.size();
    }
    size_t busy_threads() const { return active_tasks.load(); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::queue<std::function<void()>> low_priority_tasks;
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> active_tasks;
};

namespace cache {
    void hook_maincache(bool firstreceived);
    void hook_cache(bool firstreceived);
    void hook_espcache(bool firstreceived);
    void hook_chamscache(bool firstreceived);
}

namespace runs {
    void keybinds(bool firstreceived);
    void runs_speed(bool firstreceived);
    void runs_jumppower(bool firstreceived);
    void runs_hipheight(bool firstreceived);
    void runs_flight(bool firstreceived);
    void runs_noclip(bool firstreceived);
    void runs_freecam(bool firstreceived);
    void runs_lightingeffect(bool firstrecieved);
    void runs_world(bool firstreceived);

    void runs_voidhide(bool firstreceived);
    void desync(bool firstrecieved);
    void runs_aimbot(bool firstreceived);
    void runs_triggerbot(bool firstreceived);
    void runs_silentcache(bool firstreceived);
    void runs_silentaimbot(bool firstreceived);
    void runs_wallcheck(bool firstreceived);
    void run_esp(bool firstreceived);
    void runs_bullet_tracer_cache(bool firstreceived);
    void runs_player_trail_cache(bool firstrecieved);
    void runs_resource_esp_cache(bool should_run);
}

namespace runs {
   inline void runs_china_hat_cache(bool firstrecieved) {
        while (firstrecieved) {
            if (!firstrecieved) break;

            update_china_hat_cache();
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); 
        }
    }

   inline  void runs_skeleton_cache(bool firstrecieved) {
        while (firstrecieved) {
            if (!firstrecieved) break;

            update_skeleton_cache();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}







namespace hook {
    inline WorkerPool pool1(2);
    inline bool init() {
        if (hummer::heartbeated) {
            std::thread main_cache(cache::hook_cache, hummer::heartbeated);
            main_cache.detach();
            std::thread mainz_cache(cache::hook_maincache, hummer::heartbeated);
            mainz_cache.detach();
            std::thread esp_cache(cache::hook_espcache, hummer::heartbeated);
            esp_cache.detach();
            std::thread chams_cache(cache::hook_chamscache, hummer::heartbeated);
            chams_cache.detach();
            std::thread aimbotthead(runs::runs_aimbot, hummer::heartbeated);
            aimbotthead.detach();
            std::thread speedthread(runs::runs_speed, hummer::heartbeated);
            speedthread.detach();
            std::thread jumpthread(runs::runs_jumppower, hummer::heartbeated);
            jumpthread.detach();
            std::thread flightthread(runs::runs_flight, hummer::heartbeated);
            flightthread.detach();
            std::thread freecamthread(runs::runs_freecam, hummer::heartbeated);
            freecamthread.detach();
            std::thread worldthread(runs::runs_world, hummer::heartbeated);
            worldthread.detach();
            std::thread silentaimneededforit(roblox::cached_input_objectzz);
            silentaimneededforit.detach();
            std::thread silentthread(runs::runs_silentaimbot, hummer::heartbeated);
            silentthread.detach();
            std::thread silentcache(runs::runs_silentcache, hummer::heartbeated);
            silentcache.detach();
            std::thread bulletracers(runs::runs_bullet_tracer_cache, hummer::heartbeated);
            bulletracers.detach();
            std::thread trail(runs::runs_player_trail_cache, hummer::heartbeated);
            trail.detach();
            std::thread chinahat_cache_thread(runs::runs_china_hat_cache, hummer::heartbeated);
            chinahat_cache_thread.detach();
            std::thread skeleton_cache_thread(runs::runs_skeleton_cache, hummer::heartbeated);
            skeleton_cache_thread.detach();
            std::thread resource_esp_cache_thread(runs::runs_resource_esp_cache, hummer::heartbeated);
            resource_esp_cache_thread.detach();
            std::thread lighting(runs::runs_lightingeffect, hummer::heartbeated);
            lighting.detach();
            std::thread desync(runs::desync, hummer::heartbeated);
            desync.detach();
           // wallcheck::initialize();
            if (!overlay::InitializeAndRun()) {
                return false;
            }

        }
        return true;
    }

    inline void cleanup() {
        overlay::Shutdown();
    }
}