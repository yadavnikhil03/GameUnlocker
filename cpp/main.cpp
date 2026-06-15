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
        ConfigManager config(ctx);
        CompanionManager companion(ctx);
        Spoofer spoofer(ctx);
        hookManager_ = std::make_unique<HookManager>(ctx);

        if (!config.load()) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (config.isAppBlacklisted(pkgStr)) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::optional<DeviceProfile> profileOpt = config.getProfileForApp(pkgStr);
        bool appNeedsCpuSpoof = config.isCpuSpoofApp(pkgStr);

        if (profileOpt.has_value() || appNeedsCpuSpoof) {
            LOGI("GameUnlocker Target Detected: %s [Profile: %s]", pkgStr.c_str(), profileOpt.has_value() ? "Active" : "None");

            std::string modulePath = companion.resolveModulePath();
            if (appNeedsCpuSpoof && !modulePath.empty()) {
                companion.mountSpoof(modulePath + "/cpuinfo_spoof");
            } else if (!appNeedsCpuSpoof) {
                companion.unmountSpoof();
            }
        
            if (profileOpt.has_value()) {
                spoofer.applyDeviceSpoof(profileOpt.value());
            }

            SysPropHook::setProfile(profileOpt);
            hookManager_->initialize(profileOpt);
            hookManager_->enableHooks();

            if (hookManager_->hasActiveHooks()) {
                api_->pltHookCommit();
            } else {
                api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            }
        } else {
            companion.unmountSpoof();
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
    std::unique_ptr<HookManager> hookManager_;
};

} 

REGISTER_ZYGISK_MODULE(gameunlocker::AppLifecycle)
REGISTER_ZYGISK_COMPANION(gameunlocker::companionHandler)
