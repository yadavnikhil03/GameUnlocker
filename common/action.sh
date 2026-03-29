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

# Ensure CGI-BIN executing permissions
chmod -R 0755 "$MODDIR/webroot/cgi-bin"

# Export PATH to ensure inner scripts use this valid busybox environment
BB_DIR=$($BB dirname "$BB")
export PATH="$BB_DIR:$PATH"

# Clean old instances if still running
"$BB" pkill -f "httpd -p 127.0.0.1:" >/dev/null 2>&1

echo "Starting background server on port $RANDOM_PORT..."

# Launch busybox httpd bound to localhost exactly
"$BB" httpd -p 127.0.0.1:$RANDOM_PORT -h "$MODDIR/webroot" >/dev/null 2>&1

echo "Redirecting to browser..."
sleep 1
am start -a android.intent.action.VIEW -d "http://127.0.0.1:$RANDOM_PORT" >/dev/null 2>&1

echo ""
echo "Server will run in the background for 5 minutes."
echo "You may safely close this terminal or swipe it away."

# Sleep so the WebUI works locally, then automatically clean up the HTTP server to free memory
sleep 300
"$BB" pkill -f "httpd -p 127.0.0.1:$RANDOM_PORT" >/dev/null 2>&1
exit 0
