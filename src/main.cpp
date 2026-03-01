#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>

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

    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }
}

bool write_node(const char* path, const char* value) {
    FILE* file = fopen(path, "w");
    if (file) {
        fputs(value, file);
        fclose(file);
        return true;
    }
    return false;
}

std::string read_node(const char* path) {
    char buffer[256];
    std::string value;
    FILE* file = fopen(path, "r");
    if (file) {
        if (fgets(buffer, sizeof(buffer), file)) {
            value = buffer;
            // 清理尾部換行符
            while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
                value.pop_back();
            }
        }
        fclose(file);
    }
    return value;
}

std::string format_cpuset(std::string cpus) {
    if (cpus.empty()) return cpus;
    std::replace(cpus.begin(), cpus.end(), ' ', ',');
    while (!cpus.empty() && (cpus.back() == ',' || cpus.back() == '\n' || cpus.back() == '\r')) {
        cpus.pop_back();
    }
    return cpus;
}

// 避免出現連續逗號 (如 "0-3,,4-7")
std::string combine_cpus(const std::string& cpus1, const std::string& cpus2) {
    if (cpus1.empty()) return cpus2;
    if (cpus2.empty()) return cpus1;
    return cpus1 + "," + cpus2;
}

bool path_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

// 核心調度初始化邏輯
void apply_core_optimizations() {
    std::string all_cores = read_node("/sys/devices/system/cpu/possible");
    // 取得裝置所有 CPU 核心 (例如: 0-7);
    if (all_cores.empty()) all_cores = "0-7"; 

    // 讀取 CPU 策略並排序
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

    // 劃分大中小核
    std::string cluster_little = "0-3"; // 極端後備方案
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

    // 寫入 Cpuset 分配
    write_node("/dev/cpuset/top-app/cpus", all_cores.c_str());
    if (!cluster_mid.empty() && cluster_mid != cluster_big) {
        write_node("/dev/cpuset/foreground/cpus", combine_cpus(cluster_little, cluster_mid).c_str());
    } else {
        write_node("/dev/cpuset/foreground/cpus", all_cores.c_str());
    }
    write_node("/dev/cpuset/background/cpus", cluster_little.c_str());
    write_node("/dev/cpuset/system-background/cpus", cluster_little.c_str());

    // 強制所有 CPU 使用 schedutil 調度器
    const char* cpu_dir_path = "/sys/devices/system/cpu/";
    DIR* cpu_dir = opendir(cpu_dir_path);
    if (cpu_dir) {
        struct dirent* entry;
        while ((entry = readdir(cpu_dir)) != nullptr) {
            if (strncmp(entry->d_name, "cpu", 3) == 0 && isdigit(entry->d_name[3])) {
                std::string gov_path = std::string(cpu_dir_path) + entry->d_name + "/cpufreq/scaling_governor";
                write_node(gov_path.c_str(), "schedutil");
            }
        }
        closedir(cpu_dir);
    }

    // GPU 調度優化
    const char* adreno_path = "/sys/class/kgsl/kgsl-3d0/devfreq/governor";
    if (path_exists(adreno_path)) {
        write_node(adreno_path, "msm-adreno-tz"); // 高通 Adreno
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

    // 執行系統屬性設定
    std::system("settings put system config.hw_quickpoweron true");
    std::system("settings put system surface_flinger_use_frame_rate_api true");
}

int main() {
    // 啟動守護模式
    init_daemon();

    while (true) {
        apply_core_optimizations();
        sleep(1800); // 30min
    }

    return 0;
}

