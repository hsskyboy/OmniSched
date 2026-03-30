#!/system/bin/sh
MODDIR=${0%/*}

echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
if [ -n "$A_API" ] && [ "$A_API" -lt 31 ]; then
    exit 0
fi

# 從配置檔同步渲染引擎設定
CONFIG_FILE="/data/adb/omnisched/config.json"
FORCE_VULKAN=false

if [ -f "$CONFIG_FILE" ]; then
    if grep -q '"force_vulkan"\s*:\s*true' "$CONFIG_FILE" || grep -q '"force_vulkan":true' "$CONFIG_FILE"; then
        FORCE_VULKAN=true
    fi
fi

if [ "$FORCE_VULKAN" = "true" ]; then
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.vulkan.layers 0
    resetprop -n debug.hwui.skia_tracing_enabled false
    resetprop -n debug.renderengine.vulkan.precompile.enabled true
    resetprop -n debug.renderengine.graphite false
fi