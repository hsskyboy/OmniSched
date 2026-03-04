#include "utils.h"
#include <cstdio>
#include <sys/stat.h>
#include <algorithm>
#include <stdexcept>

bool write_node(const char* path, const char* value) {
    std::string current_value = read_node(path);
    if (current_value == value) {
        return true;
    }

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

std::string combine_cpus(const std::string& cpus1, const std::string& cpus2) {
    if (cpus1.empty()) return cpus2;
    if (cpus2.empty()) return cpus1;
    return cpus1 + "," + cpus2;
}

bool path_exists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

std::string execute_command(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return result;
    
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}
