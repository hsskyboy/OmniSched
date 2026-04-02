#include "config.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static OmniConfig current_config;

const OmniConfig& OmniConfig::get() {
    return current_config;
}

void OmniConfig::reload() {
    std::ifstream file("/data/adb/omnisched/config.json");
    if (!file.is_open()) return;

    json data = json::parse(file, nullptr, false);
    
    if (data.is_discarded()) {
        return;
    }

    current_config.poll_interval_seconds = data.value("poll_interval_seconds", 950);
    
    if (data.contains("cpuset") && data["cpuset"].is_object()) {
        current_config.background_little_core_only = 
            data["cpuset"].value("background_little_core_only", true);
    }
    
    if (data.contains("render") && data["render"].is_object()) {
        current_config.force_vulkan = 
            data["render"].value("force_vulkan", false);
    }

    if (data.contains("performance") && data["performance"].is_object()) {
        current_config.lite_mode =
            data["performance"].value("lite_mode", false);
    }
}
