#!/system/bin/sh
ui_print "OmniSched v3 安装程序"
ui_print "作者：HSSkyBoy / nikobe918"

sleep 0.4
ui_print "- 正在初始化环境与检测设备相容性..."

API=$(getprop ro.build.version.sdk)
DEVICE_MODEL=$(getprop ro.product.model)
SOC_MAKER=$(getprop ro.soc.manufacturer)

sleep 0.6
ui_print "- 设备型号: $DEVICE_MODEL"
ui_print "- 处理器平台: $SOC_MAKER"
ui_print "- 当前系统 API: $API"
ui_print "-"

if [ "$API" -lt 31 ]; then
    ui_print ""
    ui_print "! OmniSched 依賴 Android 12+ 的底層渲染 API。"
    ui_print "! 您的 API 級別為 $API，無法安裝本模組。"
    abort "! 安裝已中止。"
fi

# 核心硬體平台偵測與提示
if [ -d "/sys/class/kgsl" ] || echo "$SOC_MAKER" | grep -qi "Qualcomm"; then
    ui_print "- 侦测到高通平台：启用 QTI 专属优化。"
elif echo "$SOC_MAKER" | grep -qi "MediaTek" || echo "$SOC_MAKER" | grep -qi "MTK"; then
    ui_print "- 侦测到联发科平台：启用 MTK 专属调度。"
else
    ui_print "- 侦测到通用平台：套用动态调度逻辑。"
fi

ui_print "-"
ui_print "请选择是否预设开启「强制 Vulkan 渲染」"
ui_print "注意：部分设备开启后可能导致卡开机，建议首刷用户选「关闭」"
ui_print ""
ui_print " [音量 +] : 预设开启"
ui_print " [音量 -] : 预设关闭"

VK_FORCE=false
KEY_TIMEOUT=10
KEY_PRESSED=0

if command -v getevent >/dev/null 2>&1; then
    ui_print "- 等待音量键输入（${KEY_TIMEOUT} 秒后预设关闭）..."

    i=0
    while [ "$i" -lt "$KEY_TIMEOUT" ]; do
        key_event=$(getevent -qlc 1 2>/dev/null)

        if echo "$key_event" | grep -qi "KEY_VOLUMEUP"; then
            VK_FORCE=true
            KEY_PRESSED=1
            ui_print "-> 已选择：预设开启强制 Vulkan"
            break
        elif echo "$key_event" | grep -qi "KEY_VOLUMEDOWN"; then
            VK_FORCE=false
            KEY_PRESSED=1
            ui_print "-> 已选择：预设关闭强制 Vulkan"
            break
        fi

        i=$((i + 1))
        sleep 1
    done

    if [ "$KEY_PRESSED" -eq 0 ]; then
        ui_print "-> 未侦测到按键输入，将使用预设值：关闭强制 Vulkan"
    fi
else
    ui_print "-> 当前环境不支援 getevent，将使用预设值：关闭强制 Vulkan"
fi

ui_print "- 正在写入核心配置..."
CONFIG_DIR="/data/adb/omnisched"
CONFIG_FILE="$CONFIG_DIR/config.json"

mkdir -p "$CONFIG_DIR"

if [ ! -f "$CONFIG_DIR/config.json" ]; then
    cat <<EOF > "$CONFIG_DIR/config.json"
{
  "poll_interval_seconds": 950,
  "cpuset": { "background_little_core_only": true },
  "render": { "vulkan_mode": "off", "vulkan_apps": [] },
  "power": { "policy": "balanced" },
  "performance": { "auto_optimize": false }
}
EOF
else
    ui_print "- 侦测到旧配置，将尝试热重载迁移..."
fi

sleep 0.2
ui_print "- 正在设定权限..."
set_perm_recursive "$MODPATH" 0 0 0755 0755
set_perm_recursive "$CONFIG_DIR" 0 0 0755 0754

ui_print ""
ui_print "安装完成！请重启系统以套用 $MOD_NAME。"
ui_print "提示：若卡在第一屏，可进入安全模式移除模组，或手动删除 $CONFIG_FILE。"
ui_print ""
