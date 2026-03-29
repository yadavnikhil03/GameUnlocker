#!/system/bin/sh
echo "Content-Type: application/json"
echo ""

MODDIR="/data/adb/modules/Game-Unlocker"
CONFIG_FILE="$MODDIR/config.json"

ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2)
PKG=$(echo "$QUERY_STRING" | grep -o 'pkg=[^&]*' | cut -d= -f2)

if [ "$ACTION" = "get_config" ]; then
    cat "$CONFIG_FILE"
elif [ "$ACTION" = "add_game" ]; then
    if [ -n "$PKG" ]; then
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
    "MANUFACTURER": "vivo",
    "BRAND": "vivo",
    "MODEL": "V2302A",
    "DEVICE": "V2302A",
    "PRODUCT": "V2302A",
    "FINGERPRINT": "vivo/V2302A/V2302A:13/TP1A.220624.014/compil02251912:user/release-keys"
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
