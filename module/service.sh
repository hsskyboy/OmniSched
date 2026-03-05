#!/system/bin/sh
MODDIR=${0%/*}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

settings put system config.hw_quickpoweron true
settings put system surface_flinger_use_frame_rate_api true

chmod +x $MODDIR/omnisched
nohup $MODDIR/omnisched > /dev/null 2>&1 &
