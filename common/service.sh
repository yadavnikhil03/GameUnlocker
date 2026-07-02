#!/system/bin/sh

MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    exit 0
fi

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

if [ -f "$MODDIR/GameUnlockerApp.apk" ]; then
    pm install -g "$MODDIR/GameUnlockerApp.apk"
    rm -f "$MODDIR/GameUnlockerApp.apk"
fi

exec $MODDIR/gu_controller >/dev/null 2>&1 &