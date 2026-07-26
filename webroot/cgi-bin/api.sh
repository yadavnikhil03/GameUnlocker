#!/system/bin/sh
echo "Content-Type: application/json"
echo ""

MODDIR="/data/adb/modules/Game-Unlocker"
CONFIG_FILE="$MODDIR/config.json"

# Parse query string params
ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2)
PKG=$(echo "$QUERY_STRING" | grep -o 'pkg=[^&]*' | cut -d= -f2)
PROFILE=$(echo "$QUERY_STRING" | grep -o 'profile=[^&]*' | cut -d= -f2)
FIELD=$(echo "$QUERY_STRING" | grep -o 'field=[^&]*' | cut -d= -f2)
VALUE=$(echo "$QUERY_STRING" | grep -o 'value=[^&]*' | cut -d= -f2)

urldecode() {
  local data="$1"
  data=${data//+/ }
  printf '%b' "${data//%/\\x}"
}

is_valid_package() {
  echo "$1" | grep -Eq '^[a-zA-Z][a-zA-Z0-9_]*(\.[a-zA-Z0-9_]+)+$'
}

is_valid_profile_name() {
  echo "$1" | grep -Eq '^[A-Z][A-Z0-9_]*$'
}

# -----------------------------------------------------------------------
# get_config — return full config.json
# -----------------------------------------------------------------------
if [ "$ACTION" = "get_config" ]; then
    if [ -f "$CONFIG_FILE" ]; then
        cat "$CONFIG_FILE"
    else
        echo '{"error": "config.json not found"}'
    fi

# -----------------------------------------------------------------------
# list_profiles — return array of profile names from config
# -----------------------------------------------------------------------
elif [ "$ACTION" = "list_profiles" ]; then
    if ! command -v jq > /dev/null 2>&1; then
        echo '{"success": false, "error": "jq not available"}'
        exit 0
    fi
    PROFILES=$(jq -r '[.profiles | keys[]]' "$CONFIG_FILE" 2>/dev/null)
    echo "{\"success\": true, \"profiles\": $PROFILES}"

# -----------------------------------------------------------------------
# add_game — add a package to routing_rules for a given profile
# Uses jq for safe JSON manipulation (no more fragile sed)
# -----------------------------------------------------------------------
elif [ "$ACTION" = "add_game" ]; then
    if [ -z "$PKG" ] || [ -z "$PROFILE" ]; then
        echo '{"success": false, "error": "missing pkg or profile"}'
        exit 0
    fi
    PKG=$(urldecode "$PKG")
    PROFILE=$(urldecode "$PROFILE")

    if ! is_valid_package "$PKG"; then
        echo '{"success": false, "error": "invalid package name"}'
        exit 0
    fi
    if ! command -v jq > /dev/null 2>&1; then
        echo '{"success": false, "error": "jq not available on this device"}'
        exit 0
    fi

    # Check if package already has a routing rule
    EXISTING=$(jq -r --arg pkg "$PKG" \
        '[.routing_rules[] | select(.pattern == $pkg)] | length' \
        "$CONFIG_FILE" 2>/dev/null)
    if [ "$EXISTING" != "0" ] && [ -n "$EXISTING" ]; then
        echo '{"success": true, "message": "package already has a routing rule"}'
        exit 0
    fi

    # Check that the profile actually exists
    PROFILE_EXISTS=$(jq -r --arg p "$PROFILE" \
        '.profiles | has($p)' "$CONFIG_FILE" 2>/dev/null)
    if [ "$PROFILE_EXISTS" != "true" ]; then
        echo "{\"success\": false, \"error\": \"profile '$PROFILE' not found\"}"
        exit 0
    fi

    # Safely append a new routing rule using jq
    TMP=$(mktemp)
    jq --arg pkg "$PKG" --arg prof "$PROFILE" \
        '.routing_rules += [{"type": "exact", "pattern": $pkg, "profile": $prof, "priority": 50}]' \
        "$CONFIG_FILE" > "$TMP" && mv "$TMP" "$CONFIG_FILE"
    chmod 0644 "$CONFIG_FILE"
    echo '{"success": true}'

# -----------------------------------------------------------------------
# remove_game — remove all routing rules for a package
# -----------------------------------------------------------------------
elif [ "$ACTION" = "remove_game" ]; then
    if [ -z "$PKG" ]; then
        echo '{"success": false, "error": "missing pkg"}'
        exit 0
    fi
    PKG=$(urldecode "$PKG")
    if ! is_valid_package "$PKG"; then
        echo '{"success": false, "error": "invalid package name"}'
        exit 0
    fi
    if ! command -v jq > /dev/null 2>&1; then
        echo '{"success": false, "error": "jq not available"}'
        exit 0
    fi

    TMP=$(mktemp)
    jq --arg pkg "$PKG" \
        '.routing_rules = [.routing_rules[] | select(.pattern != $pkg)]' \
        "$CONFIG_FILE" > "$TMP" && mv "$TMP" "$CONFIG_FILE"
    chmod 0644 "$CONFIG_FILE"
    echo '{"success": true}'

# -----------------------------------------------------------------------
# add_profile — add or update a device profile in config.json
# Expects POST body or query params with profile fields
# Minimal required: name, MANUFACTURER, BRAND, MODEL, DEVICE, PRODUCT, FINGERPRINT
# -----------------------------------------------------------------------
elif [ "$ACTION" = "add_profile" ]; then
    # Read POST body for profile JSON (safer than query string for large data)
    BODY=$(cat)
    if [ -z "$BODY" ]; then
        echo '{"success": false, "error": "no POST body"}'
        exit 0
    fi
    if ! command -v jq > /dev/null 2>&1; then
        echo '{"success": false, "error": "jq not available"}'
        exit 0
    fi

    PROFILE_NAME=$(echo "$BODY" | jq -r '.name // empty' 2>/dev/null)
    if [ -z "$PROFILE_NAME" ]; then
        echo '{"success": false, "error": "missing profile name"}'
        exit 0
    fi
    if ! is_valid_profile_name "$PROFILE_NAME"; then
        echo '{"success": false, "error": "profile name must be uppercase alphanumeric with underscores"}'
        exit 0
    fi

    PROFILE_DATA=$(echo "$BODY" | jq 'del(.name)' 2>/dev/null)
    if [ -z "$PROFILE_DATA" ]; then
        echo '{"success": false, "error": "invalid profile data"}'
        exit 0
    fi

    TMP=$(mktemp)
    jq --arg name "$PROFILE_NAME" --argjson data "$PROFILE_DATA" \
        '.profiles[$name] = $data' \
        "$CONFIG_FILE" > "$TMP" && mv "$TMP" "$CONFIG_FILE"
    chmod 0644 "$CONFIG_FILE"
    echo '{"success": true}'

# -----------------------------------------------------------------------
# delete_profile — remove a profile and all its routing rules
# -----------------------------------------------------------------------
elif [ "$ACTION" = "delete_profile" ]; then
    if [ -z "$PROFILE" ]; then
        echo '{"success": false, "error": "missing profile"}'
        exit 0
    fi
    PROFILE=$(urldecode "$PROFILE")
    if ! command -v jq > /dev/null 2>&1; then
        echo '{"success": false, "error": "jq not available"}'
        exit 0
    fi

    TMP=$(mktemp)
    jq --arg prof "$PROFILE" \
        'del(.profiles[$prof]) |
         .routing_rules = [.routing_rules[] | select(.profile != $prof)]' \
        "$CONFIG_FILE" > "$TMP" && mv "$TMP" "$CONFIG_FILE"
    chmod 0644 "$CONFIG_FILE"
    echo '{"success": true}'

# -----------------------------------------------------------------------
# get_apps_async — list installed apps asynchronously
# -----------------------------------------------------------------------
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
            # Fallback: use pm list packages
            APPS_STR=$(pm list packages -3 | cut -f2 -d: | sort | tr '\n' ',')
            JSON_ARR="["
            first=true
            IFS=','
            for p in $APPS_STR; do
                [ -z "$p" ] && continue
                if [ "$first" = true ]; then
                    first=false
                else
                    JSON_ARR="$JSON_ARR,"
                fi
                LABEL=$(pm dump "$p" 2>/dev/null | grep 'labelRes=' | head -1 | grep -o 'label=[^ ]*' | cut -d= -f2)
                [ -z "$LABEL" ] && LABEL="$p"
                JSON_ARR="$JSON_ARR{\"package\":\"$p\",\"name\":\"$LABEL\"}"
            done
            unset IFS
            JSON_ARR="$JSON_ARR]"
            echo "{\"success\": true, \"apps\": $JSON_ARR}" > /data/local/tmp/gameunlocker_apps.json
        fi
    ) >/dev/null 2>&1 &
    echo '{"success": true, "message": "fetching"}'

# -----------------------------------------------------------------------
# poll_apps — check if async app list is ready
# -----------------------------------------------------------------------
elif [ "$ACTION" = "poll_apps" ]; then
    if [ -f /data/local/tmp/gameunlocker_apps.json ]; then
        cat /data/local/tmp/gameunlocker_apps.json
    else
        echo '{"success": false, "message": "not_ready"}'
    fi

# -----------------------------------------------------------------------
# get_device_info — real device model + active spoof status
# -----------------------------------------------------------------------
elif [ "$ACTION" = "get_device_info" ]; then
    MODEL=$(getprop ro.product.model)
    BRAND=$(getprop ro.product.brand)
    ANDROID=$(getprop ro.build.version.release)
    SOC=$(getprop ro.soc.model)
    [ -z "$SOC" ] && SOC=$(getprop ro.board.platform)
    ACTIVE_GAMES=$(jq -r '[.routing_rules[].pattern] | length' "$CONFIG_FILE" 2>/dev/null || echo "0")
    echo "{\"success\": true, \"model\": \"$MODEL\", \"brand\": \"$BRAND\", \"android\": \"$ANDROID\", \"soc\": \"$SOC\", \"active_games\": $ACTIVE_GAMES}"

# -----------------------------------------------------------------------
# generate_logs — dump recent GameUnlocker logcat output
# -----------------------------------------------------------------------
elif [ "$ACTION" = "generate_logs" ]; then
    SNIPPET=$(logcat -d -s GameUnlocker -t 50 2>/dev/null)
    logcat -d -s GameUnlocker > /sdcard/Download/GameUnlocker_Logs.txt 2>/dev/null
    echo "{\"success\": true, \"snippet\": \"$(echo "$SNIPPET" | base64 -w 0 2>/dev/null || echo "$SNIPPET" | base64)\"}"

else
    echo '{"error": "invalid action"}'
fi
