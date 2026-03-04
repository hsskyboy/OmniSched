#include "core_sched.h"
#include "utils.h"
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>
#include <cstring>

void init_daemon() {
    // 切換目錄至 "/" 並將標準輸出導向 /dev/null
    if (daemon(0, 0) < 0) {
        exit(EXIT_FAILURE);
    }
}

std::string get_best_cpu_governor(const std::string& avail_govs, bool is_mtk) {
    if (is_mtk) {
        if (avail_govs.find("sugov_ext") != std::string::npos) return "sugov_ext";
        if (avail_govs.find("schedutil") != std::string::npos) return "schedutil";
    } else {
        if (avail_govs.find("walt") != std::string::npos) return "walt";
        if (avail_govs.find("uag") != std::string::npos) return "uag";
        if (avail_govs.find("schedutil") != std::string::npos) return "schedutil";
    }
    return ""; 
}

void apply_core_optimizations() {
    std::string platform = execute_command("getprop ro.board.platform");
    bool is_mtk = (platform.find("mt") != std::string::npos);

    std::string all_cores = read_node("/sys/devices/system/cpu/possible");
    if (all_cores.empty()) all_cores = "0-7"; 

    std::vector<std::string> policies;
    const char* cpufreq_dir_path = "/sys/devices/system/cpu/cpufreq/";
    
    DIR* dir = opendir(cpufreq_dir_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strncmp(entry->d_name, "policy", 6) == 0) {
                policies.push_back(std::string(cpufreq_dir_path) + entry->d_name);
            }
        }
        closedir(dir);
    }

    std::sort(policies.begin(), policies.end());

    std::string cluster_little = "0-3"; 
    std::string cluster_mid = all_cores;
    std::string cluster_big = all_cores;

    if (policies.size() >= 3) {
        cluster_little = format_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
        cluster_mid    = format_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
        cluster_big    = format_cpuset(read_node((policies[2] + "/affected_cpus").c_str()));
    } else if (policies.size() == 2) {
        cluster_little = format_cpuset(read_node((policies[0] + "/affected_cpus").c_str()));
        cluster_big    = format_cpuset(read_node((policies[1] + "/affected_cpus").c_str()));
        cluster_mid    = cluster_big;
    }

    write_node("/dev/cpuset/top-app/cpus", all_cores.c_str());
    if (!cluster_mid.empty() && cluster_mid != cluster_big) {
        write_node("/dev/cpuset/foreground/cpus", combine_cpus(cluster_little, cluster_mid).c_str());
    } else {
        write_node("/dev/cpuset/foreground/cpus", all_cores.c_str());
    }
    write_node("/dev/cpuset/background/cpus", cluster_little.c_str());
    write_node("/dev/cpuset/system-background/cpus", cluster_little.c_str());

    if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
        write_node("/dev/cpuset/top-app/uclamp.min", "5");
    }

    const char* cpu_dir_path = "/sys/devices/system/cpu/";
    DIR* cpu_dir = opendir(cpu_dir_path);
    if (cpu_dir) {
        struct dirent* entry;
        while ((entry = readdir(cpu_dir)) != nullptr) {
            if (strncmp(entry->d_name, "cpu", 3) == 0 && isdigit(entry->d_name[3])) {
                std::string base_path = std::string(cpu_dir_path) + entry->d_name + "/cpufreq/";
                std::string avail_govs = read_node((base_path + "scaling_available_governors").c_str());
                std::string best_gov = get_best_cpu_governor(avail_govs, is_mtk);
                
                if (!best_gov.empty()) {
                    write_node((base_path + "scaling_governor").c_str(), best_gov.c_str());
                }
            }
        }
        closedir(cpu_dir);
    }

    const char* adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
    if (path_exists(adreno_path)) {
        write_node(adreno_path, "msm-adreno-tz");
    } else {
        const char* devfreq_path = "/sys/class/devfreq/";
        DIR* devfreq_dir = opendir(devfreq_path);
        if (devfreq_dir) {
            struct dirent* entry;
            while ((entry = readdir(devfreq_dir)) != nullptr) {
                if (entry->d_name[0] == '.') continue;
                
                std::string gov_path = std::string(devfreq_path) + entry->d_name + "/governor";
                std::string avail_path = std::string(devfreq_path) + entry->d_name + "/available_governors";
                std::string avail_govs = read_node(avail_path.c_str());

                if (avail_govs.find("mali_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "mali_ondemand");
                } else if (avail_govs.find("simple_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "simple_ondemand");
                }
            }
            closedir(devfreq_dir);
        }
    }

    std::system("settings put system config.hw_quickpoweron true");
    std::system("settings put system surface_flinger_use_frame_rate_api true");
}
