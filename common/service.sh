#!/system/bin/sh
MODDIR=${0%/*}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

# 處理器配置讀取
MAX_CORE=$(cat /sys/devices/system/cpu/possible | awk -F'-' '{print $2}')
if [ -z "$MAX_CORE" ]; then MAX_CORE=7; fi
ALL_CORES="0-${MAX_CORE}"

CLUSTER_LITTLE=""
CLUSTER_MID=""
CLUSTER_BIG=""

POLICIES=$(ls -d /sys/devices/system/cpu/cpufreq/policy* 2>/dev/null | sort -V)
POLICY_COUNT=$(echo "$POLICIES" | wc -w)

if [ "$POLICY_COUNT" -eq 3 ]; then
    IDX=1
    for POL in $POLICIES; do
        CORES=$(cat "$POL/affected_cpus" | tr ' ' ',')
        CORES=$(echo $CORES | sed 's/,$//')
        
        if [ $IDX -eq 1 ]; then CLUSTER_LITTLE=$CORES
        elif [ $IDX -eq 2 ]; then CLUSTER_MID=$CORES
        elif [ $IDX -eq 3 ]; then CLUSTER_BIG=$CORES
        fi
        IDX=$((IDX + 1))
    done
elif [ "$POLICY_COUNT" -eq 2 ]; then
    IDX=1
    for POL in $POLICIES; do
        CORES=$(cat "$POL/affected_cpus" | tr ' ' ',')
        CORES=$(echo $CORES | sed 's/,$//')
        
        if [ $IDX -eq 1 ]; then CLUSTER_LITTLE=$CORES
        elif [ $IDX -eq 2 ]; then CLUSTER_BIG=$CORES
        fi
        IDX=$((IDX + 1))
    done
    CLUSTER_MID=$CLUSTER_BIG
else
    HALF=$((MAX_CORE / 2))
    CLUSTER_LITTLE="0-$HALF"
    CLUSTER_MID="0-$MAX_CORE"
    CLUSTER_BIG="0-$MAX_CORE"
fi

# 寫入 Cpuset 分配
# Top-app
echo "$ALL_CORES" > /dev/cpuset/top-app/cpus 2>/dev/null

# Foreground
if [ -n "$CLUSTER_MID" ]; then
    echo "${CLUSTER_LITTLE},${CLUSTER_MID}" > /dev/cpuset/foreground/cpus 2>/dev/null
else
    echo "$ALL_CORES" > /dev/cpuset/foreground/cpus 2>/dev/null
fi

# Background & System
echo "$CLUSTER_LITTLE" > /dev/cpuset/background/cpus 2>/dev/null
echo "$CLUSTER_LITTLE" > /dev/cpuset/system-background/cpus 2>/dev/null

# CPU 調度 基礎設定
for GOV in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        # 強制使用 schedutil
        echo "schedutil" > "$GOV" 2>/dev/null
    fi
done

# CPU & GPU 调度优化
GPU_ADRENO="/sys/class/kgsl/kgsl-3d0/devfreq/governor"
if [ -f "$GPU_ADRENO" ]; then
    # 高通 Adreno
    echo "msm-adreno-tz" > "$GPU_ADRENO" 2>/dev/null
else
    # 通用 SoC
    for DEVFREQ in /sys/class/devfreq/*; do
        if [ -f "$DEVFREQ/governor" ]; then
            GOV_LIST=$(cat "$DEVFREQ/available_governors" 2>/dev/null)
            if echo "$GOV_LIST" | grep -q "mali_ondemand"; then
                echo "mali_ondemand" > "$DEVFREQ/governor" 2>/dev/null
            elif echo "$GOV_LIST" | grep -q "simple_ondemand"; then
                echo "simple_ondemand" > "$DEVFREQ/governor" 2>/dev/null
            fi
        fi
    done
fi

# 系統 Settings 參數修改
settings put system config.hw_quickpoweron true
settings put system surface_flinger_use_frame_rate_api true
