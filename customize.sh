#!/system/bin/sh

SKIPMOUNT=false
PROPFILE=false       # Do not load system.prop at boot to prevent global property bootloops
POSTFSDATA=true      # Run post-fs-data.sh
LATESTARTSERVICE=true # Run service.sh after boot_completed
SKIPUNZIP=1          # We handle extraction manually (SKIPUNZIP=1)

Market_Name=$(getprop ro.product.marketname)

if [ -z "$Market_Name" ]; then
    Market_Name="$(getprop ro.product.brand) $(getprop ro.product.model)"
fi
Device=$(getprop ro.product.device)
Model=$(getprop ro.product.model)
Version=$(getprop ro.build.version.incremental)
Android=$(getprop ro.build.version.release)
Android_SDK=$(getprop ro.build.version.sdk)
CPU_ABI=$(getprop ro.product.cpu.abi)

print_modname() {
  ui_print ""
  ui_print "  ╔══════════════════════════════════╗"
  ui_print "  ║        GAME UNLOCKER v2.3        ║"
  ui_print "  ║  High-Performance Zygisk Module  ║"
  ui_print "  ╚══════════════════════════════════╝"
  ui_print ""
  ui_print "  Maintainer: @yadavnikhil03"
  ui_print ""
  ui_print "  ─── Device Profile ───────────────"
  ui_print "  Name    : $Market_Name"
  ui_print "  Model   : $Model ($Device)"
  ui_print "  Android : $Android (SDK $Android_SDK)"
  ui_print "  Build   : $Version"
  ui_print "  ABI     : $CPU_ABI"
  ui_print "  ──────────────────────────────────"
  ui_print ""
  ui_print "  Starting installation..."
  ui_print ""
  sleep 1
}

abort_missing_zygisk() {
  ui_print ""
  ui_print "  [✗] Package validation failed"
  ui_print ""
  ui_print "  ABI detected : $CPU_ABI"
  ui_print "  Missing file : $1"
  ui_print ""
  ui_print "  This ZIP is incomplete or built without the Zygisk"
  ui_print "  library. Install the full release build."
  abort
}

print_modname

ui_print "  [1/6] Checking root environment"
sleep 0.5

check_root() {
  local count=0
  local sol=""

  command -v apd   >/dev/null 2>&1 && { sol="APatch";   count=$((count + 1)); }
  command -v ksud  >/dev/null 2>&1 && { sol="KernelSU"; count=$((count + 1)); }
  command -v magisk >/dev/null 2>&1 && { sol="Magisk";  count=$((count + 1)); }

  if [ "$count" -gt 1 ]; then
    ui_print "  [✗] Multiple root solutions detected — ambiguous environment!"
    ui_print "      Please use only one of: Magisk, KernelSU, APatch"
    abort
  elif [ "$count" -eq 0 ]; then
    ui_print "  [✗] No supported root solution found (Magisk/KSU/APatch)"
    abort
  fi

  ui_print "  [✓] Root: $sol"
  echo "$sol"
}

ROOT_SOL=$(check_root)

ui_print "  [2/6] Checking Zygisk implementation"
sleep 0.5

check_zygisk_implementation() {

  if [ "$ROOT_SOL" = "KernelSU" ] || [ "$ROOT_SOL" = "APatch" ]; then
    ui_print "  [✓] Zygisk: built-in ($ROOT_SOL)"
    return 0
  fi

  local found=0

  for mod in zygisksu rezygisk neozygisk zygisk_next; do
    [ -d "/data/adb/modules/$mod" ] && { found=1; break; }
  done

  if [ "$found" -eq 0 ]; then
    ui_print "  [✗] No standalone Zygisk implementation found!"
    ui_print "      GameUnlocker requires ZygiskNext, ReZygisk, or NeoZygisk"
    ui_print "      to be installed when using Magisk."
    abort
  fi

  ui_print "  [✓] Zygisk: standalone implementation detected"
}

check_zygisk_implementation

ui_print "  [3/6] Checking Android version"
sleep 0.3

if [ -n "$Android_SDK" ] && [ "$Android_SDK" -lt 31 ] 2>/dev/null; then
  ui_print "  [✗] Android $Android (SDK $Android_SDK) is not supported"
  ui_print "      GameUnlocker requires Android 12 (SDK 31) or higher"
  abort
fi
ui_print "  [✓] Android $Android (SDK $Android_SDK)"

ui_print "  [4/6] Extracting module files"
sleep 0.5

unzip -o "$ZIPFILE" \
  'module.prop' \
  'uninstall.sh' \
  'common/*' \
  'zygisk/*' \
  'webroot/*' \
  'GameUnlockerApp.apk' \
  'gu_controller_arm*' \
  'jq_arm*' \
  -d "$MODPATH" >&2

if [ ! -d "$MODPATH/zygisk" ]; then
  abort_missing_zygisk "$MODPATH/zygisk/<abi>.so"
fi

case "$CPU_ABI" in
  arm64-v8a)
    [ -f "$MODPATH/zygisk/arm64-v8a.so" ] || abort_missing_zygisk "$MODPATH/zygisk/arm64-v8a.so"
    mv  "$MODPATH/gu_controller_arm64" "$MODPATH/gu_controller" 2>/dev/null || true
    rm  -f "$MODPATH/gu_controller_armv7" 2>/dev/null || true
    mv  "$MODPATH/jq_arm64" "$MODPATH/jq" 2>/dev/null || true
    rm  -f "$MODPATH/jq_armv7" 2>/dev/null || true
    ;;
  armeabi-v7a|armeabi)
    [ -f "$MODPATH/zygisk/armeabi-v7a.so" ] || abort_missing_zygisk "$MODPATH/zygisk/armeabi-v7a.so"
    mv  "$MODPATH/gu_controller_armv7" "$MODPATH/gu_controller" 2>/dev/null || true
    rm  -f "$MODPATH/gu_controller_arm64" 2>/dev/null || true
    mv  "$MODPATH/jq_armv7" "$MODPATH/jq" 2>/dev/null || true
    rm  -f "$MODPATH/jq_arm64" 2>/dev/null || true
    ;;
  *)
    ui_print ""
    ui_print "  [✗] Unsupported ABI: $CPU_ABI"
    ui_print "      Supported: arm64-v8a, armeabi-v7a"
    abort
    ;;
esac

ui_print "  [✓] Zygisk library verified ($CPU_ABI)"

mv "$MODPATH"/common/* "$MODPATH/" 2>/dev/null || true
rmdir "$MODPATH/common" 2>/dev/null || true

if [ -f "$MODPATH/GameUnlockerApp.apk" ]; then

  if [ "$ROOT_SOL" != "Magisk" ]; then
    ui_print "  [i] Companion app not needed for $ROOT_SOL — skipping"
    rm -f "$MODPATH/GameUnlockerApp.apk"
  fi
fi

if [ -f "$MODPATH/config.json" ]; then
  chmod 0644 "$MODPATH/config.json"
  ui_print "  [✓] config.json present"
else
  ui_print "  [!] Warning: config.json missing after extraction"
fi

ui_print "  [5/6] SELinux sanity check"
sleep 0.3

SELINUX=$(getenforce 2>/dev/null || echo "Unknown")
ui_print "  [i] SELinux mode: $SELINUX"

if [ "$SELINUX" = "Enforcing" ]; then

  if [ -w "/proc/sys/vm/swappiness" ]; then
    ui_print "  [✓] sysfs write access: OK"
  else
    ui_print "  [!] sysfs write access limited (Enforcing SELinux)"
    ui_print "      Property injection will still work. Some sysfs"
    ui_print "      performance tuning may be skipped at runtime."
  fi
fi

ui_print "  [6/6] Applying permissions"
sleep 0.3

set_perm_recursive "$MODPATH"            0 0 0755 0644
set_perm_recursive "$MODPATH/zygisk"     0 0 0755 0644
set_perm_recursive "$MODPATH/webroot"    0 0 0755 0644

for bin in gu_controller jq; do
  [ -f "$MODPATH/$bin" ] && set_perm "$MODPATH/$bin" 0 0 0755
done
[ -f "$MODPATH/webroot/cgi-bin/api.sh" ] && \
  set_perm "$MODPATH/webroot/cgi-bin/api.sh" 0 0 0755

for sh in service.sh post-fs-data.sh action.sh uninstall.sh; do
  [ -f "$MODPATH/$sh" ] && set_perm "$MODPATH/$sh" 0 0 0755
done

ui_print ""
ui_print "  ╔══════════════════════════════════╗"
ui_print "  ║      Installation Complete!      ║"
ui_print "  ╚══════════════════════════════════╝"
ui_print ""
ui_print "  → Reboot to activate Game Unlocker"
ui_print "  → Configure via WebUI (tap Module Action)"
ui_print ""
