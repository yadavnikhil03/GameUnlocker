#!/system/bin/sh
# ============================================================
# GameUnlocker — Module Uninstall Cleanup
# Runs as root when the user removes the module via Magisk/KSU.
# ============================================================

MODDIR=${0%/*}

# ---------------------------------------------------------------
# 1. Kill the gu_controller daemon
#    The daemon runs in the background (started by service.sh)
#    listening on an abstract Unix socket. If not killed, it will
#    keep running orphaned until the next reboot.
# ---------------------------------------------------------------
# killall is in toybox on all modern Android; succeeds silently if
# nothing matches. Using both name forms in case of path differences.
killall gu_controller 2>/dev/null
pkill -f gu_controller 2>/dev/null

# Kill any stale busybox httpd WebUI servers started by action.sh
pkill -f "httpd -p 127.0.0.1:" 2>/dev/null

# ---------------------------------------------------------------
# 2. Restore GPU performance properties
#    The controller sets vendor.gpu.mode=performance and
#    vendor.gfx.low_quality=1 while games are running. On uninstall
#    we reset them to safe defaults.
# ---------------------------------------------------------------
if command -v resetprop >/dev/null 2>&1; then
    # In-memory restore (no -p flag; these are non-persist props)
    resetprop vendor.gpu.mode normal 2>/dev/null
    resetprop vendor.gfx.low_quality 0 2>/dev/null

    # Delete the FPS props we set in post-fs-data.sh
    # debug.* is in-memory only; persist.* survives reboot
    resetprop --delete debug.vendor.qti.game.fps 2>/dev/null
    resetprop --delete persist.vendor.qti.game.fps 2>/dev/null
else
    # Fallback for KSU/APatch where resetprop may not exist
    setprop vendor.gpu.mode normal 2>/dev/null
    setprop vendor.gfx.low_quality 0 2>/dev/null
    setprop debug.vendor.qti.game.fps "" 2>/dev/null
    # persist.* cannot be cleared via setprop — will clear on reboot
fi

# ---------------------------------------------------------------
# 3. Unmount any leftover /proc/cpuinfo bind-mounts
#    The companion handler bind-mounts cpuinfo_spoof over
#    /proc/cpuinfo for CPU-spoof apps. These mounts are per-PID
#    namespace so they typically die with the process, but if any
#    global mount leaked we clean it up here.
# ---------------------------------------------------------------
if grep -q "cpuinfo_spoof" /proc/mounts 2>/dev/null; then
    umount /proc/cpuinfo 2>/dev/null
    log -p i -t GameUnlocker "Unmounted leftover /proc/cpuinfo bind-mount"
fi

# ---------------------------------------------------------------
# 4. Uninstall the companion app (if installed by service.sh)
#    service.sh does: pm install -g "$MODDIR/GameUnlockerApp.apk"
#    We should remove it on uninstall to leave no orphan apps.
# ---------------------------------------------------------------
COMPANION_PKG="com.yadavnikhil03.gameunlocker"
if pm list packages 2>/dev/null | grep -q "$COMPANION_PKG"; then
    pm uninstall "$COMPANION_PKG" >/dev/null 2>&1
    log -p i -t GameUnlocker "Uninstalled companion app: $COMPANION_PKG"
fi

# ---------------------------------------------------------------
# 5. Clean up any data files we may have created outside MODDIR
#    (MODDIR itself is deleted by Magisk automatically)
# ---------------------------------------------------------------
rm -rf /data/local/tmp/gameunlocker 2>/dev/null

log -p i -t GameUnlocker "Module uninstall cleanup complete"
