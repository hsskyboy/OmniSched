#pragma once
#include <string>
#include <vector>

enum class VulkanMode { OFF, GLOBAL, PER_APP };
enum class PowerPolicy { BALANCED, PERFORMANCE, POWERSAVE };

struct OmniConfig {
    int poll_interval_seconds = 950;

    VulkanMode vulkan_mode = VulkanMode::OFF;
    std::vector<std::string> vulkan_apps;

    PowerPolicy power_policy = PowerPolicy::BALANCED;
    bool background_little_core_only = true;
    bool auto_optimize = false;
    bool lite_mode = false;

    static const OmniConfig& get();
    static void reload();
};
