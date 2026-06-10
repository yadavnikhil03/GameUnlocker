#!/system/bin/sh
echo "Content-Type: application/json"
echo ""

MODDIR="/data/adb/modules/Game-Unlocker"
CONFIG_FILE="$MODDIR/config.json"

ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2)
PKG=$(echo "$QUERY_STRING" | grep -o 'pkg=[^&]*' | cut -d= -f2)

urldecode() {
  local data="$1"
  data=${data//+/ }
  printf '%b' "${data//%/\\x}"
}

is_valid_package() {
  echo "$1" | grep -Eq '^[a-zA-Z][a-zA-Z0-9_]*(\.[a-zA-Z0-9_]+)+$'
}

if [ "$ACTION" = "get_config" ]; then
    cat "$CONFIG_FILE"
elif [ "$ACTION" = "add_game" ]; then
    if [ -n "$PKG" ]; then
    PKG=$(urldecode "$PKG")
    if ! is_valid_package "$PKG"; then
      echo "{\"success\": false, \"error\": \"invalid package\"}"
      exit 0
    fi

    if grep -q "\"$PKG\"" "$CONFIG_FILE"; then
      echo "{\"success\": true, \"message\": \"already exists\"}"
      exit 0
    fi

    sed -i 's/"PACKAGES_SAMSUNG_S24_ULTRA": \[/"PACKAGES_SAMSUNG_S24_ULTRA": \[\n    "'"$PKG"'",/' "$CONFIG_FILE"
    echo "{\"success\": true}"
    else
        echo "{\"success\": false}"
    fi
elif [ "$ACTION" = "reset_config" ]; then
    cat <<EOF > "$CONFIG_FILE"
{
  "PACKAGES_SAMSUNG_S24_ULTRA": [
    "com.tencent.ig",
    "com.pubg.imobile",
    "com.pubg.imobilelite",
    "com.pubg.imobile.india",
    "com.pubg.imobile.battlegroundsindia",
    "com.pubg.imobile.in",
    "com.pubg.imidas",
    "com.pubg.krmobile",
    "com.vng.pubgmobile",
    "com.rekoo.pubgm",
    "com.tencent.tmgp.pubgmhd",
    "com.riotgames.league.wildrift",
    "com.riotgames.league.wildrifttw",
    "com.riotgames.league.wildriftvn",
    "com.riotgames.valormobile"
  ],
  "PACKAGES_SAMSUNG_S24_ULTRA_DEVICE": {
    "MANUFACTURER": "Samsung",
    "BRAND": "samsung",
    "MODEL": "SM-S928B",
    "DEVICE": "e3q",
    "PRODUCT": "e3qxx",
    "FINGERPRINT": "samsung/e3qxx/e3q:14/UP1A.231005.007/S928BXXS1AXBG:user/release-keys"
  },
  "cpu_spoof": {
    "with_cpu": [
      "com.epicgames.fortnite",
      "com.ea.gp.apexlegendsmobilefps"
    ]
  }
}
EOF
    echo "{\"success\": true}"
elif [ "$ACTION" = "get_apps" ]; then
    APPS=$(pm list packages -3 | cut -f 2 -d ":" | tr '\n' ',')
    echo "{\"success\": true, \"apps\": \"$APPS\"}"
elif [ "$ACTION" = "get_device_info" ]; then
    MODEL=$(getprop ro.product.model)
    ANDROID=$(getprop ro.build.version.release)
    echo "{\"success\": true, \"model\": \"$MODEL\", \"android\": \"$ANDROID\"}"
elif [ "$ACTION" = "generate_logs" ]; then
    logcat -d -s GameUnlocker > /sdcard/Download/GameUnlocker_Logs.txt
    SNIPPET=$(logcat -d -s GameUnlocker -t 20)
    echo "{\"success\": true, \"snippet\": \"$(echo "$SNIPPET" | base64 -w 0)\"}"
else
    echo "{\"error\": \"invalid action\"}"
fi
