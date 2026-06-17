#!/system/bin/sh
MODDIR=${0%/*}

if command -v resetprop >/dev/null; then
    resetprop -p --delete vendor.gpu.mode
    resetprop -p --delete vendor.gfx.low_quality
fi
