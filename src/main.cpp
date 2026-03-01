#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>

namespace fs = std::filesystem;

// 將自身轉化為後台守護行程
void init_daemon() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 
    if (setsid() < 0) exit(EXIT_FAILURE); 
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); 
    umask(0);
    chdir("/"); 
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

bool write_node(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << value;
        return true;
    }
    return false;
}
std::string read_node(const std::string& path) {
    std::ifstream file(path);
    std::string value;
    if (file.is_open()) {
        std::getline(file, value);
    }
    return value;
}

// 將空格替換為逗號
std::string format_cpuset(std::string cpus) {
    std::replace(cpus.begin(), cpus.end(), ' ', ',');
    if (!cpus.empty() && cpus.back() == ',') {
        cpus.pop_back();
    }
    return cpus;
}

// 核心調度初始化邏輯
void apply_core_optimizations() {
    // 取得裝置所有 CPU 核心 (例如: 0-7)
    std::string all_cores = read_node("/sys/devices/system/cpu/possible");
    if (all_cores.empty()) all_cores = "0-7"; // 容錯機制

    // 讀取 CPU 策略並排序
    std::vector<std::string> policies;
    std::string cpufreq_dir = "/sys/devices/system/cpu/cpufreq/";
    if (fs::exists(cpufreq_dir)) {
        for (const auto& entry : fs::directory_iterator(cpufreq_dir)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("policy") == 0) {
                policies.push_back(entry.path().string());
            }
        }
    }
    std::sort(policies.begin(), policies.end());

    // 劃分大中小核
    std::string cluster_little, cluster_mid, cluster_big;
    if (policies.size() == 3) {
        cluster_little = format_cpuset(read_node(policies[0] + "/affected_cpus"));
        cluster_mid    = format_cpuset(read_node(policies[1] + "/affected_cpus"));
        cluster_big    = format_cpuset(read_node(policies[2] + "/affected_cpus"));
    } else if (policies.size() == 2) {
        cluster_little = format_cpuset(read_node(policies[0] + "/affected_cpus"));
        cluster_big    = format_cpuset(read_node(policies[1] + "/affected_cpus"));
        cluster_mid    = cluster_big;
    } else {
        cluster_little = "0-3"; // 極端後備方案
        cluster_mid    = all_cores;
        cluster_big    = all_cores;
    }

    // 寫入 Cpuset 分配
    write_node("/dev/cpuset/top-app/cpus", all_cores);
    if (!cluster_mid.empty() && cluster_mid != cluster_big) {
        write_node("/dev/cpuset/foreground/cpus", cluster_little + "," + cluster_mid);
    } else {
        write_node("/dev/cpuset/foreground/cpus", all_cores);
    }
    write_node("/dev/cpuset/background/cpus", cluster_little);
    write_node("/dev/cpuset/system-background/cpus", cluster_little);

    // 強制所有 CPU 使用 schedutil 調度器
    std::string cpu_dir = "/sys/devices/system/cpu/";
    if (fs::exists(cpu_dir)) {
        for (const auto& entry : fs::directory_iterator(cpu_dir)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("cpu") == 0 && std::isdigit(filename[3])) {
                write_node(entry.path().string() + "/cpufreq/scaling_governor", "schedutil");
            }
        }
    }

    // CPU & GPU 調度優化
    std::string adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
    if (fs::exists(adreno_path)) {
        write_node(adreno_path, "msm-adreno-tz"); // 高通 Adreno
    } else if (fs::exists("/sys/class/devfreq/")) {
        for (const auto& entry : fs::directory_iterator("/sys/class/devfreq/")) {
            std::string gov_path = entry.path().string() + "/governor";
            std::string avail_govs = read_node(entry.path().string() + "/available_governors");

            // 通用 SoC 判斷
            if (avail_govs.find("mali_ondemand") != std::string::npos) {
                write_node(gov_path, "mali_ondemand");
            } else if (avail_govs.find("simple_ondemand") != std::string::npos) {
                write_node(gov_path, "simple_ondemand");
            }
        }
    }

    std::system("settings put system config.hw_quickpoweron true");
    std::system("settings put system surface_flinger_use_frame_rate_api true");
}

int main() {
    // 啟動守護模式
    init_daemon();

    apply_core_optimizations();
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }

    return 0;
}
