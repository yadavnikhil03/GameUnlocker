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

    sed -i 's/"PACKAGES_iQOO_Neo8_Pro": \[/"PACKAGES_iQOO_Neo8_Pro": \[\n    "'"$PKG"'",/' "$CONFIG_FILE"
    echo "{\"success\": true}"
    else
        echo "{\"success\": false}"
    fi
elif [ "$ACTION" = "reset_config" ]; then
    cat <<EOF > "$CONFIG_FILE"
{
  "PACKAGES_iQOO_Neo8_Pro": [
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
    "com.riotgames.league.wildriftvn"
  ],
  "PACKAGES_iQOO_Neo8_Pro_DEVICE": {
    "MANUFACTURER": "Asus",
    "BRAND": "asus",
    "MODEL": "ZS673KS",
    "DEVICE": "ASUS_I005_1",
    "PRODUCT": "WW_I005D",
    "FINGERPRINT": "asus/WW_I005D/ASUS_I005_1:11/RKQ1.201022.002/18.0840.2103.26-0:user/release-keys"
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
else
    echo "{\"error\": \"invalid action\"}"
fi
