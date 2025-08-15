#!/system/bin/sh

MODDIR=${0%/*}

# ===== FPS Unlock =====
setprop debug.vendor.qti.game.fps 120
setprop persist.vendor.qti.game.fps 120
setprop ro.vendor.display.enable_fps_switch 1

# ===== Thermal & Performance =====
setprop persist.vendor.thermal.engine.disable 1
setprop vendor.gpu.mode performance

# Lower texture quality
setprop vendor.gfx.low_quality 1
setprop vendor.gfx.disable_high_res_textures 1

# Disable high-quality shadows
setprop vendor.gfx.disable_shadows 1

# Lower rendering resolution scaling
setprop debug.hwui.renderer opengl
setprop debug.hwui.render_dirty_regions false
setprop debug.hwui.disable_vsync true
setprop persist.sys.sf.lcd_density 220  
setprop vendor.gfx.anisotropic_filtering 0

# ===== Input Lag Tweaks =====
setprop touch.vendor.sampling_rate 240

# Log
log -p i -t GameUnlocker "Applied FPS unlock + potato graphics mode"
