#pragma once

#include <string>

void init_daemon();
std::string get_best_cpu_governor(const std::string& avail_govs);
void apply_core_optimizations();