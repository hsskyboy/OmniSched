#include "core_sched.h"
#include "cpu_topology.h"
#include "config.h"
#include "root_adapter.h"
#include "utils.h"
#include <vector>
#include <unistd.h>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <dirent.h>

namespace {
int get_android_api_level() {
    const std::string api = execute_command("getprop ro.build.version.sdk");
    return std::atoi(api.c_str());
}

bool is_mtk_soc() {
    std::string soc = execute_command("getprop ro.soc.manufacturer");
    std::transform(soc.begin(), soc.end(), soc.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return soc.find("mediatek") != std::string::npos || soc.find("mtk") != std::string::npos;
}

void apply_render_engine_optimizations(const OmniConfig& config, const IRootAdapter& root) {
    if (!config.force_vulkan) return;

    const int api = get_android_api_level();
    const bool mtk = is_mtk_soc();

    if (mtk || api < 34) {
        root.set_system_prop("ro.hwui.renderer", "skiavk");
        root.set_system_prop("debug.hwui.renderer", "skiavk");
        root.set_system_prop("debug.renderengine.backend", "skiavk");
        root.set_system_prop("ro.hwui.use_vulkan", "true");
        root.set_system_prop("debug.renderengine.graphite", "false");
    } else {
        // Android 14+
        root.set_system_prop("ro.hwui.renderer", "skia");
        root.set_system_prop("debug.hwui.renderer", "skia");
        root.set_system_prop("debug.renderengine.backend", "skiavk");
        root.set_system_prop("ro.hwui.use_vulkan", "true");
        root.set_system_prop("debug.renderengine.graphite", "true");
    }

    // Vulkan 強化
    root.set_system_prop("debug.vulkan.layers", "");
    root.set_system_prop("debug.hwui.skia_tracing_enabled", "false");
    root.set_system_prop("debug.renderengine.vulkan.precompile.enabled", "true");
}
} // namespace

void init_daemon() {
    // 切換目錄至 "/" 並將標準輸出導向 /dev/null
    if (daemon(0, 0) < 0) {
        exit(EXIT_FAILURE);
    }
}

void apply_memory_optimizations() {
    // 啟用 Multi-Gen LRU (Android 14+ 預設啟用)
    if (path_exists("/sys/kernel/mm/lru_gen/enabled")) {
        write_node("/sys/kernel/mm/lru_gen/enabled", "7"); 
    }
    // 最佳化 ZRAM 與 Page Swap 行為
    if (path_exists("/proc/sys/vm/swappiness")) {
        write_node("/proc/sys/vm/swappiness", "100");
    }
    // 降低記憶體分配延遲
    if (path_exists("/proc/sys/vm/watermark_scale_factor")) {
        write_node("/proc/sys/vm/watermark_scale_factor", "20"); 
    }
}

void apply_core_optimizations() {
    OmniConfig::reload();

    const auto& topology = CpuTopology::get(); 
    const auto& config = OmniConfig::get();
    const auto& root = RootEnvironment::get_adapter();

    apply_memory_optimizations();

    write_node("/dev/cpuset/top-app/cpus", topology.all_cores.c_str());
    
    if (!topology.cluster_mid.empty() && topology.cluster_mid != topology.cluster_big) {
        const std::string fg_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
        write_node("/dev/cpuset/foreground/cpus", fg_cpus.c_str());
    } else {
        write_node("/dev/cpuset/foreground/cpus", topology.all_cores.c_str());
    }
    
    const std::string sys_bg_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
    write_node("/dev/cpuset/system-background/cpus", sys_bg_cpus.c_str());

    if (config.background_little_core_only) {
        write_node("/dev/cpuset/background/cpus", topology.cluster_little.c_str());
    } else {
        write_node("/dev/cpuset/background/cpus", sys_bg_cpus.c_str());
    }

    if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
        if (config.lite_mode) {
            write_node("/dev/cpuset/top-app/uclamp.max", "85");
        } else {
            write_node("/dev/cpuset/top-app/uclamp.max", "max");
        }
        write_node("/dev/cpuset/top-app/uclamp.min", "10");

        if (config.lite_mode) {
            write_node("/dev/cpuset/background/uclamp.max", "40");
            write_node("/dev/cpuset/system-background/uclamp.max", "40");
        } else {
            write_node("/dev/cpuset/background/uclamp.max", "50");
            write_node("/dev/cpuset/system-background/uclamp.max", "50");
        }
    }

    if (!config.lite_mode && !topology.best_cpu_governor.empty()) {
        if (DIR* cpu_dir = opendir("/sys/devices/system/cpu/"); cpu_dir) {
            struct dirent* entry;
            while ((entry = readdir(cpu_dir)) != nullptr) {
                if (strncmp(entry->d_name, "cpu", 3) == 0 &&
                    std::isdigit(static_cast<unsigned char>(entry->d_name[3]))) {
                    const std::string gov_path = std::string("/sys/devices/system/cpu/") + entry->d_name + "/cpufreq/scaling_governor";
                    write_node(gov_path.c_str(), topology.best_cpu_governor.c_str());
                }
            }
            closedir(cpu_dir);
        }
    }

    if (!config.lite_mode) {
        const char* adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
        if (path_exists(adreno_path)) {
            write_node(adreno_path, "msm-adreno-tz");
        } else if (DIR* devfreq_dir = opendir("/sys/class/devfreq/"); devfreq_dir) {
            struct dirent* entry;
            while ((entry = readdir(devfreq_dir)) != nullptr) {
                if (entry->d_name[0] == '.') continue;

                const std::string gov_path = std::string("/sys/class/devfreq/") + entry->d_name + "/governor";
                const std::string avail_path = std::string("/sys/class/devfreq/") + entry->d_name + "/available_governors";
                const std::string avail_govs = read_node(avail_path.c_str());

                if (avail_govs.find("mali_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "mali_ondemand");
                } else if (avail_govs.find("simple_ondemand") != std::string::npos) {
                    write_node(gov_path.c_str(), "simple_ondemand");
                }
            }
            closedir(devfreq_dir);
        }
    }

    apply_render_engine_optimizations(config, root);
}
