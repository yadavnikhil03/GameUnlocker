SKIPMOUNT=false
PROPFILE=true
POSTFSDATA=false
LATESTARTSERVICE=false
Market_Name=`getprop ro.product.marketname`
Device=`getprop ro.product.device`
Model=`getprop ro.product.model`
Version=`getprop ro.build.version.incremental`
Android=`getprop ro.build.version.release`
CPU_ABI=`getprop ro.product.cpu.abi`
CommonPath=$MODPATH/common

print_modname() {
  ui_print ""
  ui_print "╔═╗╔═╗╔═╗  ╦ ╦╔╗╔╦  ╔═╗╔═╗╦╔═╔═╗╦═╗"
  ui_print "╠╣ ╠═╝╚═╗  ║ ║║║║║  ║ ║║  ╠╩╗║╣ ╠╦╝"
  ui_print "╚  ╩  ╚═╝  ╚═╝╝╚╝╩═╝╚═╝╚═╝╩ ╩╚═╝╩╚═"
  ui_print ""
  ui_print "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓"
  ui_print "┃       ⚡ SUPERCHARGE YOUR GAMING EXPERIENCE ⚡   ┃"
  ui_print "┃           Developed by @yadavnikhil03             ┃"
  ui_print "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛"
  ui_print ""
  ui_print "📱 DEVICE PROFILE 📱"
  ui_print "━━━━━━━━━━━━━━━━━━━━"
  ui_print "  ➤ Device: $Market_Name"
  ui_print "  ➤ Model: $Model"
  ui_print "  ➤ Codename: $Device"
  ui_print "  ➤ Android: $Android"
  ui_print "  ➤ Build: $Version"
  ui_print "  ➤ Architecture: $CPU_ABI"
  ui_print ""
  ui_print "⚙️ INITIALIZING MODULE INSTALLATION ⚙️"
  ui_print "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  ui_print ""
}

print_modname

on_install() {
  ui_print " ▶️ Extracting module files..."
  unzip -o "$ZIPFILE" 'system/*' -d $MODPATH >&2
}

set_permissions() {
  ui_print " ▶️ Setting appropriate permissions..."
  set_perm_recursive  $MODPATH  0  0  0755  0644
}

ui_print " ▶️ Moving files to destination..."
mv  ${CommonPath}/*  $MODPATH
rm  -rf ${CommonPath}

ui_print ""
ui_print " ✅ INSTALLATION COMPLETED SUCCESSFULLY ✅"
ui_print " 🔄 Reboot your device to apply changes 🔄"
ui_print ""
ui_print "┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓"
ui_print "┃  Thank you for using FPS Unlocker - Enjoy gaming!  ┃"
ui_print "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛"
ui_print ""

sleep 1
coolapkTesting=`pm list package | grep -w 'com.coolapk.market'`
  if [[ "$coolapkTesting" != "" ]];then
  am start -d 'coolmarket://u/9960587' >/dev/null 2>&1
  fi