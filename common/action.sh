#!/system/bin/sh
MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

echo "=========================================="
echo "    GameUnlocker App Manager (Magisk)     "
echo "=========================================="
echo ""
echo "Select an action:"
echo "  1) Open WebUI to Manage Games"
echo "  2) Update Default Config (Requires Internet)"
echo "  3) Cancel"
echo ""
read -p "Enter choice (1-3): " choice

case "$choice" in
  2)
    echo "Downloading latest config..."
    REMOTE_CONFIG="https://raw.githubusercontent.com/yadavnikhil03/GameUnlocker/main/common/config.json"
    if command -v curl >/dev/null 2>&1; then
        curl -s -L "$REMOTE_CONFIG" -o "$MODDIR/config_temp.json"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$MODDIR/config_temp.json" "$REMOTE_CONFIG"
    else
        echo "Error: curl or wget not found."
        exit 1
    fi
    
    if [ -f "$MODDIR/config_temp.json" ] && grep -q "{" "$MODDIR/config_temp.json"; then
        mv "$MODDIR/config_temp.json" "$CONFIG_FILE"
        chmod 0644 "$CONFIG_FILE"
        echo "Config updated successfully! Reboot to apply."
    else
        echo "Failed to download valid config."
        rm -f "$MODDIR/config_temp.json"
    fi
    exit 0
    ;;
  3)
    echo "Cancelled."
    exit 0
    ;;
  *)
    echo "Starting WebUI Configuration..."
    ;;
esac

find_busybox() {
    for candidate in /data/adb/ksu/bin/busybox /data/adb/magisk/busybox /data/adb/ap/bin/busybox /system/bin/busybox; do
        if [ -f "$candidate" ] && [ -x "$candidate" ]; then
            if "$candidate" true >/dev/null 2>&1; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    if command -v busybox >/dev/null 2>&1; then
        sys_bb=$(command -v busybox)
        if "$sys_bb" true >/dev/null 2>&1; then
            echo "$sys_bb"
            return 0
        fi
    fi
    return 1
}

generate_random_port() {
    if [ -c "/dev/urandom" ]; then
        PORT=$(od -An -N2 -tu2 /dev/urandom | tr -d ' ')
        PORT=$((6000 + (PORT % 4000)))
    else
        PORT=$((6000 + ($(date +%s) % 4000)))
    fi
    echo "$PORT"
}

BB=$(find_busybox)
if [ -z "$BB" ]; then
    echo "Error: Busybox not found! Cannot start WebUI."
    exit 1
fi

RANDOM_PORT=$(generate_random_port)

chmod -R 0755 "$MODDIR/webroot/cgi-bin"

BB_DIR=$($BB dirname "$BB")
export PATH="$BB_DIR:$PATH"

"$BB" pkill -f "httpd -p 127.0.0.1:" >/dev/null 2>&1

echo "Starting background server and opening WebUI..."

(
    "$BB" httpd -p 127.0.0.1:$RANDOM_PORT -h "$MODDIR/webroot" >/dev/null 2>&1
    sleep 300
    "$BB" pkill -f "httpd -p 127.0.0.1:$RANDOM_PORT" >/dev/null 2>&1
) &

echo "Redirecting to browser..."
sleep 1
am start -a android.intent.action.VIEW -d "http://127.0.0.1:$RANDOM_PORT" >/dev/null 2>&1

echo ""
echo "Done! You can now use the WebUI in your browser."
exit 0
