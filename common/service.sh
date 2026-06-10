#!/system/bin/sh

MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

get_foreground_app() {
    local pkg
    pkg=$(dumpsys window 2>/dev/null | grep -m1 -E 'mCurrentFocus|mFocusedApp' | sed -n 's/.* \([A-Za-z0-9_.]*\)\/.*/\1/p')
    if [ -z "$pkg" ]; then
        pkg=$(dumpsys activity activities 2>/dev/null | grep -m1 -E 'mResumedActivity|topResumedActivity' | sed -n 's/.* \([A-Za-z0-9_.]*\)\/.*/\1/p')
    fi
    echo "$pkg"
}

is_game_configured() {
    [ -n "$1" ] && grep -q "\"$1\"" "$CONFIG_FILE"
}

apply_perf_mode() {
    setprop persist.vendor.thermal.engine.disable 1
    setprop vendor.gpu.mode performance
    setprop vendor.gfx.low_quality 1
}

restore_perf_mode() {
    setprop persist.vendor.thermal.engine.disable 0
    setprop vendor.gpu.mode normal
    setprop vendor.gfx.low_quality 0
}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

sleep 15
chmod 0644 "$CONFIG_FILE"
chcon u:object_r:system_file:s0 "$MODDIR"
chcon u:object_r:system_file:s0 "$CONFIG_FILE"


setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1
setprop touch.vendor.sampling_rate 240

last_state="idle"
last_pkg=""

while true; do
    FOREGROUND_APP=$(get_foreground_app)

    if is_game_configured "$FOREGROUND_APP"; then
        if [ "$last_state" != "game" ] || [ "$last_pkg" != "$FOREGROUND_APP" ]; then
            log -p i -t GameUnlocker "Game detected: $FOREGROUND_APP - applying performance mode"
            apply_perf_mode
            last_state="game"
            last_pkg="$FOREGROUND_APP"
        fi
    else
        if [ "$last_state" != "idle" ]; then
            log -p i -t GameUnlocker "Game exited: $last_pkg - restoring normal mode"
            restore_perf_mode
            last_state="idle"
            last_pkg=""
        fi
    fi

    sleep 3
done
