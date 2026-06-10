#!/system/bin/sh

MODDIR=${0%/*}

setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1
setprop touch.vendor.sampling_rate 240

log -p i -t GameUnlocker "Applied baseline FPS properties"
