#!/system/bin/sh
MODDIR=${0%/*}

umount /proc/cpuinfo 2>/dev/null

rm -rf /data/adb/modules/Game-Unlocker
rm -f /sdcard/Download/GameUnlocker_Logs.txt

resetprop -p --delete persist.vendor.qti.game.fps
resetprop -p --delete persist.vendor.thermal.engine.disable

if [ -f "$INFO" ]; then
  while read LINE; do
    if [ "$(echo -n "$LINE" | tail -c 1)" == "~" ]; then
      continue
    elif [ -f "${LINE}~" ]; then
      mv -f "${LINE}~" "$LINE"
    else
      rm -f "$LINE"
    fi
  done < "$INFO"
fi
