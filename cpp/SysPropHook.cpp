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

static bool getSpoofedValue(const char* name, std::string& outValue) {
    if (!name) return false;

    if (SysPropHook::isCpuSpoofOnly() || SysPropHook::getProfile().has_value()) {
        if (strcmp(name, "ro.board.platform") == 0) { outValue = "kalama"; return true; }
        if (strcmp(name, "ro.hardware") == 0) { outValue = "qcom"; return true; }
        if (strcmp(name, "ro.soc.model") == 0) { outValue = "SM8550"; return true; }
        if (strcmp(name, "ro.soc.manufacturer") == 0) { outValue = "Qualcomm"; return true; }
    }

    auto profileOpt = SysPropHook::getProfile();
    if (!profileOpt.has_value()) return false;

    const auto& profile = profileOpt.value();

    if (strcmp(name, "ro.product.manufacturer") == 0 ||
        strcmp(name, "ro.product.vendor.manufacturer") == 0 ||
        strcmp(name, "ro.product.odm.manufacturer") == 0 ||
        strcmp(name, "ro.product.system.manufacturer") == 0 ||
        strcmp(name, "ro.product.product.manufacturer") == 0) {
        outValue = profile.manufacturer; return true;
    }
    if (strcmp(name, "ro.product.model") == 0 ||
        strcmp(name, "ro.product.vendor.model") == 0 ||
        strcmp(name, "ro.product.odm.model") == 0 ||
        strcmp(name, "ro.product.system.model") == 0 ||
        strcmp(name, "ro.product.product.model") == 0) {
        outValue = profile.model; return true;
    }
    if (strcmp(name, "ro.product.device") == 0 ||
        strcmp(name, "ro.product.vendor.device") == 0 ||
        strcmp(name, "ro.product.odm.device") == 0 ||
        strcmp(name, "ro.product.system.device") == 0 ||
        strcmp(name, "ro.product.product.device") == 0) {
        outValue = profile.device; return true;
    }
    if (strcmp(name, "ro.product.brand") == 0 ||
        strcmp(name, "ro.product.vendor.brand") == 0 ||
        strcmp(name, "ro.product.odm.brand") == 0 ||
        strcmp(name, "ro.product.system.brand") == 0 ||
        strcmp(name, "ro.product.product.brand") == 0) {
        outValue = profile.brand; return true;
    }
    if (strcmp(name, "ro.product.name") == 0 ||
        strcmp(name, "ro.product.vendor.name") == 0 ||
        strcmp(name, "ro.product.odm.name") == 0 ||
        strcmp(name, "ro.product.system.name") == 0 ||
        strcmp(name, "ro.product.product.name") == 0) {
        outValue = profile.product; return true;
    }
    if (strcmp(name, "ro.build.fingerprint") == 0 ||
        strcmp(name, "ro.bootimage.build.fingerprint") == 0 ||
        strcmp(name, "ro.vendor.build.fingerprint") == 0 ||
        strcmp(name, "ro.system.build.fingerprint") == 0 ||
        strcmp(name, "ro.product.build.fingerprint") == 0 ||
        strcmp(name, "ro.odm.build.fingerprint") == 0) {
        outValue = profile.fingerprint; return true;
    }

    return false;
}

static int my_system_property_get(const char* name, char* value) {
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: Intercepted __system_property_get for '%s', returning spoofed value '%s'", name, spoofedVal.c_str());
        writeSpoofedValue(value, spoofedVal);
        return (int)strlen(value);
    }
    if (orig_property_get) {
        return orig_property_get(name, value);
    }
    return 0;
}

static void my_read_cb(void* cookie, const char* name, const char* value, uint32_t serial) {
    if (!tls_app_callback) return;
    
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: Intercepted read_cb for '%s', returning spoofed value '%s'", name, spoofedVal.c_str());
        tls_app_callback(cookie, name, spoofedVal.c_str(), serial);
        return;
    }
    tls_app_callback(cookie, name, value, serial);
}

static void my_system_property_read_callback(const void* pi, prop_read_cb_t cb, void* cookie) {
    if (!orig_property_read_callback) return;
    
    auto old_cb = tls_app_callback;
    tls_app_callback = cb;
    orig_property_read_callback(pi, my_read_cb, cookie);
    tls_app_callback = old_cb;
}

static void my_system_property_read(const void* pi, unsigned* serial, char* name, char* value) {
    if (!orig_property_read) return;
    orig_property_read(pi, serial, name, value);

    if (!name || !value) return;

    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: Intercepted __system_property_read for '%s', injecting spoofed value '%s'", name, spoofedVal.c_str());
        writeSpoofedValue(value, spoofedVal);
    }
}

static void on_hooked(bytehook_stub_t task_stub, int status_code, const char *caller_path_name,
                      const char *sym_name, void *new_func, void *prev_func, void *arg) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK && prev_func) {
        if (strcmp(sym_name, "__system_property_get") == 0 && !orig_property_get) {
            orig_property_get = (prop_get_t)prev_func;
            LOGI("Property hook installed on __system_property_get");
        } else if (strcmp(sym_name, "__system_property_read_callback") == 0 && !orig_property_read_callback) {
            orig_property_read_callback = (prop_read_t)prev_func;
            LOGI("Property hook installed on __system_property_read_callback");
        } else if (strcmp(sym_name, "__system_property_read") == 0 && !orig_property_read) {
            orig_property_read = (prop_read_old_t)prev_func;
            LOGI("Property hook installed on __system_property_read");
        }
    } else {
        LOGE("Bytehook failed for symbol: %s (Status: %d)", sym_name, status_code);
    }
}

static bool bytehook_initialized_ = false;

bool SysPropHook::onEnable(const Context& ctx) {
    if (!bytehook_initialized_) {
        bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
        bytehook_initialized_ = true;
    }

    bytehook_hook_all(NULL, "__system_property_get", (void*)my_system_property_get, on_hooked, NULL);
    bytehook_hook_all(NULL, "__system_property_read_callback", (void*)my_system_property_read_callback, on_hooked, NULL);
    bytehook_hook_all(NULL, "__system_property_read", (void*)my_system_property_read, on_hooked, NULL);

    LOGI("SysPropHook registered for __system_property* functions via bytehook");
    return true;
}



REGISTER_HOOK(SysPropHook);

} 
