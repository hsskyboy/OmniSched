#pragma once

#include <functional>
#include <vector>
#include <string>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include "config.h"
#include "cpu_topology.h"
#include "utils.h"

class SchedEventFlow {
private:
    int epoll_fd = -1;
    int inotify_fd = -1;
    std::vector<int> watch_descriptors;
    static constexpr int AUTO_TIMEOUT_SECONDS = 600;

    int get_auto_timeout_seconds() const {
        const auto& topology = CpuTopology::get();
        const int cpu_count = count_cpus_in_cpuset(topology.all_cores);
        const bool dedicated_big_cluster =
            !topology.cluster_big.empty() && topology.cluster_big != topology.cluster_mid;

        if (cpu_count <= 4) return 720;
        if (cpu_count >= 8 && dedicated_big_cluster) return 480;
        return AUTO_TIMEOUT_SECONDS;
    }

    int get_timeout_ms() const {
        OmniConfig::reload();

        int timeout_seconds = OmniConfig::get().poll_interval_seconds;
        if (OmniConfig::get().auto_optimize) {
            timeout_seconds = get_auto_timeout_seconds();
        }

        timeout_seconds = std::clamp(timeout_seconds, 300, 3600);
        return timeout_seconds * 1000;
    }

public:
    SchedEventFlow() {
        epoll_fd = epoll_create1(0);
        inotify_fd = inotify_init1(IN_NONBLOCK);
        
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = inotify_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev);
        
        const std::vector<std::string> target_nodes = {
            "/dev/cpuset/top-app/cpus",
            "/dev/cpuset/background/cpus",
            "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor",
            "/data/adb/omnisched/config.json"
        };
        
        for (const auto& node : target_nodes) {
            int wd = inotify_add_watch(inotify_fd, node.c_str(), IN_MODIFY | IN_ATTRIB);
            if (wd >= 0) watch_descriptors.push_back(wd);
        }
    }

    ~SchedEventFlow() {
        for (int wd : watch_descriptors) {
            inotify_rm_watch(inotify_fd, wd);
        }
        if (inotify_fd >= 0) close(inotify_fd);
        if (epoll_fd >= 0) close(epoll_fd);
    }

    void collect(const std::function<void()>& action) {
        epoll_event events[5];
        char buffer[1024];

        while (true) {
            int n = epoll_wait(epoll_fd, events, 5, get_timeout_ms());
            
            if (n == 0) {
                // 定期重新应用调度器设置。
                action();
            } else if (n > 0) {
                while (read(inotify_fd, buffer, sizeof(buffer)) > 0) {}
                
                action();

                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }
    }
};
