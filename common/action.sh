#!/system/bin/sh
MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

echo "=========================================="
echo "    GameUnlocker App Manager (Magisk)     "
echo "=========================================="
echo "Starting WebUI Configuration..."

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
