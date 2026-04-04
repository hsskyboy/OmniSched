#include "config.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static OmniConfig current_config;

const OmniConfig& OmniConfig::get() { return current_config; }

void OmniConfig::reload() {
    std::ifstream file("/data/adb/omnisched/config.json");
    if (!file.is_open()) return;

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded()) return;

    current_config = OmniConfig{}; // Reset to defaults
    current_config.poll_interval_seconds = data.value("poll_interval_seconds", 950);

    // 處理電源策略 (支援預設回退
    if (data.contains("power") && data["power"].is_object()) {
        std::string policy = data["power"].value("policy", "balanced");
        if (policy == "performance") current_config.power_policy = PowerPolicy::PERFORMANCE;
        else if (policy == "powersave") current_config.power_policy = PowerPolicy::POWERSAVE;
        else current_config.power_policy = PowerPolicy::BALANCED;
    }
    if (data.contains("render") && data["render"].is_object()) {
        auto renderNode = data["render"];
        // 兼容舊版 boolean 配置
        if (renderNode.contains("force_vulkan") && renderNode["force_vulkan"].is_boolean()) {
            current_config.vulkan_mode = renderNode["force_vulkan"].get<bool>() ? VulkanMode::GLOBAL : VulkanMode::OFF;
        }

        else {
            std::string vMode = renderNode.value("vulkan_mode", "off");
            if (vMode == "global") current_config.vulkan_mode = VulkanMode::GLOBAL;
            else if (vMode == "per_app") current_config.vulkan_mode = VulkanMode::PER_APP;
            else current_config.vulkan_mode = VulkanMode::OFF;
        }
        if (renderNode.contains("vulkan_apps") && renderNode["vulkan_apps"].is_array()) {
            for (const auto& app : renderNode["vulkan_apps"]) {
                if (app.is_string()) current_config.vulkan_apps.push_back(app.get<std::string>());
            }
        }
    }
}
