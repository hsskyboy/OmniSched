#!/system/bin/sh
MODDIR=${0%/*}

# ZRAM 與記憶體底層優化
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
A_API=$(getprop ro.build.version.sdk)
if [ -n "$A_API" ] && [ "$A_API" -lt 31 ]; then
    exit 0
fi

# 動態設備嗅探與屬性注入
SOC_MAKER=$(getprop ro.soc.manufacturer)

if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    # 高通 (Snapdragon) 專屬底層調優
    resetprop ro.vendor.qti.config.zram true
    resetprop ro.vendor.qti.sys.fw.bservice_enable true
    # 限制高通核心控制的最大/最小干預，防止與我們的腳本衝突
    resetprop ro.vendor.qti.core.ctl_max_cpu 4
    resetprop ro.vendor.qti.core.ctl_min_cpu 2
    
elif echo "$SOC_MAKER" | grep -qi "MediaTek"; then
    # 聯發科 (MediaTek) 專屬底層調優
    resetprop ro.mtk_perf_fast_start_win 1
    resetprop ro.mtk_perf_response_time 1
    resetprop ro.vendor.mtk_zram_extend 1
fi
