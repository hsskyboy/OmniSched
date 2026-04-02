#pragma once
#include <string>

struct OmniConfig {
    int poll_interval_seconds = 950;
    bool background_little_core_only = true;
    bool force_vulkan = false;
    bool lite_mode = false;
    
    static const OmniConfig& get();
    static void reload();
};
