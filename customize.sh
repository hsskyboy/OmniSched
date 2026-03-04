#!/system/bin/sh
ui_print "========================================="
ui_print "      OmniSched | 全域靈動分配 (A12+)"
ui_print "       Author：HSSkyBoy/nikobe918"
ui_print "========================================="

sleep 0.4
ui_print "- 正在初始化環境與檢測設備相容性..."

API=$(getprop ro.build.version.sdk)
DEVICE_MODEL=$(getprop ro.product.model)
SOC_MAKER=$(getprop ro.soc.manufacturer)

sleep 0.6
ui_print "- 設備型號: $DEVICE_MODEL"
ui_print "- 處理器平台: $SOC_MAKER"
ui_print "- 當前系統 API: $API"
ui_print "-"

if [ "$API" -lt 31 ]; then
    ui_print " "
    ui_print "! OmniSched 依賴 Android 12+ 的底層渲染 API。"
    ui_print "! 您的 API 級別為 $API，無法安裝本模組。"
    abort "! 安裝已中止。"
fi

# 核心硬體平台偵測與提示
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    ui_print "- 侦测到高通 Snapdragon 平台！"
    ui_print "- 已启用 QTI 专属底层优化与 ZRAM 配置。"
elif echo "$SOC_MAKER" | grep -qi "MediaTek" || echo "$SOC_MAKER" | grep -qi "MTK"; then
    ui_print "- 侦测到联发科天玑 平台！"
    ui_print "- 已启用 MTK 专属底层优化与调度配置。"
else
    ui_print "- 侦测到通用平台。"
    ui_print "- 将套用动态通用型调度并强制 Vulkan 渲染。"
fi

ui_print "-"
ui_print "- 正在部署核心调度文件..."
sleep 0.2
ui_print "- 正在设定权限..."

set_perm_recursive "$MODPATH" 0 0 0755 0755

ui_print " "
ui_print "========================================="
ui_print "  安装完成！请重启系统以套用 OmniSched "
ui_print "========================================="
ui_print " "
