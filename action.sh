#!/system/bin/sh

MODDIR=${0%/*}

print_diagnostic() {
    local line="$1"
    echo "$line"
}

print_diagnostic "=== GameUnlocker v2.3 Diagnostics ==="
print_diagnostic ""

print_diagnostic "--- Module ---"

if [ -f "$MODDIR/disable" ]; then
    print_diagnostic "Status: DISABLED"
else
    print_diagnostic "Status: ENABLED"
fi
print_diagnostic ""

print_diagnostic "--- Root ---"

if command -v apd >/dev/null 2>&1; then
    print_diagnostic "Root: APatch $(apd --version 2>/dev/null | head -1)"
elif command -v ksud >/dev/null 2>&1; then
    print_diagnostic "Root: KernelSU $(ksud --version 2>/dev/null | head -1)"
elif command -v magisk >/dev/null 2>&1; then
    print_diagnostic "Root: Magisk $(magisk --version 2>/dev/null | head -1)"
else
    print_diagnostic "Root: Unknown"
fi
print_diagnostic "SELinux: $(getenforce 2>/dev/null || echo Unknown)"
print_diagnostic ""

print_diagnostic "--- Daemon ---"

if pgrep -x "gu_controller" >/dev/null 2>&1; then
    DAEMON_PID=$(pgrep -x gu_controller | head -1)
    print_diagnostic "gu_controller: RUNNING (pid=$DAEMON_PID)"
else
    print_diagnostic "gu_controller: NOT RUNNING"
fi
print_diagnostic ""

print_diagnostic "--- Performance Props ---"
print_diagnostic "Qualcomm GPU mode : $(getprop vendor.gpu.mode)"
print_diagnostic "Game FPS (debug)  : $(getprop debug.vendor.qti.game.fps)"
print_diagnostic "Game FPS (persist): $(getprop persist.vendor.qti.game.fps)"
print_diagnostic "HWUI renderer     : $(getprop debug.hwui.renderer)"
print_diagnostic "Dalvik heap size  : $(getprop dalvik.vm.heapsize)"
print_diagnostic "Thermal mode      : $(getprop vendor.thermal.thermal_mode)"
print_diagnostic ""

print_diagnostic "--- CPU ---"

for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$policy" ] || continue
    POL_NAME=$(basename "$policy")
    GOV=$(cat "$policy/scaling_governor" 2>/dev/null || echo "?")
    CUR=$(cat "$policy/scaling_cur_freq"  2>/dev/null | awk '{printf "%.0f MHz", $1/1000}')
    MIN=$(cat "$policy/scaling_min_freq"  2>/dev/null | awk '{printf "%.0f", $1/1000}')
    MAX=$(cat "$policy/scaling_max_freq"  2>/dev/null | awk '{printf "%.0f", $1/1000}')
    print_diagnostic "  $POL_NAME: $GOV @ $CUR (min=${MIN}MHz max=${MAX}MHz)"
done
print_diagnostic ""

print_diagnostic "--- GPU ---"

for d in /sys/class/kgsl/kgsl-3d0 /sys/kernel/gpu/gpu0; do
    [ -d "$d" ] || continue
    GOV=$(cat "$d/devfreq/governor" 2>/dev/null || echo "?")
    CUR=$(cat "$d/devfreq/cur_freq"  2>/dev/null | awk '{printf "%.0f MHz", $1/1000000}' 2>/dev/null || echo "?")
    MIN=$(cat "$d/devfreq/min_freq"  2>/dev/null | awk '{printf "%.0f MHz", $1/1000000}' 2>/dev/null || echo "?")
    MAX=$(cat "$d/devfreq/max_freq"  2>/dev/null | awk '{printf "%.0f MHz", $1/1000000}' 2>/dev/null || echo "?")
    print_diagnostic "  Adreno: $GOV @ $CUR (min=$MIN max=$MAX)"
    print_diagnostic "  idle_timer: $(cat "$d/idle_timer" 2>/dev/null)ms"
    break
done
print_diagnostic ""

print_diagnostic "--- VM ---"
print_diagnostic "  swappiness        : $(cat /proc/sys/vm/swappiness 2>/dev/null)"
print_diagnostic "  vfs_cache_pressure: $(cat /proc/sys/vm/vfs_cache_pressure 2>/dev/null)"
print_diagnostic "  dirty_ratio       : $(cat /proc/sys/vm/dirty_ratio 2>/dev/null)"
print_diagnostic ""

print_diagnostic "--- Network ---"
print_diagnostic "  tcp_congestion    : $(cat /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null)"
print_diagnostic ""

print_diagnostic "--- Recent Logs (last 20 lines) ---"
logcat -d -s "GameUnlocker" -t 20 2>/dev/null | tail -20 || \
    print_diagnostic "(logcat not available)"

print_diagnostic ""
print_diagnostic "=== End of Diagnostics ==="
