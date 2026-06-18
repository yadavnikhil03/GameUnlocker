#!/system/bin/sh
# Runs on module removal. Restore the GPU performance hints that the late-start
# service toggles back to their defaults. We set explicit defaults rather than
# deleting the properties: a deleted prop may resolve to an empty value that
# differs from what the vendor expects, so "normal"/"0" is the safe, explicit
# restoration.

if command -v resetprop >/dev/null; then
    resetprop -p vendor.gpu.mode normal
    resetprop -p vendor.gfx.low_quality 0
fi
