#!/system/bin/sh

MODDIR=${0%/*}
CONFIG_FILE="$MODDIR/config.json"

until [ "$(getprop sys.boot_completed)" = "1" ]; do
    sleep 2
done

sleep 15

setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1
setprop touch.vendor.sampling_rate 240

while true; do
    FOREGROUND_APP=$(dumpsys window | grep -E 'mCurrentFocus' | cut -d '/' -f 1 | rev | cut -d ' ' -f 1 | rev)
    
    if grep -q "\"$FOREGROUND_APP\"" "$CONFIG_FILE"; then
        if [ "$(getprop persist.vendor.thermal.engine.disable)" != "1" ]; then  
            log -p i -t GameUnlocker "Game Detected ($FOREGROUND_APP) - Applying Performance Mode"

            setprop persist.vendor.thermal.engine.disable 1
            setprop vendor.gpu.mode performance
            setprop vendor.gfx.low_quality 1
            setprop vendor.gfx.disable_high_res_textures 1
            setprop vendor.gfx.disable_shadows 1
            setprop debug.hwui.renderer opengl
            setprop debug.hwui.disable_vsync true
            setprop vendor.gfx.anisotropic_filtering 0
        fi
    else
        if [ "$(getprop persist.vendor.thermal.engine.disable)" == "1" ]; then  
            log -p i -t GameUnlocker "Game Exited - Restoring Normal Mode"

            setprop persist.vendor.thermal.engine.disable 0
            setprop vendor.gpu.mode normal
            setprop vendor.gfx.low_quality 0
            setprop vendor.gfx.disable_high_res_textures 0
            setprop vendor.gfx.disable_shadows 0
            setprop debug.hwui.renderer skia
            setprop debug.hwui.disable_vsync false
            setprop vendor.gfx.anisotropic_filtering 1
        fi
    fi
