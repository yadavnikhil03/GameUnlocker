#!/system/bin/sh

MODDIR=${0%/*}
LOG_TAG="GameUnlocker/PostFS"

_log() { log -p i -t "$LOG_TAG" "$1" 2>/dev/null || true; }
_logw() { log -p w -t "$LOG_TAG" "$1" 2>/dev/null || true; }

_log "post-fs-data: starting (pid=$$)"

rm -f "$MODDIR/auth_token" 2>/dev/null

safe_resetprop() {
    local name="$1" val="$2"

    if command -v resetprop >/dev/null 2>&1; then
        resetprop "$name" "$val" 2>/dev/null
    else
        setprop "$name" "$val" 2>/dev/null
    fi
}

resetprop_delete() {

    if command -v resetprop >/dev/null 2>&1; then
        resetprop --delete "$1" 2>/dev/null || true
    fi
}

getprop_val() { getprop "$1" 2>/dev/null; }

HARDWARE=$(getprop_val ro.hardware)
BOARD_PLATFORM=$(getprop_val ro.board.platform)
SOC_MODEL=$(getprop_val ro.soc.model)
ANDROID_VER=$(getprop_val ro.build.version.sdk)

_log "Device: hw=$HARDWARE board=$BOARD_PLATFORM soc=$SOC_MODEL api=$ANDROID_VER"

is_qualcomm() {

    case "$HARDWARE" in
        qcom|*kalama*|*taro*|*lahaina*|*shima*|*crow*|*cape*|*uksi*|*napa*|*pineapple*|*sun*) return 0 ;;
    esac

    case "$BOARD_PLATFORM" in
        sm8*|sm7*|sm6*|sdm*|msm*|qcom*) return 0 ;;
    esac

    case "$SOC_MODEL" in
        SM8*|SM7*|SM6*|SDM*|MSM*) return 0 ;;
    esac
    return 1
}

is_mediatek() {

    case "$HARDWARE" in
        *mt*|*mediatek*) return 0 ;;
    esac

    case "$BOARD_PLATFORM" in
        mt*) return 0 ;;
    esac
    return 1
}

is_exynos() {

    case "$HARDWARE" in
        *exynos*|*s5e*) return 0 ;;
    esac
    return 1
}

_log "Injecting FPS properties"

safe_resetprop debug.vendor.qti.game.fps 120
safe_resetprop persist.vendor.qti.game.fps 120

safe_resetprop persist.vendor.miui.game.mode 1
safe_resetprop vendor.xiaomi.game.enabled 1 2>/dev/null || true

safe_resetprop ro.config.ringtone_game 1 2>/dev/null || true

safe_resetprop ro.surface_flinger.use_content_detection_for_refresh_rate true
safe_resetprop ro.surface_flinger.set_touch_timer_ms 200
safe_resetprop ro.surface_flinger.set_display_power_timer_ms 1000

_log "Injecting GPU/renderer properties"

safe_resetprop debug.hwui.renderer skiavk
safe_resetprop debug.hwui.use_hint_manager true
safe_resetprop debug.hwui.target_cpu_time_percent 66

safe_resetprop debug.egl.hw 1
safe_resetprop debug.egl.profiler 0

safe_resetprop debug.vulkan.layers ""
safe_resetprop debug.vulkan.renderdoc 0
safe_resetprop debug.renderengine.backend skiagl

safe_resetprop debug.sf.disable_backpressure 0
safe_resetprop debug.sf.enable_hwc_vds 0

if is_qualcomm; then
    _log "Injecting Qualcomm GPU props"
    safe_resetprop vendor.display.disable_rotator_downscale 1
    safe_resetprop vendor.display.enable_async_vds_creation 1
    safe_resetprop vendor.display.comp_mask 0
    safe_resetprop vendor.gralloc.disable_ubwc 0  # Keep UBWC for bandwidth
    safe_resetprop vendor.gpu.available_frequencies ""
fi

if is_mediatek; then
    _log "Injecting MediaTek GPU props"
    safe_resetprop vendor.mtk.gpu.gaming.mode 1
    safe_resetprop debug.mtk.gpu.performance 1 2>/dev/null || true
fi

_log "Injecting VM/memory properties"

safe_resetprop dalvik.vm.heapstartsize 16m
safe_resetprop dalvik.vm.heapgrowthlimit 256m
safe_resetprop dalvik.vm.heapsize 512m
safe_resetprop dalvik.vm.heaptargetutilization 0.75
safe_resetprop dalvik.vm.heapminfree 512k
safe_resetprop dalvik.vm.heapmaxfree 8m

safe_resetprop dalvik.vm.gctype CMS 2>/dev/null || true

_log "Injecting ART compilation properties"

safe_resetprop dalvik.vm.usejitprofiles true
safe_resetprop dalvik.vm.usejit true

_log "Injecting build classification props"

safe_resetprop ro.build.type user 2>/dev/null || true
safe_resetprop ro.build.tags release-keys 2>/dev/null || true

safe_resetprop debug.atrace.tags.enableflags 0
safe_resetprop debug.sf.recomputecrop 0

_log "post-fs-data: completed"
