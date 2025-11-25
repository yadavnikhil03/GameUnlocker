#!/system/bin/sh

MODDIR=${0%/*}
sleep 15

# ===== FPS Unlock (Android 12+ Compatible) =====
setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1
setprop debug.sf.hw 1
setprop debug.performance.tuning 1
setprop video.accelerate.hw 1

# ===== Safe Performance Optimizations =====
setprop debug.composition.type gpu
setprop persist.sys.use_dithering 0
setprop debug.egl.profiler 1
setprop debug.egl.hw 1

# ===== GPU Performance =====
setprop debug.gr.numframebuffers 3
setprop debug.overlayui.enable 1
setprop persist.sys.ui.hw 1

# ===== Reduce Input Lag =====
setprop touch.vendor.sampling_rate 240
setprop touch.presure.scale 0.001

# Log to Magisk
log -p i -t GameUnlocker "[v1.1.0] Applied Android 12 compatible FPS unlock"
