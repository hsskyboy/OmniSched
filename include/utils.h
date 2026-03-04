#pragma once

#include <string>

bool write_node(const char* path, const char* value);
std::string read_node(const char* path);
std::string format_cpuset(std::string cpus);
std::string combine_cpus(const std::string& cpus1, const std::string& cpus2);
std::string execute_command(const char* cmd);
bool path_exists(const char* path);