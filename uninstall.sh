#!/system/bin/sh

MODDIR=${0%/*}

killall gu_controller 2>/dev/null
pkill -f gu_controller 2>/dev/null
pkill -f "httpd -p 127.0.0.1:" 2>/dev/null

if command -v resetprop >/dev/null 2>&1; then
    resetprop vendor.gpu.mode normal 2>/dev/null
    resetprop vendor.gfx.low_quality 0 2>/dev/null
    resetprop --delete debug.vendor.qti.game.fps 2>/dev/null
    resetprop --delete persist.vendor.qti.game.fps 2>/dev/null
else
    setprop vendor.gpu.mode normal 2>/dev/null
    setprop vendor.gfx.low_quality 0 2>/dev/null
    setprop debug.vendor.qti.game.fps "" 2>/dev/null
fi

if grep -q "cpuinfo_spoof" /proc/mounts 2>/dev/null; then
    umount /proc/cpuinfo 2>/dev/null
fi

COMPANION_PKG="com.yadavnikhil03.gameunlocker"
if pm list packages 2>/dev/null | grep -q "$COMPANION_PKG"; then
    pm uninstall "$COMPANION_PKG" >/dev/null 2>&1
fi

rm -f /data/local/tmp/gameunlocker_apps.json 2>/dev/null
rm -rf /data/local/tmp/gameunlocker 2>/dev/null

log -p i -t GameUnlocker "Module uninstall cleanup complete"
