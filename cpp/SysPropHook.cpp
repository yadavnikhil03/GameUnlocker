#include "SysPropHook.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"
#include <string.h>

namespace gameunlocker {

std::optional<DeviceProfile> SysPropHook::activeProfile_ = std::nullopt;

void SysPropHook::setProfile(const std::optional<DeviceProfile>& profile) {
    activeProfile_ = profile;
}

std::optional<DeviceProfile> SysPropHook::getProfile() {
    return activeProfile_;
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
    auto profileOpt = SysPropHook::getProfile();
    if (!profileOpt.has_value() || !name) return false;
    
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
        strncpy(value, spoofedVal.c_str(), 91);
        value[91] = '\0';
    }
}

bool SysPropHook::onEnable(const Context& ctx) {
    zygisk::Api* api = ctx.getApi();
    if (!api) {
        LOGE("SysPropHook::onEnable failed: zygisk API is null");
        return false;
    }

    api->pltHookRegister(".*", "__system_property_get", (void*)my_system_property_get, (void**)&orig_property_get);
    api->pltHookRegister(".*", "__system_property_read_callback", (void*)my_system_property_read_callback, (void**)&orig_property_read_callback);
    api->pltHookRegister(".*", "__system_property_read", (void*)my_system_property_read, (void**)&orig_property_read);

    LOGI("SysPropHook registered for __system_property* functions");
    return true;
}

void SysPropHook::onDisable(const Context& ctx) {
}

REGISTER_HOOK(SysPropHook);

} // namespace gameunlocker
