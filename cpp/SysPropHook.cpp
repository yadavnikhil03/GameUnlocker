#include "SysPropHook.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"
#include <string.h>
#include <string_view>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <bytehook.h>

namespace gameunlocker {

// ----------------------------------------------------------------
// Static profile state
// ----------------------------------------------------------------

std::optional<DeviceProfile> SysPropHook::activeProfile_ = std::nullopt;
bool SysPropHook::cpuSpoofOnly_ = false;

void SysPropHook::setProfile(const std::optional<DeviceProfile>& profile, bool cpuSpoofOnly) {
    activeProfile_ = profile;
    cpuSpoofOnly_ = cpuSpoofOnly;
}

std::optional<DeviceProfile> SysPropHook::getProfile() {
    return activeProfile_;
}

bool SysPropHook::isCpuSpoofOnly() {
    return cpuSpoofOnly_;
}

// ----------------------------------------------------------------
// Helper: derive SDK int string from Android version string
// ----------------------------------------------------------------

static const char* sdkFromVersion(const std::string& ver) {
    if (ver == "16") return "36";
    if (ver == "15") return "35";
    if (ver == "14") return "34";
    if (ver == "13") return "33";
    if (ver == "12") return "32";
    if (ver == "11") return "30";
    if (ver == "10") return "29";
    return "34";
}

// ----------------------------------------------------------------
// Core spoof logic
//
// This follows the PROVEN PlayIntegrityFix pattern:
//  - Single hook on __system_property_read_callback only
//  - std::string_view suffix matching — partition-agnostic
//  - Returns true + sets outValue if this prop should be overridden
// ----------------------------------------------------------------

static bool getSpoofedValue(const char* name, std::string& outValue) {
    if (!name) return false;

    std::string_view prop(name);

    auto profileOpt = SysPropHook::getProfile();
    const bool hasProfile = profileOpt.has_value();
    const bool cpuOnly    = SysPropHook::isCpuSpoofOnly();

    if (!hasProfile && !cpuOnly) return false;

    // ============================================================
    // SoC / CPU properties — spoofed in both modes
    // ============================================================

    if (prop == "ro.product.board" || prop == "ro.board.platform") {
        outValue = (hasProfile && !profileOpt->board.empty())
                   ? profileOpt->board : "pineapple";
        return true;
    }
    if (prop == "ro.hardware") {
        outValue = (hasProfile && !profileOpt->hardware.empty())
                   ? profileOpt->hardware : "qcom";
        return true;
    }
    if (prop == "ro.soc.model") {
        outValue = (hasProfile && !profileOpt->soc_model.empty())
                   ? profileOpt->soc_model : "SM8650";
        return true;
    }
    if (prop == "ro.soc.manufacturer") {
        outValue = (hasProfile && !profileOpt->soc_manufacturer.empty())
                   ? profileOpt->soc_manufacturer : "Qualcomm";
        return true;
    }
    if (prop == "ro.vendor.qti.soc_id") {
        if (hasProfile && !profileOpt->soc_id.empty()) {
            outValue = profileOpt->soc_id; return true;
        }
        if (cpuOnly) { outValue = "519"; return true; }
        return false;
    }
    if (prop == "ro.vendor.qti.soc_name") {
        if (hasProfile && !profileOpt->soc_model.empty()) {
            outValue = profileOpt->soc_model; return true;
        }
        if (cpuOnly) { outValue = "SM8650"; return true; }
        return false;
    }

    // ============================================================
    // Full device profile properties (profile required)
    // ============================================================

    if (!hasProfile) return false;
    const DeviceProfile& p = profileOpt.value();

    // Suffix matching — covers ro.product.*.manufacturer, ro.product.vendor.manufacturer, etc.
    if (prop.ends_with(".manufacturer")) {
        outValue = p.manufacturer; return true;
    }
    if (prop.ends_with(".model")) {
        outValue = p.model; return true;
    }
    if (prop.ends_with(".brand")) {
        outValue = p.brand; return true;
    }
    // Device codename — only "ro.product.*" namespaces, not ro.product.cpu.abilist
    if (prop.starts_with("ro.product.") && prop.ends_with(".device")) {
        outValue = p.device; return true;
    }
    // Product name
    if (prop.starts_with("ro.product.") && prop.ends_with(".name")) {
        outValue = p.product; return true;
    }

    // Fingerprint
    if (prop.ends_with(".fingerprint") || prop == "ro.build.fingerprint") {
        outValue = p.fingerprint; return true;
    }

    // Android version
    if (prop.ends_with(".version.release") ||
        prop.ends_with(".version.release_or_codename")) {
        if (!p.android_version.empty()) {
            outValue = p.android_version; return true;
        }
        return false;
    }

    // SDK level — PIF uses "api_level" suffix; we cover both forms
    if (prop.ends_with("api_level") || prop.ends_with(".version.sdk")) {
        if (!p.android_version.empty()) {
            outValue = sdkFromVersion(p.android_version);
            return true;
        }
        return false;
    }

    // Security patch — exact PIF pattern
    if (prop.ends_with(".security_patch")) {
        if (!p.security_patch.empty()) {
            outValue = p.security_patch; return true;
        }
        return false;
    }

    // Build ID — extract 4th /…/ segment from fingerprint
    if (prop.ends_with(".build.id") || prop == "ro.build.id" ||
        prop == "ro.build.display.id") {
        const std::string& fp = p.fingerprint;
        int slashes = 0;
        size_t start = 0;
        for (size_t i = 0; i < fp.size(); i++) {
            if (fp[i] == '/') {
                slashes++;
                if (slashes == 3) start = i + 1;
                if (slashes == 4) {
                    outValue = fp.substr(start, i - start);
                    return !outValue.empty();
                }
            }
        }
        return false;
    }

    // Build type / tags
    if (prop == "ro.build.type" || prop.ends_with(".build.type")) {
        outValue = "user"; return true;
    }
    if (prop == "ro.build.tags") {
        outValue = "release-keys"; return true;
    }

    return false;
}

// ================================================================
// THE HOOK — proven PlayIntegrityFix pattern
//
// Single hook on __system_property_read_callback.
// The callback receives (cookie, name, value, serial).
// We intercept the app's callback and inject spoofed values.
// ================================================================

typedef void (*T_Callback)(void*, const char*, const char*, uint32_t);
typedef void (*prop_read_cb_fn_t)(const prop_info*, T_Callback, void*);

// The original __system_property_read_callback function
static prop_read_cb_fn_t o_system_property_read_callback = nullptr;

// The app's real callback for each read — captured per-call (not TLS)
// This is safe: __system_property_read_callback is not re-entrant
static T_Callback o_app_callback = nullptr;

// Our intercept of the app's callback — receives name+value before delivery
static void my_modify_callback(void* cookie, const char* name, const char* value,
                                uint32_t serial) {
    if (!cookie || !name || !value || !o_app_callback) return;

    const char* finalValue = value;
    std::string spoofedVal;

    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: [%s]: %s -> %s", name, value, spoofedVal.c_str());
        finalValue = spoofedVal.c_str();
    }

    o_app_callback(cookie, name, finalValue, serial);
}

// Our hook that replaces __system_property_read_callback
static void my_system_property_read_callback(const prop_info* pi, T_Callback callback,
                                              void* cookie) {
    if (pi && callback && cookie) {
        o_app_callback = callback;  // save app's real callback
    }
    o_system_property_read_callback(pi, my_modify_callback, cookie);
}

// ----------------------------------------------------------------
// Hooked callback (C linkage required for bytehook_hooked_t)
// ----------------------------------------------------------------
static void on_hook_status(bytehook_stub_t /*stub*/, int status_code,
                           const char* caller_path, const char* sym_name,
                           void* /*new_func*/, void* prev_func, void* /*arg*/) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK) {
        // Capture original pointer when bytehook delivers it
        if (prev_func && !o_system_property_read_callback) {
            o_system_property_read_callback =
                reinterpret_cast<prop_read_cb_fn_t>(prev_func);
        }
        LOGI("SysPropHook: hook OK for %s in '%s' (prev=%p)",
             sym_name, caller_path ? caller_path : "?", prev_func);
    } else {
        LOGE("SysPropHook: hook FAILED for %s in '%s' (status=%d)",
             sym_name, caller_path ? caller_path : "?", status_code);
    }
}

// ----------------------------------------------------------------
// onEnable
// ----------------------------------------------------------------

bool SysPropHook::onEnable(const Context& /*ctx*/) {
    // Init bytehook (idempotent if called multiple times)
    int ret = bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
    if (ret != 0) {
        LOGE("SysPropHook: bytehook_init failed (%d)", ret);
        return false;
    }

    // Hook __system_property_read_callback in ALL loaded libraries.
    // This is the single, proven hook point used by PlayIntegrityFix.
    bytehook_hook_all(
        nullptr,                   // callee_path_name: search all libs (libc.so)
        "__system_property_read_callback",
        reinterpret_cast<void*>(my_system_property_read_callback),
        on_hook_status,
        nullptr
    );

    // Ensure o_system_property_read_callback is always valid.
    // In AUTOMATIC mode prev_func in the hook callback may be null;
    // fall back to direct dlsym so my_system_property_read_callback
    // never crashes with a null function pointer.
    if (!o_system_property_read_callback) {
        void* rawFn = dlsym(RTLD_DEFAULT, "__system_property_read_callback");
        if (rawFn) {
            o_system_property_read_callback =
                reinterpret_cast<prop_read_cb_fn_t>(rawFn);
            LOGI("SysPropHook: fallback original via RTLD_DEFAULT=%p", rawFn);
        } else {
            LOGE("SysPropHook: CRITICAL — cannot resolve original fn, hook will crash");
            return false;
        }
    }

    LOGI("SysPropHook: active (profile=%s, cpu_only=%d)",
         SysPropHook::getProfile().has_value()
             ? SysPropHook::getProfile()->model.c_str() : "none",
         SysPropHook::isCpuSpoofOnly() ? 1 : 0);
    return true;
}

REGISTER_HOOK(SysPropHook);

} // namespace gameunlocker
