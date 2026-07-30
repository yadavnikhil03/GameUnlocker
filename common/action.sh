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

if pm list packages | grep -q "io.github.a13e300.ksuwebui"; then
    echo "Launching natively inside KSUWebUI App..."
    su -c "am start -n \"io.github.a13e300.ksuwebui/.WebUIActivity\" -e id \"Game-Unlocker\""
    exit 0
fi

BB=$(find_busybox)
if [ -z "$BB" ]; then
    echo "Error: Busybox not found! Cannot start WebUI."
    exit 1
fi

RANDOM_PORT=$(generate_random_port)

# Generate a secure 16-byte hex token
AUTH_TOKEN=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
echo "$AUTH_TOKEN" > "$MODDIR/auth_token"
chmod 0600 "$MODDIR/auth_token"

chmod -R 0755 "$MODDIR/webroot/cgi-bin"

BB_DIR=$($BB dirname "$BB")
export PATH="$BB_DIR:$PATH"

"$BB" pkill -f "httpd -p 127.0.0.1:" >/dev/null 2>&1

echo "Starting background server and opening browser..."

(
    "$BB" httpd -p 127.0.0.1:$RANDOM_PORT -h "$MODDIR/webroot" >/dev/null 2>&1
    sleep 300
    "$BB" pkill -f "httpd -p 127.0.0.1:$RANDOM_PORT" >/dev/null 2>&1
    rm -f "$MODDIR/auth_token"
) &

sleep 1
am start -a android.intent.action.VIEW -d "http://127.0.0.1:$RANDOM_PORT?token=$AUTH_TOKEN" >/dev/null 2>&1

echo ""
echo "Done! The WebUI should now be open."
exit 0
