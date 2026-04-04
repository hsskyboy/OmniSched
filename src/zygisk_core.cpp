#include "config.h"
#include "project_paths.h"
#include "zygisk/api.hpp"

#include <sys/stat.h>
#include <sys/system_properties.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

    std::string jstring_to_string(JNIEnv* env, jstring value) {
        if (env == nullptr || value == nullptr) return {};
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars == nullptr) return {};

        std::string result(chars);
        env->ReleaseStringUTFChars(value, chars);
        return result;
    }

    struct RenderConfigSnapshot {
        VulkanMode vulkan_mode = VulkanMode::OFF;
        std::unordered_set<std::string> vulkan_apps;
    };

    RenderConfigSnapshot load_render_config() {
        static RenderConfigSnapshot cached_snapshot;
        static time_t last_modified_time = 0;
        static std::mutex cache_mutex;

        std::lock_guard<std::mutex> lock(cache_mutex);

        std::string active_path;
        struct stat st;
        bool file_found = false;

        for (const auto& config_path : omnisched::config_path_candidates()) {
            if (stat(config_path.c_str(), &st) == 0) {
                active_path = config_path;
                file_found = true;
                break;
            }
        }

        if (!file_found) return cached_snapshot;

        if (st.st_mtime == last_modified_time && last_modified_time != 0) {
            return cached_snapshot;
        }

        std::ifstream file(active_path);
        if (!file.is_open()) return cached_snapshot;

        json data = json::parse(file, nullptr, false);
        if (data.is_discarded() || !data.contains("render") || !data["render"].is_object()) {
            return cached_snapshot;
        }

        RenderConfigSnapshot new_snapshot;
        const auto& render = data["render"];
        const std::string mode = render.value("vulkan_mode", "off");

        if (mode == "global") new_snapshot.vulkan_mode = VulkanMode::GLOBAL;
        else if (mode == "per_app") new_snapshot.vulkan_mode = VulkanMode::PER_APP;

        if (render.contains("vulkan_apps") && render["vulkan_apps"].is_array()) {
            for (const auto& app : render["vulkan_apps"]) {
                if (app.is_string()) new_snapshot.vulkan_apps.emplace(app.get<std::string>());
            }
        }

        cached_snapshot = std::move(new_snapshot);
        last_modified_time = st.st_mtime;

        return cached_snapshot;
    }

    static int (*old_system_property_get)(const char*, char*);

    static int new_system_property_get(const char* name, char* value) {
        if (name == nullptr || value == nullptr) {
            return old_system_property_get ? old_system_property_get(name, value) : 0;
        }

        std::string prop_name(name);
        if (prop_name == "ro.hwui.renderer" ||
                prop_name == "debug.hwui.renderer" ||
                prop_name == "debug.renderengine.backend") {
            strcpy(value, "skiavk");
            return strlen(value);
        }
        if (prop_name == "ro.hwui.use_vulkan") {
            strcpy(value, "true");
            return strlen(value);
        }
        if (prop_name == "debug.renderengine.graphite") {
            strcpy(value, "false");
            return strlen(value);
        }

        return old_system_property_get ? old_system_property_get(name, value) : 0;
    }

    class OmniSchedZygiskModule final : public zygisk::ModuleBase {
    public:
        void onLoad(zygisk::Api* api, JNIEnv* env) override {
            api_ = api;
            env_ = env;

            load_render_config();
        }

        void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
            if (api_ == nullptr || env_ == nullptr || args == nullptr) return;

            const auto snapshot = load_render_config();

            if (snapshot.vulkan_mode != VulkanMode::PER_APP || snapshot.vulkan_apps.empty()) {
                api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                return;
            }

            const std::string process_name = jstring_to_string(env_, args->nice_name);

            if (!process_name.empty() && snapshot.vulkan_apps.contains(process_name)) {
                apply_per_app_vulkan_env();
            } else {
                api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            }
        }

    private:
        zygisk::Api* api_ = nullptr;
        JNIEnv* env_ = nullptr;

        void apply_per_app_vulkan_env() {
            setenv("debug.hwui.renderer", "skiavk", 1);
            setenv("debug.renderengine.backend", "skiavk", 1);
            setenv("ro.hwui.use_vulkan", "true", 1);
            setenv("debug.renderengine.graphite", "false", 1);
            setenv("OMNISCHED_VULKAN_INJECTED", "1", 1);

            api_->pltHookRegister(0, 0, "__system_property_get",
                    (void*)new_system_property_get,
                    (void**)&old_system_property_get);

            api_->pltHookCommit();
        }
    };

}  // namespace

REGISTER_ZYGISK_MODULE(OmniSchedZygiskModule)