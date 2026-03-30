#!/system/bin/sh
MODDIR=${0%/*}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

A_API=$(getprop ro.build.version.sdk)
SOC_MAKER=$(getprop ro.soc.manufacturer)

# Android 14+ 專屬記憶體調優
if [ -n "$A_API" ] && [ "$A_API" -ge 34 ]; then
    resetprop ro.lmk.use_minfree_levels true
    resetprop ro.lmk.enhance_batch_kill true
    resetprop ro.lmk.swap_util_max 90
fi

# 設備底層屬性調優
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    # 高通 (Snapdragon)
    resetprop ro.vendor.qti.config.zram true
    resetprop ro.vendor.qti.sys.fw.bservice_enable true
    resetprop ro.vendor.qti.core.ctl_max_cpu 4
    resetprop ro.vendor.qti.core.ctl_min_cpu 0
elif echo "$SOC_MAKER" | grep -qi "MediaTek"; then
    # 聯發科 (MediaTek)
    resetprop ro.mtk_perf_fast_start_win 1
    resetprop ro.mtk_perf_response_time 1
    resetprop ro.vendor.mtk_zram_extend 1
    resetprop ro.vendor.mtk.sensor.support true
    resetprop ro.vendor.num_mdm_crashes 0
fi

settings put system config.hw_quickpoweron true
settings put system surface_flinger_use_frame_rate_api true

chmod +x $MODDIR/omnisched
nohup $MODDIR/omnisched > /dev/null 2>&1 &
