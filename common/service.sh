#!/system/bin/sh

MODDIR=${0%/*}
LOG_TAG="GameUnlocker/Service"

_log()  { log -p i -t "$LOG_TAG" "$1" 2>/dev/null || true; }

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

_log "service.sh: boot complete. Lightweight mode active (no sysfs tuning)."

exit 0