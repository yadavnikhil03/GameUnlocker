#!/system/bin/sh
SKIPMOUNT=false
PROPFILE=true
POSTFSDATA=true
LATESTARTSERVICE=true
SKIPUNZIP=1

Market_Name=$(getprop ro.product.marketname)
if [ -z "$Market_Name" ]; then
    Market_Name="$(getprop ro.product.brand) $(getprop ro.product.model)"
fi
Device=$(getprop ro.product.device)
Model=$(getprop ro.product.model)
Version=$(getprop ro.build.version.incremental)
Android=$(getprop ro.build.version.release)
CPU_ABI=$(getprop ro.product.cpu.abi)

abort_missing_zygisk() {
  ui_print ""
  ui_print " [!] Package validation failed"
  ui_print ""
  ui_print " Detected ABI : $CPU_ABI"
  ui_print " Missing file  : $1"
  ui_print ""
  ui_print " This ZIP looks incomplete or was built without the Zygisk library."
  ui_print " Install the release build, then reboot and try again."
  abort
}

print_modname() {
  ui_print ""
  ui_print " Game Unlocker"
  ui_print " High-performance Zygisk module"
  ui_print " Maintainer: @yadavnikhil03"
  ui_print ""
  ui_print " Device profile"
  ui_print " - Name : $Market_Name"
  ui_print " - Model: $Model"
  ui_print " - Code : $Device"
  ui_print " - Android: $Android"
  ui_print " - Build : $Version"
  ui_print " - ABI   : $CPU_ABI"
  ui_print ""
  ui_print " Starting installation..."
  ui_print ""
  sleep 1
}

print_modname

check_root() {
  ui_print " [*] Checking Root Environment"
  sleep 0.5
  local count=0
  local sol=""

  if command -v apd >/dev/null; then
    sol="APatch"
    count=$((count + 1))
  fi

  if command -v ksud >/dev/null; then
    sol="KernelSU"
    count=$((count + 1))
  fi

  if command -v magisk >/dev/null; then
    sol="Magisk"
    count=$((count + 1))
  fi

  if [ "$count" -gt 1 ]; then
    ui_print " [!] Error: Multiple Root Solutions Found!"
    abort
  elif [ "$count" -eq 0 ]; then
    ui_print " [!] Error: No Supported Root Solution (Magisk/KSU/APatch)!"
    abort
  else
    ui_print " [*] Detected: $sol"
  fi
}

check_root

check_zygisk_implementation() {
  ui_print " [*] Checking Zygisk Implementation"
  sleep 0.5

  if [ ! -d "/data/adb/modules/zygisksu" ] && \
     [ ! -d "/data/adb/modules/rezygisk" ] && \
     [ ! -d "/data/adb/modules/neozygisk" ] && \
     [ ! -d "/data/adb/modules/zygisk_next" ]; then
    ui_print " [!] Error: No standalone Zygisk implementation found!"
    ui_print " [!] GameUnlocker is incompatible with built-in Magisk Zygisk."
    ui_print " [!] Please install ZygiskNext, ReZygisk, or NeoZygisk first."
    abort
  else
    ui_print " [*] Standalone Zygisk detected."
  fi
}

check_zygisk_implementation

ui_print " [*] Extracting module files"
sleep 0.5
unzip -o "$ZIPFILE" \
  'module.prop' \
  'uninstall.sh' \
  'common/*' \
  'zygisk/*' \
  'webroot/*' \
  -d "$MODPATH" >&2

if [ ! -d "$MODPATH/zygisk" ]; then
  abort_missing_zygisk "$MODPATH/zygisk/<abi>.so"
fi

case "$CPU_ABI" in
  arm64-v8a)
    [ -f "$MODPATH/zygisk/arm64-v8a.so" ] || abort_missing_zygisk "$MODPATH/zygisk/arm64-v8a.so"
    ;;
  armeabi-v7a|armeabi)
    [ -f "$MODPATH/zygisk/armeabi-v7a.so" ] || abort_missing_zygisk "$MODPATH/zygisk/armeabi-v7a.so"
    ;;
  *)
    ui_print ""
    ui_print " [!] Unsupported CPU ABI: $CPU_ABI"
    ui_print " Supported ABIs: arm64-v8a, armeabi-v7a"
    abort
    ;;
esac

ui_print " [*] Zygisk library verified for $CPU_ABI"
sleep 0.5

ui_print " [*] Moving common files into module root"
mv "$MODPATH"/common/* "$MODPATH/" 2>/dev/null
rmdir "$MODPATH/common" 2>/dev/null

if [ -f "$MODPATH/config.json" ]; then
  chmod 0644 "$MODPATH/config.json"
else
  ui_print " [!] Warning: config.json missing after extraction"
fi

ui_print " [*] Applying permissions"
sleep 0.5
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm_recursive "$MODPATH/zygisk" 0 0 0755 0644

ui_print ""
ui_print " Installation complete"
ui_print " Reboot the device to activate the module"
ui_print ""
ui_print " Game Unlocker is ready"
ui_print ""
