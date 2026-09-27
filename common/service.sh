#!/system/bin/sh

MODDIR=${0%/*}
LOG_TAG="GameUnlocker/Service"
GU_CTRL="$MODDIR/gu_controller"
CONFIG_FILE="$MODDIR/config.json"
STATE_DIR="/data/local/tmp/gameunlocker"
PERF_STATE="$STATE_DIR/perf_active"
GAME_PID_FILE="$STATE_DIR/game_pid"

_log()  { log -p i -t "$LOG_TAG" "$1" 2>/dev/null || true; }
_logw() { log -p w -t "$LOG_TAG" "$1" 2>/dev/null || true; }
_loge() { log -p e -t "$LOG_TAG" "$1" 2>/dev/null || true; }

if [ ! -f "$CONFIG_FILE" ]; then
    _loge "config.json not found — service aborted"
    exit 0
fi

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done
sleep 3

_log "service.sh: boot complete, initialising performance engine (pid=$$)"

mkdir -p "$STATE_DIR"
chmod 0700 "$STATE_DIR"

find_busybox() {

    for bb in \
        /data/adb/magisk/busybox \
        /data/adb/ksu/bin/busybox \
        /data/adb/ap/bin/busybox \
        /data/adb/modules/busybox-ndk/system/xbin/busybox \
        /system/xbin/busybox \
        /system/bin/busybox; do

        if [ -f "$bb" ] && [ -x "$bb" ]; then
            echo "$bb"
            return 0
        fi
    done

    if command -v busybox >/dev/null 2>&1; then
        command -v busybox
        return 0
    fi
    return 1
}

BB=$(find_busybox)

if [ -n "$BB" ]; then
    _log "Busybox: $BB"
    export PATH="$($BB dirname "$BB"):$PATH"
fi

safe_write() {
    local path="$1" val="$2"

    if [ -w "$path" ]; then
        echo "$val" > "$path" 2>/dev/null && return 0
    fi
    return 1
}

try_write_first() {
    local path="$1"
    shift

    for val in "$@"; do
        safe_write "$path" "$val" && return 0
    done
    return 1
}

HARDWARE=$(getprop ro.hardware)
BOARD_PLATFORM=$(getprop ro.board.platform)
SOC_MODEL=$(getprop ro.soc.model)
ANDROID_SDK=$(getprop ro.build.version.sdk)

is_qualcomm() {

    case "$HARDWARE" in
        qcom|*kalama*|*taro*|*lahaina*|*shima*|*crow*|*cape*|*pineapple*|*sun*|*uksi*) return 0 ;;
    esac

    case "$BOARD_PLATFORM" in
        sm8*|sm7*|sm6*|sdm*|msm*|qcom*) return 0 ;;
    esac
    return 1
}

is_mediatek() {

    case "$HARDWARE" in *mt*|*mediatek*) return 0 ;; esac

    case "$BOARD_PLATFORM" in mt*) return 0 ;; esac
    return 1
}

SELINUX_MODE=$(getenforce 2>/dev/null || echo "Unknown")
_log "SELinux: $SELINUX_MODE"

if [ -f "$MODDIR/GameUnlockerApp.apk" ]; then

    if command -v magisk >/dev/null 2>&1; then
        _log "Installing companion app"
        pm install -g "$MODDIR/GameUnlockerApp.apk" >/dev/null 2>&1 && \
            _log "Companion app installed" || \
            _logw "Companion app install failed (non-fatal)"
        rm -f "$MODDIR/GameUnlockerApp.apk"
    else
        rm -f "$MODDIR/GameUnlockerApp.apk"
    fi
fi

apply_cpu_baseline() {
    _log "Applying CPU schedutil baseline"
    local gov_path="/sys/devices/system/cpu/cpufreq"

    for policy_dir in "$gov_path"/policy*; do
        [ -d "$policy_dir" ] || continue
        local gov_file="$policy_dir/scaling_governor"
        local current_gov
        current_gov=$(cat "$gov_file" 2>/dev/null)

        case "$current_gov" in
            schedutil)
                safe_write "$policy_dir/schedutil/up_rate_limit_us"   500
                safe_write "$policy_dir/schedutil/down_rate_limit_us" 20000
                safe_write "$policy_dir/schedutil/hispeed_load"       90
                safe_write "$policy_dir/schedutil/pl"                 1
                ;;
            interactive)
                safe_write "$policy_dir/interactive/timer_rate"       20000
                safe_write "$policy_dir/interactive/min_sample_time"  40000
                safe_write "$policy_dir/interactive/hispeed_freq"     1200000
                safe_write "$policy_dir/interactive/go_hispeed_load"  90
                ;;
        esac
    done
}

apply_cpu_baseline

apply_io_tuning() {
    _log "Applying I/O scheduler tuning"

    for q in /sys/block/*/queue; do
        [ -f "$q/scheduler" ] || continue
        try_write_first "$q/scheduler" none mq-deadline deadline || true
        safe_write "$q/add_random" 0
        safe_write "$q/read_ahead_kb" 256
        safe_write "$q/nr_requests" 64
    done
}

apply_io_tuning

apply_memory_tuning() {
    _log "Applying VM/memory tuning"
    safe_write /proc/sys/vm/swappiness           10
    safe_write /proc/sys/vm/vfs_cache_pressure   50
    safe_write /proc/sys/vm/dirty_ratio          20
    safe_write /proc/sys/vm/dirty_background_ratio 5
    safe_write /proc/sys/vm/dirty_expire_centisecs 3000
    safe_write /proc/sys/vm/dirty_writeback_centisecs 500
    safe_write /proc/sys/vm/page-cluster         0   # Disable read-around for random access
    safe_write /proc/sys/vm/stat_interval        10  # Reduce accounting overhead
}

apply_memory_tuning

apply_network_tuning() {
    _log "Applying network tuning"

    if grep -q "bbr" /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null; then
        safe_write /proc/sys/net/ipv4/tcp_congestion_control bbr
    fi
    safe_write /proc/sys/net/ipv4/tcp_low_latency   1
    safe_write /proc/sys/net/ipv4/tcp_fastopen      3
    safe_write /proc/sys/net/ipv4/tcp_slow_start_after_idle 0
    safe_write /proc/sys/net/core/rmem_max          16777216
    safe_write /proc/sys/net/core/wmem_max          16777216
}

apply_network_tuning

apply_qualcomm_gpu_baseline() {
    _log "Applying Qualcomm Adreno baseline"

    local kgsl_dev

    for d in /sys/class/kgsl/kgsl-3d0 /sys/kernel/gpu/gpu0; do
        [ -d "$d" ] && kgsl_dev="$d" && break
    done

    if [ -z "$kgsl_dev" ]; then
        _logw "kgsl device not found — skipping GPU tuning"
        return
    fi

    try_write_first "$kgsl_dev/devfreq/governor" \
        msm-adreno-tz simple_ondemand || true

    safe_write "$kgsl_dev/idle_timer" 80

    safe_write "$kgsl_dev/bus_split" 1
    safe_write "$kgsl_dev/force_bus_on" 0  # Let driver manage normally
    safe_write "$kgsl_dev/force_clk_on"  0  # Same
    safe_write "$kgsl_dev/force_rail_on" 0

    _log "Adreno baseline applied at $kgsl_dev"
}

apply_mediatek_gpu_baseline() {
    _log "Applying MediaTek GPU baseline"
    local gpu_base="/sys/kernel/ged/hal"
    safe_write "$gpu_base/gpu_boost_level" 0 2>/dev/null || true
    safe_write /sys/power/mtk_lpm/plat_isr_stall 1 2>/dev/null || true
}

if is_qualcomm; then
    apply_qualcomm_gpu_baseline
elif is_mediatek; then
    apply_mediatek_gpu_baseline
fi

_thermal_setprop() {
    command -v resetprop >/dev/null 2>&1 && \
        resetprop "$1" "$2" 2>/dev/null || \
        setprop "$1" "$2" 2>/dev/null || true
}

apply_thermal_baseline() {
    _log "Applying thermal baseline"

    local qti_thermal_cfg="/vendor/etc/thermal-engine.conf"

    if [ -f "$qti_thermal_cfg" ] && command -v resetprop >/dev/null 2>&1; then
        resetprop vendor.thermal.config thermal-engine-game.conf 2>/dev/null || \
        resetprop vendor.thermal.config thermal-engine.conf 2>/dev/null || true
    fi

    _thermal_setprop vendor.thermal.thermal_mode Gaming
    _thermal_setprop persist.vendor.thermal.config thermal-engine-game.conf

    if is_qualcomm; then
        _thermal_setprop vendor.thermal.gaming.mode 1
    fi
}

apply_thermal_baseline

if [ ! -f "$GU_CTRL" ] || [ ! -x "$GU_CTRL" ]; then
    _loge "gu_controller binary not found or not executable at $GU_CTRL"
else
    killall gu_controller 2>/dev/null || true
    pkill -f gu_controller 2>/dev/null || true
    sleep 0.5

    _log "Starting gu_controller daemon"
    "$GU_CTRL" >/dev/null 2>&1 &
    CTRL_PID=$!
    _log "gu_controller started (pid=$CTRL_PID)"
fi

extract_game_packages() {

    if command -v jq >/dev/null 2>&1 && [ -f "$CONFIG_FILE" ]; then
        jq -r '.routing_rules[].pattern' "$CONFIG_FILE" 2>/dev/null
    elif [ -f "$MODDIR/jq" ]; then
        "$MODDIR/jq" -r '.routing_rules[].pattern' "$CONFIG_FILE" 2>/dev/null
    else
        grep -o '"pattern": *"[^"]*"' "$CONFIG_FILE" 2>/dev/null | \
            sed 's/.*"pattern": *"\([^"]*\)".*/\1/'
    fi
}

GAME_PACKAGES=$(extract_game_packages)
GAME_COUNT=$(echo "$GAME_PACKAGES" | grep -c '[a-z]' 2>/dev/null || echo 0)
_log "Monitoring $GAME_COUNT configured game packages"

apply_game_perf() {
    local pkg="$1"
    [ -f "$PERF_STATE" ] && return  # Already active
    _log "Game detected: $pkg — applying performance mode"
    touch "$PERF_STATE"

    for policy_dir in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -d "$policy_dir" ] || continue
        local max_freq
        max_freq=$(cat "$policy_dir/cpuinfo_max_freq" 2>/dev/null)
        local min_freq
        min_freq=$(cat "$policy_dir/cpuinfo_min_freq" 2>/dev/null)

        if [ -n "$max_freq" ] && [ "$max_freq" -gt 0 ] 2>/dev/null; then
            local floor=$((max_freq * 60 / 100))
            safe_write "$policy_dir/scaling_min_freq" "$floor"
        fi
    done

    if is_qualcomm; then

        for d in /sys/class/kgsl/kgsl-3d0 /sys/kernel/gpu/gpu0; do
            [ -d "$d" ] || continue
            local max_gpu_freq
            max_gpu_freq=$(cat "$d/devfreq/max_freq" 2>/dev/null || echo 0)

            if [ "$max_gpu_freq" -gt 0 ] 2>/dev/null; then
                local gpu_floor=$((max_gpu_freq * 50 / 100))
                safe_write "$d/devfreq/min_freq" "$gpu_floor"
            fi
        done
    fi

    safe_write /proc/sys/vm/swappiness 5
}

restore_game_perf() {
    [ -f "$PERF_STATE" ] || return
    _log "No active game — restoring baseline"
    rm -f "$PERF_STATE"

    for policy_dir in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -d "$policy_dir" ] || continue
        local hw_min
        hw_min=$(cat "$policy_dir/cpuinfo_min_freq" 2>/dev/null)
        [ -n "$hw_min" ] && safe_write "$policy_dir/scaling_min_freq" "$hw_min"
    done

    if is_qualcomm; then

        for d in /sys/class/kgsl/kgsl-3d0 /sys/kernel/gpu/gpu0; do
            [ -d "$d" ] || continue
            local hw_min_gpu
            hw_min_gpu=$(cat "$d/devfreq/available_frequencies" 2>/dev/null | \
                         tr ' ' '\n' | grep '^[0-9]' | sort -n | head -1)
            [ -n "$hw_min_gpu" ] && safe_write "$d/devfreq/min_freq" "$hw_min_gpu"
        done
    fi

    safe_write /proc/sys/vm/swappiness 10
}

get_foreground_package() {
    dumpsys activity 2>/dev/null | \
        grep -m1 'mCurrentFocus\|mFocusedApp' | \
        grep -oE '[a-z][a-zA-Z0-9_]+(\.[a-zA-Z0-9_]+){1,}' | \
        tail -1 | tr -d ' ' 2>/dev/null || echo ""
}

check_game_proc() {

    if [ -z "$GAME_PACKAGES" ]; then return 1; fi
    local fg_pkg
    fg_pkg=$(get_foreground_package)

    if [ -z "$fg_pkg" ]; then return 1; fi

    echo "$GAME_PACKAGES" | grep -qxF "$fg_pkg" 2>/dev/null
}

_log "Starting game process monitor loop (event-driven)"

(
    FIFO_PATH="$STATE_DIR/fg_trigger"
    rm -f "$FIFO_PATH"
    mkfifo "$FIFO_PATH" 2>/dev/null
    chmod 0666 "$FIFO_PATH"

    exec 3<> "$FIFO_PATH"

    if check_game_proc; then
        pkg=$(get_foreground_package)
        apply_game_perf "$pkg"
    else
        restore_game_perf
    fi

    while read -r line <&3; do

        if check_game_proc; then
            pkg=$(get_foreground_package)
            apply_game_perf "$pkg"
        else
            restore_game_perf
        fi
    done
) &

MONITOR_PID=$!
_log "Monitor loop started (pid=$MONITOR_PID)"

(

    while true; do
        sleep 10

        if ! pgrep -f "gu_controller" >/dev/null 2>&1; then
            _loge "Watchdog: gu_controller crashed! Restarting..."
            "$GU_CTRL" >/dev/null 2>&1 &
        fi
    done
) &
WATCHDOG_PID=$!
_log "Watchdog started (pid=$WATCHDOG_PID)"

if [ "$ANDROID_SDK" -ge 31 ] 2>/dev/null; then
    _log "Applying ADPF hints (SDK $ANDROID_SDK)"
    command -v resetprop >/dev/null 2>&1 && {
        resetprop debug.hwui.use_hint_manager true 2>/dev/null || true
        resetprop debug.hwui.target_cpu_time_percent 66 2>/dev/null || true
    }
fi

_log "service.sh: all components initialised"
wait