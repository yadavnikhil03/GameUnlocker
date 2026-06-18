#!/system/bin/sh

MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

if [ ! -f "$CONFIG_FILE" ]; then
    exit 0
fi

get_foreground_app() {
    local pkg window_out

    # A single dumpsys window call; reuse the output for both regex variants.
    window_out=$(dumpsys window 2>/dev/null)
    pkg=$(printf '%s\n' "$window_out" | grep -m1 -E 'mCurrentFocus|mFocusedApp' | sed -n 's/.*[[:space:]]\([A-Za-z0-9_][A-Za-z0-9_.]*\)\/.*/\1/p')
    if [ -z "$pkg" ]; then
        pkg=$(printf '%s\n' "$window_out" | grep -m1 -E 'mCurrentFocus|mFocusedApp' | sed -n 's/.*\([A-Za-z0-9_][A-Za-z0-9_.]*\)\/.*/\1/p')
    fi
    if [ -z "$pkg" ]; then
        pkg=$(dumpsys activity activities 2>/dev/null | grep -m1 -E 'mResumedActivity|topResumedActivity|ResumedActivity' | sed -n 's/.*[[:space:]]\([A-Za-z0-9_][A-Za-z0-9_.]*\)\/.*/\1/p')
    fi
    if [ -z "$pkg" ]; then
        pkg=$(dumpsys activity activities 2>/dev/null | grep -m1 -E 'topResumedActivity|ResumedActivity' | sed -n 's/.*u0 \([A-Za-z0-9_][A-Za-z0-9_.]*\)\/.*/\1/p')
    fi

    if [ -n "$pkg" ]; then
        case "$pkg" in
            *Error*|*null*|*KeyEvent*) pkg="" ;;
        esac
    fi

    echo "$pkg"
}

is_system_package() {
    case "$1" in
        ""|android|com.android.*|com.google.android.*|com.miui.*|com.xiaomi.*|com.sec.*|com.huawei.*|com.oppo.*|com.coloros.*|com.heytap.*|com.vivo.*|com.iqoo.*|com.bbk.*|miui.*|org.lineageos.*|*.launcher|*.launcher.*|*.systemui|com.android.inputmethod.*|*inputmethod*)
            return 0 ;;
    esac
    return 1
}

build_game_list() {
    GAME_PKGS=""
    if [ -f "$CONFIG_FILE" ]; then
        GAME_PKGS=$(sed -n 's/.*"pattern"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$CONFIG_FILE" | grep -v '^\*$')
    fi
}

is_game_configured() {
    [ -z "$1" ] && return 1

    if is_system_package "$1"; then
        return 1
    fi

    echo "$GAME_PKGS" | grep -qxF "$1"
}

is_qualcomm_device() {
    local hw
    hw=$(getprop ro.hardware)
    case "$hw" in
        qcom|kalama|taro|lahaina|shima|holi|napa|napali|crow|cape|uksi|kalama*|taro*|lahaina*|shima*|napa*|napali*)
            return 0 ;;
    esac
    return 1
}

# Capture OEM defaults once so restore returns the device to its real state
# rather than forcing an arbitrary value.
GPU_MODE_DEFAULT=$(getprop vendor.gpu.mode)
GFX_LOW_QUALITY_DEFAULT=$(getprop vendor.gfx.low_quality)

apply_perf_mode() {
    if is_qualcomm_device; then
        setprop vendor.gpu.mode performance 2>/dev/null
        setprop vendor.gfx.low_quality 1 2>/dev/null
    fi
}

restore_perf_mode() {
    if is_qualcomm_device; then
        setprop vendor.gpu.mode "$GPU_MODE_DEFAULT" 2>/dev/null
        setprop vendor.gfx.low_quality "$GFX_LOW_QUALITY_DEFAULT" 2>/dev/null
    fi
}

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

sleep 10

build_game_list

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
