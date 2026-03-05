#!/system/bin/sh
MODDIR=${0%/*}

# ZRAM 與記憶體底層優化
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
if [ -n "$A_API" ] && [ "$A_API" -lt 31 ]; then
    exit 0
fi

# 渲染引擎動態分配
SOC_MAKER=$(getprop ro.soc.manufacturer)

if echo "$SOC_MAKER" | grep -qi "MediaTek"; then
    # 天璣 (MediaTek) 專屬
    resetprop -n ro.hwui.renderer skiavk
    resetprop -n debug.hwui.renderer skiavk
    resetprop -n debug.renderengine.backend skiavk
    resetprop -n ro.hwui.use_vulkan true
    resetprop -n debug.renderengine.graphite false
else
    # 常规 SoC 判斷
    if [ "$A_API" -ge 34 ]; then
        # Android 14~16
        resetprop -n ro.hwui.renderer skia
        resetprop -n debug.hwui.renderer skia
        resetprop -n debug.renderengine.backend skia
        resetprop -n ro.hwui.use_vulkan true
        resetprop -n debug.renderengine.graphite true
    elif [ "$A_API" -ge 31 ]; then
        # Android 12~13
        resetprop -n ro.hwui.renderer skiavk
        resetprop -n debug.hwui.renderer skiavk
        resetprop -n debug.renderengine.backend skiavk
        resetprop -n ro.hwui.use_vulkan true
        resetprop -n debug.renderengine.graphite false
    fi
fi

# 設備底層屬性調優
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    # 高通 (Snapdragon) 專屬底層調優
    resetprop ro.vendor.qti.config.zram true
    resetprop ro.vendor.qti.sys.fw.bservice_enable true
    # 限制高通核心控制的最大/最小干預
    resetprop ro.vendor.qti.core.ctl_max_cpu 4
    resetprop ro.vendor.qti.core.ctl_min_cpu 0
    
elif echo "$SOC_MAKER" | grep -qi "MediaTek"; then
    # 聯發科 (MediaTek) 專屬底層調優
    resetprop ro.mtk_perf_fast_start_win 1
    resetprop ro.mtk_perf_response_time 1
    resetprop ro.vendor.mtk_zram_extend 1
    # 針對天璣高階晶片的額外穩定屬性
    resetprop -n ro.vendor.mtk.sensor.support true
    resetprop -n ro.vendor.num_mdm_crashes 0
fi
