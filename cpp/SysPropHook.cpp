#include "SysPropHook.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"
#include <string.h>
#include <string_view>
#include <sys/system_properties.h>
#include <dlfcn.h>
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
    if (!hasProfile) return false;
    const DeviceProfile& p = profileOpt.value();
    if (ends_with(prop, ".manufacturer")) {
        outValue = p.manufacturer; return true;
    }
    if (ends_with(prop, ".model")) {
        outValue = p.model; return true;
    }
    if (ends_with(prop, ".brand")) {
        outValue = p.brand; return true;
    }
    if (starts_with(prop, "ro.product.") && ends_with(prop, ".device")) {
        outValue = p.device; return true;
    }
    if (starts_with(prop, "ro.product.") && ends_with(prop, ".name")) {
        outValue = p.product; return true;
    }
    if (ends_with(prop, ".fingerprint") || prop == "ro.build.fingerprint") {
        outValue = p.fingerprint; return true;
    }
    if (ends_with(prop, ".security_patch")) {
        if (!p.security_patch.empty()) {
            outValue = p.security_patch; return true;
        }
        return false;
    }
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
    if (prop == "ro.build.type" || ends_with(prop, ".build.type")) {
        outValue = "user"; return true;
    }
    if (prop == "ro.build.tags") {
        outValue = "release-keys"; return true;
    }
    return false;
}
typedef void (*T_Callback)(void*, const char*, const char*, uint32_t);
typedef void (*prop_read_cb_fn_t)(const prop_info*, T_Callback, void*);
typedef int (*prop_get_fn_t)(const char*, char*);
typedef int (*prop_read_fn_t)(const prop_info*, char*, char*);

static prop_read_cb_fn_t o_system_property_read_callback = nullptr;

static prop_get_fn_t o_system_property_get = nullptr;

static prop_read_fn_t o_system_property_read = nullptr;
struct PropCallContext {
    T_Callback  realCallback;

    void*       realCookie;
};

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

static void my_system_property_read_callback(const prop_info* pi, T_Callback callback,

                                              void* cookie) {
    if (!o_system_property_read_callback) {
        if (callback) callback(cookie, "", "", 0);
        return;
    }
    if (pi && callback) {
        PropCallContext ctx{ callback, cookie };
        o_system_property_read_callback(pi, my_modify_callback, &ctx);
    } else {
        o_system_property_read_callback(pi, callback, cookie);
    }
}

static int my_system_property_get(const char* name, char* value) {
    if (!name || !value) {
        if (o_system_property_get) return o_system_property_get(name, value);
        return 0;
    }

    std::string spoofedVal;
    if (getSpoofedValue(name, spoofedVal)) {
        LOGD("SysPropHook: __system_property_get [%s]: -> '%s'", name, spoofedVal.c_str());
        strncpy(value, spoofedVal.c_str(), PROP_VALUE_MAX - 1);
        value[PROP_VALUE_MAX - 1] = '\0';
        return spoofedVal.length();
    }
    if (o_system_property_get) {
        return o_system_property_get(name, value);
    }
    return 0;
}

static int my_system_property_read(const prop_info* pi, char* name, char* value) {
    if (!o_system_property_read) return 0;

    int ret = o_system_property_read(pi, name, value);
    if (ret == 0 && name && value) {

        std::string spoofedVal;
        if (getSpoofedValue(name, spoofedVal)) {
            LOGD("SysPropHook: __system_property_read [%s]: -> '%s'", name, spoofedVal.c_str());
            strncpy(value, spoofedVal.c_str(), PROP_VALUE_MAX - 1);
            value[PROP_VALUE_MAX - 1] = '\0';
        }
    }
    return ret;
}

static void on_hook_status(bytehook_stub_t , int status_code,
                           const char* caller_path, const char* sym_name,

                           void* , void* prev_func, void* ) {
    if (status_code == BYTEHOOK_STATUS_CODE_OK) {
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

bool SysPropHook::onEnable(const Context& ) {

    int ret = bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);
    if (ret != 0) {
        LOGE("SysPropHook: bytehook_init failed (%d)", ret);
        return false;
    }
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
    bytehook_hook_all(
        nullptr,                   
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
} 