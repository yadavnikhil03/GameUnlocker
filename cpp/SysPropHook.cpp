#include "SysPropHook.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"
#include <string.h>
#include <sys/system_properties.h>
#include <bytehook.h>

namespace gameunlocker {

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

typedef int (*prop_get_t)(const char*, char*);
typedef void (*prop_read_cb_t)(void*, const char*, const char*, uint32_t);
typedef void (*prop_read_t)(const void*, prop_read_cb_t, void*);
typedef void (*prop_read_old_t)(const void*, unsigned*, char*, char*);

static prop_get_t orig_property_get = nullptr;
static prop_read_t orig_property_read_callback = nullptr;
static prop_read_old_t orig_property_read = nullptr;

thread_local prop_read_cb_t tls_app_callback = nullptr;

static void writeSpoofedValue(char* value, const std::string& spoofedVal) {
    size_t toCopy = spoofedVal.size();
    if (toCopy >= PROP_VALUE_MAX) toCopy = PROP_VALUE_MAX - 1;
    memcpy(value, spoofedVal.c_str(), toCopy);
    value[toCopy] = '\0';
}

// ----------------------------------------------------------------
// Fingerprint helpers
// ----------------------------------------------------------------

// Extract the build ID segment (between 3rd and 4th slash) from a fingerprint.
// Format: brand/product/device:VERSION/BUILD_ID/INCREMENTAL:type/tags
static std::string fpBuildId(const std::string& fp) {
    size_t f1 = fp.find('/');
    if (f1 == std::string::npos) return "";
    size_t f2 = fp.find('/', f1 + 1);
    if (f2 == std::string::npos) return "";
    size_t f3 = fp.find('/', f2 + 1);
    if (f3 == std::string::npos) return "";
    size_t f4 = fp.find('/', f3 + 1);
    if (f4 == std::string::npos) return fp.substr(f3 + 1);
    return fp.substr(f3 + 1, f4 - f3 - 1);
}

// Extract Android version from fingerprint (between ':' and next '/').
static std::string fpVersion(const std::string& fp) {
    size_t c = fp.find(':');
    if (c == std::string::npos) return "";
    size_t s = fp.find('/', c + 1);
    if (s == std::string::npos) return fp.substr(c + 1);
    return fp.substr(c + 1, s - c - 1);
}

// Derive SDK_INT string from Android version string.
static std::string sdkFromVersion(const std::string& ver) {
    if (ver == "16") return "36";
    if (ver == "15") return "35";
    if (ver == "14") return "34";
    if (ver == "13") return "33";
    if (ver == "12") return "32";
    if (ver == "11") return "30";
    if (ver == "10") return "29";
    return "34"; // default to Android 14
}

// ----------------------------------------------------------------
// Core spoofing logic
// ----------------------------------------------------------------

static bool getSpoofedValue(const char* name, std::string& outValue) {
    if (!name) return false;

    auto profileOpt = SysPropHook::getProfile();
    const bool hasProfile = profileOpt.has_value();
    const bool cpuOnly    = SysPropHook::isCpuSpoofOnly();

    if (!hasProfile && !cpuOnly) return false;

    // ================================================================
    // === CPU / SoC properties — spoofed for BOTH modes ==============
    // ================================================================

    if (strcmp(name, "ro.product.board") == 0 ||
        strcmp(name, "ro.board.platform") == 0) {
        outValue = (hasProfile && !profileOpt.value().board.empty())
                   ? profileOpt.value().board : "pineapple";
        return true;
    }

    if (strcmp(name, "ro.hardware") == 0) {
        outValue = (hasProfile && !profileOpt.value().hardware.empty())
                   ? profileOpt.value().hardware : "qcom";
        return true;
    }

    if (strcmp(name, "ro.soc.model") == 0) {
        outValue = (hasProfile && !profileOpt.value().soc_model.empty())
                   ? profileOpt.value().soc_model : "SM8650";
        return true;
    }

    if (strcmp(name, "ro.soc.manufacturer") == 0) {
        outValue = (hasProfile && !profileOpt.value().soc_manufacturer.empty())
                   ? profileOpt.value().soc_manufacturer : "Qualcomm";
        return true;
    }

    // QTI SoC numeric identifier (read by PUBG, COD, etc. to detect chip tier)
    if (strcmp(name, "ro.vendor.qti.soc_id") == 0) {
        if (hasProfile && !profileOpt.value().soc_id.empty()) {
            outValue = profileOpt.value().soc_id;
            return true;
        } else if (cpuOnly) {
            outValue = "519"; // SM8650 (Snapdragon 8 Gen 3)
            return true;
        }
        return false;
    }

    if (strcmp(name, "ro.vendor.qti.soc_name") == 0) {
        if (hasProfile && !profileOpt.value().soc_model.empty()) {
            outValue = profileOpt.value().soc_model;
            return true;
        } else if (cpuOnly) {
            outValue = "SM8650";
            return true;
        }
        return false;
    }

    // ================================================================
    // === Full device profile properties =============================
    // ================================================================

    if (!hasProfile) return false;
    const auto& profile = profileOpt.value();

    // --- Manufacturer ---
    if (strcmp(name, "ro.product.manufacturer") == 0 ||
        strcmp(name, "ro.product.vendor.manufacturer") == 0 ||
        strcmp(name, "ro.product.odm.manufacturer") == 0 ||
        strcmp(name, "ro.product.system.manufacturer") == 0 ||
        strcmp(name, "ro.product.product.manufacturer") == 0) {
        outValue = profile.manufacturer; return true;
    }

    // --- Model ---
    if (strcmp(name, "ro.product.model") == 0 ||
        strcmp(name, "ro.product.vendor.model") == 0 ||
        strcmp(name, "ro.product.odm.model") == 0 ||
        strcmp(name, "ro.product.system.model") == 0 ||
        strcmp(name, "ro.product.product.model") == 0) {
        outValue = profile.model; return true;
    }

    // --- Device codename ---
    if (strcmp(name, "ro.product.device") == 0 ||
        strcmp(name, "ro.product.vendor.device") == 0 ||
        strcmp(name, "ro.product.odm.device") == 0 ||
        strcmp(name, "ro.product.system.device") == 0 ||
        strcmp(name, "ro.product.product.device") == 0) {
        outValue = profile.device; return true;
    }

    // --- Brand ---
    if (strcmp(name, "ro.product.brand") == 0 ||
        strcmp(name, "ro.product.vendor.brand") == 0 ||
        strcmp(name, "ro.product.odm.brand") == 0 ||
        strcmp(name, "ro.product.system.brand") == 0 ||
        strcmp(name, "ro.product.product.brand") == 0) {
        outValue = profile.brand; return true;
    }

    // --- Product name ---
    if (strcmp(name, "ro.product.name") == 0 ||
        strcmp(name, "ro.product.vendor.name") == 0 ||
        strcmp(name, "ro.product.odm.name") == 0 ||
        strcmp(name, "ro.product.system.name") == 0 ||
        strcmp(name, "ro.product.product.name") == 0) {
        outValue = profile.product; return true;
    }

    // --- Fingerprint (all partitions) ---
    if (strcmp(name, "ro.build.fingerprint") == 0 ||
        strcmp(name, "ro.bootimage.build.fingerprint") == 0 ||
        strcmp(name, "ro.vendor.build.fingerprint") == 0 ||
        strcmp(name, "ro.system.build.fingerprint") == 0 ||
        strcmp(name, "ro.product.build.fingerprint") == 0 ||
        strcmp(name, "ro.odm.build.fingerprint") == 0) {
        outValue = profile.fingerprint; return true;
    }

    // --- Android version (all partitions) ---
    if (strcmp(name, "ro.build.version.release") == 0 ||
        strcmp(name, "ro.build.version.release_or_codename") == 0 ||
        strcmp(name, "ro.vendor.build.version.release") == 0 ||
        strcmp(name, "ro.system.build.version.release") == 0 ||
        strcmp(name, "ro.product.build.version.release") == 0 ||
        strcmp(name, "ro.odm.build.version.release") == 0) {
        outValue = !profile.android_version.empty()
                   ? profile.android_version : fpVersion(profile.fingerprint);
        return !outValue.empty();
    }

    // --- SDK integer (as string, all partitions) ---
    if (strcmp(name, "ro.build.version.sdk") == 0 ||
        strcmp(name, "ro.vendor.build.version.sdk") == 0 ||
        strcmp(name, "ro.system.build.version.sdk") == 0 ||
        strcmp(name, "ro.product.build.version.sdk") == 0) {
        std::string ver = !profile.android_version.empty()
                          ? profile.android_version : fpVersion(profile.fingerprint);
        outValue = sdkFromVersion(ver);
        return true;
    }

    // --- Security patch date ---
    if (strcmp(name, "ro.build.version.security_patch") == 0 ||
        strcmp(name, "ro.vendor.build.security_patch") == 0 ||
        strcmp(name, "ro.system.build.security_patch") == 0 ||
        strcmp(name, "ro.product.build.security_patch") == 0 ||
        strcmp(name, "ro.odm.build.security_patch") == 0) {
        if (!profile.security_patch.empty()) {
            outValue = profile.security_patch; return true;
        }
        return false;
    }

    // --- Build ID and display build ID ---
    if (strcmp(name, "ro.build.id") == 0 ||
        strcmp(name, "ro.build.display.id") == 0 ||
        strcmp(name, "ro.system.build.id") == 0 ||
        strcmp(name, "ro.vendor.build.id") == 0) {
        outValue = fpBuildId(profile.fingerprint);
        return !outValue.empty();
    }

    // --- Build type (always "user" for production devices) ---
    if (strcmp(name, "ro.build.type") == 0 ||
        strcmp(name, "ro.system.build.type") == 0 ||
        strcmp(name, "ro.vendor.build.type") == 0) {
        outValue = "user"; return true;
    }

    // --- Build tags ---
    if (strcmp(name, "ro.build.tags") == 0) {
        outValue = "release-keys"; return true;
    }

    return false;
}

// ----------------------------------------------------------------
// Hook implementations
// ----------------------------------------------------------------

static int my_system_property_get(const char* name, char* value) {
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: get '%s' -> '%s'", name, spoofedVal.c_str());
        writeSpoofedValue(value, spoofedVal);
        return static_cast<int>(strlen(value));
    }
    if (orig_property_get) {
        return orig_property_get(name, value);
    }
    // Fallback: use bytehook call-prev trampoline
    BYTEHOOK_CALL_PREV(my_system_property_get, name, value);
    return 0;
}

static void my_read_cb(void* cookie, const char* name, const char* value, uint32_t serial) {
    if (!tls_app_callback) return;

    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: read_cb '%s' -> '%s'", name, spoofedVal.c_str());
        tls_app_callback(cookie, name, spoofedVal.c_str(), serial);
        return;
    }
    tls_app_callback(cookie, name, value, serial);
}

static void my_system_property_read_callback(const void* pi, prop_read_cb_t cb, void* cookie) {
    if (!orig_property_read_callback) {
        // Fallback to bytehook call-prev; saves original cb via TLS
        auto old_cb = tls_app_callback;
        tls_app_callback = cb;
        BYTEHOOK_CALL_PREV(my_system_property_read_callback, pi, my_read_cb, cookie);
        tls_app_callback = old_cb;
        return;
    }

    auto old_cb = tls_app_callback;
    tls_app_callback = cb;
    orig_property_read_callback(pi, my_read_cb, cookie);
    tls_app_callback = old_cb;
}

static void my_system_property_read(const void* pi, unsigned* serial, char* name, char* value) {
    if (orig_property_read) {
        orig_property_read(pi, serial, name, value);
    } else {
        BYTEHOOK_CALL_PREV(my_system_property_read, pi, serial, name, value);
    }

    if (!name || !value) return;

    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: read '%s' -> '%s'", name, spoofedVal.c_str());
        writeSpoofedValue(value, spoofedVal);
    }
}

// ----------------------------------------------------------------
// Hook installation callback
// ----------------------------------------------------------------

static void on_hooked(bytehook_stub_t task_stub, int status_code, const char* caller_path_name,
                      const char* sym_name, void* new_func, void* prev_func, void* arg) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK) {
        if (strcmp(sym_name, "__system_property_get") == 0) {
            // prev_func may be null in AUTOMATIC mode (uses call-prev trampolines instead).
            // That is OK — my_system_property_get uses BYTEHOOK_CALL_PREV as fallback.
            if (prev_func && !orig_property_get) {
                orig_property_get = reinterpret_cast<prop_get_t>(prev_func);
            }
            LOGI("SysPropHook: hook OK for __system_property_get (prev=%p)", prev_func);
        } else if (strcmp(sym_name, "__system_property_read_callback") == 0) {
            if (prev_func && !orig_property_read_callback) {
                orig_property_read_callback = reinterpret_cast<prop_read_t>(prev_func);
            }
            LOGI("SysPropHook: hook OK for __system_property_read_callback (prev=%p)", prev_func);
        } else if (strcmp(sym_name, "__system_property_read") == 0) {
            if (prev_func && !orig_property_read) {
                orig_property_read = reinterpret_cast<prop_read_old_t>(prev_func);
            }
            LOGI("SysPropHook: hook OK for __system_property_read (prev=%p)", prev_func);
        }
    } else {
        LOGE("SysPropHook: bytehook FAILED for '%s' in '%s' (status=%d)",
             sym_name, caller_path_name ? caller_path_name : "?", status_code);
    }
}

static bool bytehook_initialized_ = false;

bool SysPropHook::onEnable(const Context& ctx) {
    if (!bytehook_initialized_) {
        int ret = bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
        if (ret != 0) {
            LOGE("SysPropHook: bytehook_init failed (%d)", ret);
            return false;
        }
        bytehook_initialized_ = true;
    }

    bytehook_hook_all(nullptr, "__system_property_get",
                      reinterpret_cast<void*>(my_system_property_get), on_hooked, nullptr);
    bytehook_hook_all(nullptr, "__system_property_read_callback",
                      reinterpret_cast<void*>(my_system_property_read_callback), on_hooked, nullptr);
    bytehook_hook_all(nullptr, "__system_property_read",
                      reinterpret_cast<void*>(my_system_property_read), on_hooked, nullptr);

    LOGI("SysPropHook: registered hooks for __system_property_get/read_callback/read");
    return true;
}

REGISTER_HOOK(SysPropHook);

} // namespace gameunlocker
