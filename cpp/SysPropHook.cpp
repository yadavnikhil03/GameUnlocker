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

static inline bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static inline bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

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
    if (ends_with(prop, ".manufacturer")) {
        outValue = p.manufacturer; return true;
    }
    if (ends_with(prop, ".model")) {
        outValue = p.model; return true;
    }
    if (ends_with(prop, ".brand")) {
        outValue = p.brand; return true;
    }
    // Device codename — only "ro.product.*" namespaces, not ro.product.cpu.abilist
    if (starts_with(prop, "ro.product.") && ends_with(prop, ".device")) {
        outValue = p.device; return true;
    }
    // Product name
    if (starts_with(prop, "ro.product.") && ends_with(prop, ".name")) {
        outValue = p.product; return true;
    }

    // Fingerprint
    if (ends_with(prop, ".fingerprint") || prop == "ro.build.fingerprint") {
        outValue = p.fingerprint; return true;
    }

    // Android version
    if (ends_with(prop, ".version.release") ||
        ends_with(prop, ".version.release_or_codename")) {
        if (!p.android_version.empty()) {
            outValue = p.android_version; return true;
        }
        return false;
    }

    // SDK level — PIF uses "api_level" suffix; we cover both forms
    if (ends_with(prop, "api_level") || ends_with(prop, ".version.sdk")) {
        if (!p.android_version.empty()) {
            outValue = sdkFromVersion(p.android_version);
            return true;
        }
        return false;
    }

    // Security patch — exact PIF pattern
    if (ends_with(prop, ".security_patch")) {
        if (!p.security_patch.empty()) {
            outValue = p.security_patch; return true;
        }
        return false;
    }

    // Build ID — extract 4th /…/ segment from fingerprint
    if (ends_with(prop, ".build.id") || prop == "ro.build.id" ||
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
    if (prop == "ro.build.type" || ends_with(prop, ".build.type")) {
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
//
// Thread-safety note: __system_property_read_callback is not
// re-entrant per call, but multiple threads can call it
// simultaneously. We use bytehook's BYTEHOOK_STACK_SCOPE to
// capture the original callback per-call rather than using a
// global that can be overwritten by another thread mid-call.
// ================================================================

typedef void (*T_Callback)(void*, const char*, const char*, uint32_t);
typedef void (*prop_read_cb_fn_t)(const prop_info*, T_Callback, void*);
typedef int (*prop_get_fn_t)(const char*, char*);
typedef int (*prop_read_fn_t)(const prop_info*, char*, char*);

// The original functions
static prop_read_cb_fn_t o_system_property_read_callback = nullptr;
static prop_get_fn_t o_system_property_get = nullptr;
static prop_read_fn_t o_system_property_read = nullptr;

// ----------------------------------------------------------------
// Per-call state passed through cookie wrapping.
//
// Instead of a single global o_app_callback (which is NOT thread-safe),
// we wrap the original cookie+callback in a small struct allocated on
// the stack of our hook, and pass that struct as the new cookie.
// This makes each call to __system_property_read_callback fully
// independent even when called from multiple threads simultaneously.
// ----------------------------------------------------------------
struct PropCallContext {
    T_Callback  realCallback;
    void*       realCookie;
};

// Our intercept of the app's callback
static void my_modify_callback(void* cookie, const char* name, const char* value,
                                uint32_t serial) {
    if (!cookie || !name || !value) return;

    auto* ctx = static_cast<PropCallContext*>(cookie);
    if (!ctx->realCallback) return;

    const char* finalValue = value;
    std::string spoofedVal;

    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: [%s]: '%s' -> '%s'", name, value, spoofedVal.c_str());
        finalValue = spoofedVal.c_str();
    }

    ctx->realCallback(ctx->realCookie, name, finalValue, serial);
}

// Our hook that replaces __system_property_read_callback
static void my_system_property_read_callback(const prop_info* pi, T_Callback callback,
                                              void* cookie) {
    if (!o_system_property_read_callback) {
        // Fallback — should not happen, but avoid a crash
        if (callback) callback(cookie, "", "", 0);
        return;
    }

    if (pi && callback) {
        // Wrap the caller's callback+cookie in a stack-allocated context.
        // This is safe because o_system_property_read_callback will invoke
        // my_modify_callback synchronously before returning.
        PropCallContext ctx{ callback, cookie };
        o_system_property_read_callback(pi, my_modify_callback, &ctx);
    } else {
        o_system_property_read_callback(pi, callback, cookie);
    }
}

// Our hook for __system_property_get
static int my_system_property_get(const char* name, char* value) {
    if (!name || !value) {
        if (o_system_property_get) return o_system_property_get(name, value);
        return 0;
    }
    
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: __system_property_get [%s]: -> '%s'", name, spoofedVal.c_str());
        strcpy(value, spoofedVal.c_str());
        return spoofedVal.length();
    }
    
    if (o_system_property_get) {
        return o_system_property_get(name, value);
    }
    return 0;
}

// Our hook for __system_property_read (deprecated but sometimes used)
static int my_system_property_read(const prop_info* pi, char* name, char* value) {
    if (!o_system_property_read) return 0;
    
    int ret = o_system_property_read(pi, name, value);
    
    if (ret == 0 && name && value) {
        std::string spoofedVal;
        if (getSpoofedValue(name, spoofedVal)) {
            LOGD("SysPropHook: __system_property_read [%s]: -> '%s'", name, spoofedVal.c_str());
            strcpy(value, spoofedVal.c_str());
        }
    }
    
    return ret;
}

// ----------------------------------------------------------------
// Hooked callback (C linkage required for bytehook_hooked_t)
// ----------------------------------------------------------------
static void on_hook_status(bytehook_stub_t /*stub*/, int status_code,
                           const char* caller_path, const char* sym_name,
                           void* /*new_func*/, void* prev_func, void* /*arg*/) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK) {
        // Capture original pointer when bytehook delivers it
        if (prev_func) {
            if (strcmp(sym_name, "__system_property_read_callback") == 0 && !o_system_property_read_callback) {
                o_system_property_read_callback = reinterpret_cast<prop_read_cb_fn_t>(prev_func);
            } else if (strcmp(sym_name, "__system_property_get") == 0 && !o_system_property_get) {
                o_system_property_get = reinterpret_cast<prop_get_fn_t>(prev_func);
            } else if (strcmp(sym_name, "__system_property_read") == 0 && !o_system_property_read) {
                o_system_property_read = reinterpret_cast<prop_read_fn_t>(prev_func);
            }
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

    // Ensure o_system_property_read_callback is resolved BEFORE hooking.
    // In AUTOMATIC mode the hook status callback may arrive after we've
    // already set up the hook, so we pre-resolve via dlsym to guarantee
    // it's never null when our hook runs.
    if (!o_system_property_read_callback) {
        void* rawFn = dlsym(RTLD_DEFAULT, "__system_property_read_callback");
        if (rawFn) {
            o_system_property_read_callback = reinterpret_cast<prop_read_cb_fn_t>(rawFn);
        }
    }
    if (!o_system_property_get) {
        void* rawFn = dlsym(RTLD_DEFAULT, "__system_property_get");
        if (rawFn) {
            o_system_property_get = reinterpret_cast<prop_get_fn_t>(rawFn);
        }
    }
    if (!o_system_property_read) {
        void* rawFn = dlsym(RTLD_DEFAULT, "__system_property_read");
        if (rawFn) {
            o_system_property_read = reinterpret_cast<prop_read_fn_t>(rawFn);
        }
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

    bytehook_hook_all(
        nullptr,
        "__system_property_get",
        reinterpret_cast<void*>(my_system_property_get),
        on_hook_status,
        nullptr
    );

    bytehook_hook_all(
        nullptr,
        "__system_property_read",
        reinterpret_cast<void*>(my_system_property_read),
        on_hook_status,
        nullptr
    );

    LOGI("SysPropHook: active (model='%s', cpu_only=%d)",
         SysPropHook::getProfile().has_value()
             ? SysPropHook::getProfile()->model.c_str() : "none",
         SysPropHook::isCpuSpoofOnly() ? 1 : 0);
    return true;
}

REGISTER_HOOK(SysPropHook);

} // namespace gameunlocker
