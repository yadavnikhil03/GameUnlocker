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

print_modname() {
  ui_print ""
  sleep 0.5
  ui_print "╔═╗╔═╗╔═╗  ╦ ╦╔╗╔╦  ╔═╗╔═╗╦╔═╔═╗╦═╗"
  ui_print "╠╣ ╠═╝╚═╗  ║ ║║║║║  ║ ║║  ╠╩╗║╣ ╠╦╝"
  ui_print "╚  ╩  ╚═╝  ╚═╝╝╚╝╩═╝╚═╝╚═╝╩ ╩╚═╝╩╚═"
  sleep 0.5
  ui_print ""
  sleep 0.3
  ui_print "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓"
  ui_print "┃       ⚡ SUPERCHARGE YOUR GAMING EXPERIENCE ⚡ ┃"
  ui_print "┃           Developed by @yadavnikhil03           ┃"
  ui_print "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛"
  sleep 1
  ui_print ""
  ui_print "📱 DEVICE PROFILE 📱"
  ui_print "━━━━━━━━━━━━━━━━━━━━"
  sleep 0.5
  ui_print "  ➤ Device: $Market _Name"
  sleep 0.3
  ui_print "  ➤ Model: $Model"
  sleep 0.3
  ui_print "  ➤ Codename: $Device"
  sleep 0.3
  ui_print "  ➤ Android: $Android"
  sleep 0.3
  ui_print "  ➤ Build: $Version"
  sleep 0.3
  ui_print "  ➤ Architecture: $CPU_ABI"
  sleep 0.5
  ui_print ""
  ui_print "⚙️ INITIALIZING MODULE INSTALLATION ⚙️"
  ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  sleep 1
  ui_print ""
}

print_modname

on_install() {
  ui_print " ▶️ Extracting module files..."
  sleep 0.7
  unzip -o "$ZIPFILE" 'system/*' -d $MODPATH >&2
  sleep 0.5
}

set_permissions() {
  ui_print " ▶️ Setting appropriate permissions..."
  sleep 0.7
  set_perm_recursive  $MODPATH  0  0  0755  0644
  sleep 0.5
}

ui_print " ▶️ Moving files to destination..."
sleep 0.7
mv  ${CommonPath}/*  $MODPATH
rm  -rf ${CommonPath}
sleep 1

ui_print ""
ui_print " ✅ INSTALLATION COMPLETED SUCCESSFULLY ✅"
sleep 0.5
ui_print " 🔄 Reboot your device to apply changes 🔄"
sleep 0.5
ui_print ""
ui_print "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓"
ui_print "┃  Thank you for using Game Unlocker 🎮 - Enjoy gaming!  ┃"
ui_print "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛"
sleep 1
ui_print ""

sleep 1
