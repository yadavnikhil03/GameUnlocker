#!/system/bin/sh

MODDIR=${0%/*}

setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120

log -p i -t GameUnlocker "Applied baseline FPS properties"
rm -f "$MODDIR/auth_token"
