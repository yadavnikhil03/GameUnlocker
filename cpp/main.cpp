#include "zygisk.hpp"
#include "context.hpp"
#include "Config.hpp"
#include "Companion.hpp"
#include "Spoofer.hpp"
#include "HookManager.hpp"
#include "SysPropHook.hpp"
#include "logger.hpp"
#include "raii.hpp"

JavaVM* g_vm = nullptr;

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

namespace gameunlocker {

class AppLifecycle : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        api_ = api;
        env_ = env;
        
        Context ctx(api_, env_);
        ConfigManager::globalInit(ctx);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!api_ || !env_ || !args) return;

        JniString pkg(env_, args->nice_name);
        const char* package_name = pkg.get();
        if (!package_name) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string pkgStr = package_name;
        auto pos = pkgStr.find(':');
        if (pos != std::string::npos) {
            pkgStr = pkgStr.substr(0, pos);
        }

        Context ctx(api_, env_);
        CompanionManager companion(ctx);
        HookManager hookManager(ctx);

        if (ConfigManager::isAppBlacklisted(pkgStr)) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::optional<DeviceProfile> profileOpt = ConfigManager::getProfileForApp(pkgStr);
        bool appNeedsCpuSpoof = ConfigManager::isCpuSpoofApp(pkgStr);

        if (profileOpt.has_value() || appNeedsCpuSpoof) {
            LOGI("GameUnlocker Target Detected: %s [Profile: %s]", pkgStr.c_str(), profileOpt.has_value() ? "Active" : "None");

            std::string modulePath = companion.resolveModulePath();
            if (appNeedsCpuSpoof && !modulePath.empty()) {
                companion.mountSpoof(modulePath + "/cpuinfo_spoof");
            } else if (!appNeedsCpuSpoof) {
                companion.unmountSpoof();
            }
        
            activeProfile_ = profileOpt;

            SysPropHook::setProfile(profileOpt);
            hookManager.initialize(profileOpt);
            hookManager.enableHooks();

            if (hookManager.hasActiveHooks()) {
                api_->pltHookCommit();
            } else {
                api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            }
        } else {
            companion.unmountSpoof();
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        if (!api_ || !env_) return;

        if (activeProfile_.has_value()) {
            Context ctx(api_, env_);
            Spoofer spoofer(ctx);
            spoofer.applyDeviceSpoof(activeProfile_.value());
        }
    }

private:
    zygisk::Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
    std::optional<DeviceProfile> activeProfile_ = std::nullopt;
};

} 

REGISTER_ZYGISK_MODULE(gameunlocker::AppLifecycle)
REGISTER_ZYGISK_COMPANION(gameunlocker::companionHandler)
