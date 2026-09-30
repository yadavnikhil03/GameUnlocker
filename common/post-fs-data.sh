#!/system/bin/sh
MODDIR=${0%/*}
LOG_TAG="GameUnlocker/PostFS"
_log() { log -p i -t "$LOG_TAG" "$1" 2>/dev/null || true; }
_log "post-fs-data: starting (pid=$$)"
rm -f "$MODDIR/auth_token" 2>/dev/null
_log "post-fs-data: completed"
