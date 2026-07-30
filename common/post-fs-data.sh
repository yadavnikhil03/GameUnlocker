#!/system/bin/sh

MODDIR=${0%/*}

# ===== Compatibility Baseline =====
# Only set writable vendor props. ro.* props cannot be set via setprop on Android 12+
# and are handled by Magisk's system.prop mechanism instead.
# Dynamic game-mode tuning is handled in service.sh via gu_controller.
setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120

# Log
log -p i -t GameUnlocker "Applied baseline FPS properties"
rm -f "$MODDIR/auth_token"
