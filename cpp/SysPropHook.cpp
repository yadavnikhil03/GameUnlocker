#include "SysPropHook.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"
#include <string.h>
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
    
    if (strcmp(name, "ro.product.model") == 0) outValue = profile.model;
    else if (strcmp(name, "ro.product.brand") == 0) outValue = profile.brand;
    else if (strcmp(name, "ro.product.name") == 0) outValue = profile.product;
    else if (strcmp(name, "ro.product.device") == 0) outValue = profile.device;
    else if (strcmp(name, "ro.product.manufacturer") == 0) outValue = profile.manufacturer;
    else if (strcmp(name, "ro.build.fingerprint") == 0) outValue = profile.fingerprint;
    else return false;

    return true;
}

static int my_system_property_get(const char* name, char* value) {
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: Intercepted __system_property_get for '%s', returning spoofed value '%s'", name, spoofedVal.c_str());
        strncpy(value, spoofedVal.c_str(), 91);
        value[91] = '\0';
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
    
    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: Intercepted __system_property_read for '%s', injecting spoofed value '%s'", name, spoofedVal.c_str());
        strncpy(value, spoofedVal.c_str(), 91);
        value[91] = '\0';
    }
}

static void on_hooked(bytehook_stub_t task_stub, int status_code, const char *caller_path_name,
                      const char *sym_name, void *new_func, void *prev_func, void *arg) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK && prev_func) {
        if (strcmp(sym_name, "__system_property_get") == 0 && !orig_property_get) {
            orig_property_get = (prop_get_t)prev_func;
        } else if (strcmp(sym_name, "__system_property_read_callback") == 0 && !orig_property_read_callback) {
            orig_property_read_callback = (prop_read_t)prev_func;
        } else if (strcmp(sym_name, "__system_property_read") == 0 && !orig_property_read) {
            orig_property_read = (prop_read_old_t)prev_func;
        }
    }
}

bool SysPropHook::onEnable(const Context& ctx) {
    bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);

    bytehook_hook_all(NULL, "__system_property_get", (void*)my_system_property_get, on_hooked, NULL);
    bytehook_hook_all(NULL, "__system_property_read_callback", (void*)my_system_property_read_callback, on_hooked, NULL);
    bytehook_hook_all(NULL, "__system_property_read", (void*)my_system_property_read, on_hooked, NULL);

    LOGI("SysPropHook registered for __system_property* functions via bytehook");
    return true;
}

void SysPropHook::onDisable(const Context& ctx) {
}

REGISTER_HOOK(SysPropHook);

} // namespace gameunlocker
