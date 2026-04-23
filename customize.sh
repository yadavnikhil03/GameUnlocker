#!/bin/bash
SKIPMOUNT=false
PROPFILE=true
POSTFSDATA=true
LATESTARTSERVICE=true

Market_Name=`getprop ro.product.marketname`
Device=`getprop ro.product.device`
Model=`getprop ro.product.model`
Version=`getprop ro.build.version.incremental`
Android=`getprop ro.build.version.release`
CPU_ABI=`getprop ro.product.cpu.abi`
CommonPath=$MODPATH/common

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
}

print_modname

on_install() {
  ui_print " [*] Extracting module files"
  unzip -o "$ZIPFILE" \
    'module.prop' \
    'common/*' \
    'zygisk/*' \
    'webroot/*' \
    'system/*' \
    -d $MODPATH >&2
}

set_permissions() {
  ui_print " [*] Applying permissions"
  set_perm_recursive  $MODPATH  0  0  0755  0644
}

ui_print " [*] Preparing package"
mv ${CommonPath}/* $MODPATH
rm -rf ${CommonPath}

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

ui_print " [*] Finalizing config files"
chmod 0644 $MODPATH/config.json
chmod 0444 $MODPATH/cpuinfo_spoof

ui_print ""
ui_print " Installation complete"
ui_print " Reboot the device to activate the module"
ui_print ""
ui_print " Game Unlocker is ready"
ui_print ""
