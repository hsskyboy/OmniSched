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
constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_ENTRY = "12";
constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_BALANCED = "16";
constexpr const char* AUTO_TOP_APP_UCLAMP_MIN_PERF = "20";
constexpr const char* DEFAULT_TOP_APP_UCLAMP_MIN = "10";
constexpr const char* LITE_TOP_APP_UCLAMP_MAX = "85";
constexpr const char* DEFAULT_TOP_APP_UCLAMP_MAX = "max";
constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_ENTRY = "45";
constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_BALANCED = "40";
constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_PERF = "32";
constexpr const char* AUTO_BACKGROUND_UCLAMP_MAX_DEFAULT = "35";
constexpr const char* LITE_BACKGROUND_UCLAMP_MAX = "40";
constexpr const char* DEFAULT_BACKGROUND_UCLAMP_MAX = "50";

struct AutoOptimizeProfile {
    std::string foreground_cpus;
    std::string system_background_cpus;
    std::string background_cpus;
    const char* top_app_uclamp_min;
    const char* background_uclamp_max;
};

int get_android_api_level() {
    const std::string api = execute_command("getprop ro.build.version.sdk");
    return std::atoi(api.c_str());
}

bool has_dedicated_big_cluster(const CpuTopology& topology) {
    return !topology.cluster_big.empty() && topology.cluster_big != topology.cluster_mid;
}

bool has_dedicated_mid_cluster(const CpuTopology& topology) {
    return !topology.cluster_mid.empty() && topology.cluster_mid != topology.cluster_big;
}

std::string resolve_system_background_cpus(const CpuTopology& topology) {
    std::string sys_bg = combine_cpus(topology.cluster_little, topology.cluster_mid);
    if (sys_bg.empty()) {
        sys_bg = topology.cluster_little;
    }
    if (sys_bg.empty()) {
        sys_bg = topology.all_cores;
    }
    return sys_bg;
}

const char* pick_auto_top_app_uclamp_min(int cpu_count, bool dedicated_big_cluster) {
    if (cpu_count <= 4) return AUTO_TOP_APP_UCLAMP_MIN_ENTRY;
    if (cpu_count <= 6) return AUTO_TOP_APP_UCLAMP_MIN_BALANCED;
    return dedicated_big_cluster ? AUTO_TOP_APP_UCLAMP_MIN_PERF : AUTO_TOP_APP_UCLAMP_MIN_BALANCED;
}

const char* pick_auto_background_uclamp_max(int cpu_count, bool dedicated_big_cluster) {
    if (cpu_count <= 4) return AUTO_BACKGROUND_UCLAMP_MAX_ENTRY;
    if (cpu_count <= 6) return AUTO_BACKGROUND_UCLAMP_MAX_BALANCED;
    return dedicated_big_cluster ? AUTO_BACKGROUND_UCLAMP_MAX_PERF : AUTO_BACKGROUND_UCLAMP_MAX_DEFAULT;
}

AutoOptimizeProfile build_auto_optimize_profile(const CpuTopology& topology) {
    const int cpu_count = count_cpus_in_cpuset(topology.all_cores);
    const bool dedicated_big_cluster = has_dedicated_big_cluster(topology);
    const bool dedicated_mid_cluster = has_dedicated_mid_cluster(topology);

    std::string system_background_cpus = resolve_system_background_cpus(topology);
    std::string foreground_cpus = topology.all_cores;

    if (cpu_count >= 8 && dedicated_big_cluster && dedicated_mid_cluster) {
        foreground_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
    }
    if (foreground_cpus.empty()) {
        foreground_cpus = topology.all_cores;
    }

    std::string background_cpus = topology.cluster_little.empty()
                                  ? system_background_cpus
                                  : topology.cluster_little;

    return AutoOptimizeProfile{
        foreground_cpus,
        system_background_cpus,
        background_cpus,
        pick_auto_top_app_uclamp_min(cpu_count, dedicated_big_cluster),
        pick_auto_background_uclamp_max(cpu_count, dedicated_big_cluster)
    };
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
    const auto auto_profile = build_auto_optimize_profile(topology);
    const bool auto_optimize = config.auto_optimize;
    const bool lite_mode = config.lite_mode && !auto_optimize;
    const bool background_little_core_only = auto_optimize || config.background_little_core_only;

    apply_memory_optimizations();

    write_node("/dev/cpuset/top-app/cpus", topology.all_cores.c_str());
    
    if (auto_optimize) {
        write_node("/dev/cpuset/foreground/cpus", auto_profile.foreground_cpus.c_str());
    } else if (!topology.cluster_mid.empty() && topology.cluster_mid != topology.cluster_big) {
        const std::string fg_cpus = combine_cpus(topology.cluster_little, topology.cluster_mid);
        write_node("/dev/cpuset/foreground/cpus", fg_cpus.c_str());
    } else {
        write_node("/dev/cpuset/foreground/cpus", topology.all_cores.c_str());
    }
    
    const std::string sys_bg_cpus = auto_profile.system_background_cpus;
    write_node("/dev/cpuset/system-background/cpus", sys_bg_cpus.c_str());

    if (background_little_core_only) {
        write_node("/dev/cpuset/background/cpus", auto_profile.background_cpus.c_str());
    } else {
        write_node("/dev/cpuset/background/cpus", sys_bg_cpus.c_str());
    }

    if (path_exists("/dev/cpuset/top-app/uclamp.min")) {
        if (lite_mode) {
            write_node("/dev/cpuset/top-app/uclamp.max", LITE_TOP_APP_UCLAMP_MAX);
        } else {
            write_node("/dev/cpuset/top-app/uclamp.max", DEFAULT_TOP_APP_UCLAMP_MAX);
        }
        write_node("/dev/cpuset/top-app/uclamp.min",
                   auto_optimize ? auto_profile.top_app_uclamp_min : DEFAULT_TOP_APP_UCLAMP_MIN);

        if (auto_optimize) {
            write_node("/dev/cpuset/background/uclamp.max", auto_profile.background_uclamp_max);
            write_node("/dev/cpuset/system-background/uclamp.max", auto_profile.background_uclamp_max);
        } else if (lite_mode) {
            write_node("/dev/cpuset/background/uclamp.max", LITE_BACKGROUND_UCLAMP_MAX);
            write_node("/dev/cpuset/system-background/uclamp.max", LITE_BACKGROUND_UCLAMP_MAX);
        } else {
            write_node("/dev/cpuset/background/uclamp.max", DEFAULT_BACKGROUND_UCLAMP_MAX);
            write_node("/dev/cpuset/system-background/uclamp.max", DEFAULT_BACKGROUND_UCLAMP_MAX);
        }
    }

    if (!lite_mode && !topology.best_cpu_governor.empty()) {
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

    if (!lite_mode) {
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
