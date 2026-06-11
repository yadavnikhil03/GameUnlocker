#!/system/bin/sh
echo "Content-Type: application/json"
echo ""

MODDIR="/data/adb/modules/Game-Unlocker"
CONFIG_FILE="$MODDIR/config.json"

ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2)
PKG=$(echo "$QUERY_STRING" | grep -o 'pkg=[^&]*' | cut -d= -f2)
PROFILE=$(echo "$QUERY_STRING" | grep -o 'profile=[^&]*' | cut -d= -f2)

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
    if [ -n "$PKG" ] && [ -n "$PROFILE" ]; then
        PKG=$(urldecode "$PKG")
        if ! is_valid_package "$PKG"; then
            echo "{\"success\": false, \"error\": \"invalid package\"}"
            exit 0
        fi

        if grep -q "\"$PKG\"" "$CONFIG_FILE"; then
            echo "{\"success\": true, \"message\": \"already exists\"}"
            exit 0
        fi
        sed -i 's/"'"$PROFILE"'": \[/"'"$PROFILE"'": \[\n    "'"$PKG"'",/' "$CONFIG_FILE"
        echo "{\"success\": true}"
    else
        echo "{\"success\": false, \"error\": \"missing args\"}"
    fi
elif [ "$ACTION" = "remove_game" ]; then
    if [ -n "$PKG" ]; then
        PKG=$(urldecode "$PKG")
        if ! is_valid_package "$PKG"; then
            echo "{\"success\": false, \"error\": \"invalid package\"}"
            exit 0
        fi
        sed -i '/"'"$PKG"'"/d' "$CONFIG_FILE"
        echo "{\"success\": true}"
    else
        echo "{\"success\": false, \"error\": \"missing args\"}"
    fi
elif [ "$ACTION" = "reset_config" ]; then
    echo "{\"success\": false, \"error\": \"Reset disabled, reinstall module instead\"}"
elif [ "$ACTION" = "get_apps_async" ]; then
    rm -f /data/local/tmp/gameunlocker_apps.json
    (
        APPS_JSON=""
        if [ -f "$MODDIR/common/get_apps.dex" ]; then
            export CLASSPATH="$MODDIR/common/get_apps.dex"
            APPS_JSON=$(app_process /system/bin AppList 2>/dev/null)
        fi
        
        if echo "$APPS_JSON" | grep -q '\[.*\]'; then
            echo "{\"success\": true, \"apps\": $APPS_JSON}" > /data/local/tmp/gameunlocker_apps.json
        else
            APPS_STR=$(pm list packages -3 | cut -f 2 -d ":" | tr '\n' ',' | sed 's/,$//')
            JSON_ARR="["
            IFS=','
            first=true
            for p in $APPS_STR; do
                if [ "$first" = true ]; then
                    first=false
                else
                    JSON_ARR="$JSON_ARR, "
                fi
                JSON_ARR="$JSON_ARR{\"package\":\"$p\",\"name\":\"$p\"}"
            done
            JSON_ARR="$JSON_ARR]"
            echo "{\"success\": true, \"apps\": $JSON_ARR}" > /data/local/tmp/gameunlocker_apps.json
        fi
    ) >/dev/null 2>&1 &
    echo "{\"success\": true, \"message\": \"fetching\"}"
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
