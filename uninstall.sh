#!/system/bin/sh

MODDIR=${0%/*}
LOG_TAG="GameUnlocker/Uninstall"

_log()  { log -p i -t "$LOG_TAG" "$1" 2>/dev/null || true; }
_logw() { log -p w -t "$LOG_TAG" "$1" 2>/dev/null || true; }

_log "Uninstall cleanup starting"

_log "Stopping gu_controller daemon"
killall gu_controller   2>/dev/null || true
pkill -f gu_controller  2>/dev/null || true
pkill -f "httpd -p 127.0.0.1:" 2>/dev/null || true

pkill -f "gameunlocker" 2>/dev/null || true

_log "Resetting runtime properties"

if command -v resetprop >/dev/null 2>&1; then
    resetprop vendor.gpu.mode           normal   2>/dev/null || true
    resetprop vendor.gfx.low_quality    0        2>/dev/null || true
    resetprop vendor.thermal.gaming.mode 0       2>/dev/null || true
    resetprop vendor.thermal.thermal_mode ""     2>/dev/null || true

    resetprop --delete debug.vendor.qti.game.fps    2>/dev/null || true
    resetprop --delete persist.vendor.qti.game.fps  2>/dev/null || true

    resetprop debug.hwui.renderer skiagl            2>/dev/null || true
    resetprop debug.hwui.use_hint_manager false     2>/dev/null || true

    resetprop vendor.mtk.gpu.gaming.mode 0          2>/dev/null || true
else
    setprop vendor.gpu.mode           normal         2>/dev/null || true
    setprop vendor.gfx.low_quality    0              2>/dev/null || true
    setprop debug.vendor.qti.game.fps ""             2>/dev/null || true
    setprop persist.vendor.qti.game.fps ""           2>/dev/null || true
    setprop debug.hwui.renderer skiagl               2>/dev/null || true
fi

_log "Restoring CPU freq floors"

for policy_dir in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$policy_dir" ] || continue
    hw_min=$(cat "$policy_dir/cpuinfo_min_freq" 2>/dev/null)

    if [ -n "$hw_min" ] && [ -w "$policy_dir/scaling_min_freq" ]; then
        echo "$hw_min" > "$policy_dir/scaling_min_freq" 2>/dev/null || true
    fi
done

_log "Restoring GPU freq floors"

for d in /sys/class/kgsl/kgsl-3d0 /sys/kernel/gpu/gpu0; do
    [ -d "$d" ] || continue
    hw_min_gpu=$(cat "$d/devfreq/available_frequencies" 2>/dev/null | \
                 tr ' ' '\n' | grep '^[0-9]' | sort -n | head -1)

    if [ -n "$hw_min_gpu" ] && [ -w "$d/devfreq/min_freq" ]; then
        echo "$hw_min_gpu" > "$d/devfreq/min_freq" 2>/dev/null || true
    fi
done

if grep -q "cpuinfo_spoof\|gameunlocker" /proc/mounts 2>/dev/null; then
    _log "Unmounting /proc/cpuinfo spoof"
    umount /proc/cpuinfo 2>/dev/null || true
fi

_log "Restoring VM defaults"
echo 60  > /proc/sys/vm/swappiness            2>/dev/null || true
echo 80  > /proc/sys/vm/vfs_cache_pressure    2>/dev/null || true
echo 20  > /proc/sys/vm/dirty_ratio           2>/dev/null || true
echo 500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true

COMPANION_PKG="com.yadavnikhil03.gameunlocker"

if pm list packages 2>/dev/null | grep -q "$COMPANION_PKG"; then
    _log "Uninstalling companion app: $COMPANION_PKG"
    pm uninstall "$COMPANION_PKG" >/dev/null 2>&1 || \
        _logw "Companion uninstall failed (may require user action)"
fi

_log "Removing temporary files"
rm -f  /data/local/tmp/gameunlocker_apps.json  2>/dev/null || true
rm -rf /data/local/tmp/gameunlocker            2>/dev/null || true
rm -f  "$MODDIR/auth_token"                    2>/dev/null || true

_log "Uninstall cleanup complete"
