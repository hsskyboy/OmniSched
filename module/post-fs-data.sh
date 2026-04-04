#!/system/bin/sh
MODDIR=${0%/*}

CONFIG_FILE="/data/adb/omnisched/config.json"

# 基礎 VM 優化
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
[ -z "$A_API" ] && exit 0
[ "$A_API" -lt 31 ] && exit 0

# Android 14+ LMK 微調
if [ "$A_API" -ge 34 ]; then
    resetprop -n ro.lmk.use_minfree_levels true
    resetprop -n ro.lmk.enhance_batch_kill false
    resetprop -n ro.lmk.swap_util_max 90
fi

FORCE_VULKAN=true
if [ -f "$CONFIG_FILE" ]; then
    FORCE_VULKAN_VALUE=$(grep -o '"force_vulkan"[[:space:]]*:[[:space:]]*\(true\|false\)' "$CONFIG_FILE" 2>/dev/null \
        | tail -n1 \
        | sed 's/.*:[[:space:]]*//')
    [ "$FORCE_VULKAN_VALUE" = "false" ] && FORCE_VULKAN=false
fi

# 渲染引擎行為
if [ "$FORCE_VULKAN" = "false" ]; then
    resetprop -n ro.hwui.renderer skiagl
    resetprop -n debug.hwui.renderer skiagl
    resetprop -n debug.renderengine.backend skiagl
    resetprop -n ro.hwui.use_vulkan false
    resetprop -n debug.renderengine.graphite true
    resetprop -n debug.renderengine.vulkan.precompile.enabled false
else
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.renderengine.graphite false
    resetprop -n debug.renderengine.vulkan.precompile.enabled false
    resetprop -n debug.hwui.use_buffer_age true
    resetprop -n debug.hwui.disable_scissor_opt false
fi

SOC=$(getprop ro.soc.manufacturer)
if [ -d "/sys/class/kgsl" ] || echo "$SOC" | grep -qi "qualcomm"; then
    resetprop -n ro.vendor.qti.config.zram true
    resetprop -n ro.vendor.qti.sys.fw.bservice_enable true
    resetprop -n ro.vendor.qti.core.ctl_max_cpu 6
    resetprop -n ro.vendor.qti.core.ctl_min_cpu 0

elif echo "$SOC" | grep -qi "mediatek\|mtk"; then
    # 聯發科 (MediaTek) 專屬底層調優
    resetprop -n ro.mtk_perf_fast_start_win 1
    resetprop -n ro.mtk_perf_response_time 1
    resetprop -n ro.vendor.mtk_zram_extend 1
    resetprop -n ro.vendor.mtk.sensor.support true
    resetprop -n ro.vendor.num_mdm_crashes 0
fi
