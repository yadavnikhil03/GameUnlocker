#!/system/bin/sh

MODDIR=${0%/*}

# ===== Compatibility Baseline =====
# Keep only low-risk baseline properties at post-fs-data.
# Dynamic game-mode tuning is handled in service.sh when a configured game is foregrounded.
setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1
setprop touch.vendor.sampling_rate 240

# Log
log -p i -t GameUnlocker "Applied baseline FPS properties"
