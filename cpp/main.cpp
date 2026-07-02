#include "zygisk.hpp"
#include "context.hpp"
#include "Config.hpp"
#include "Spoofer.hpp"
#include "HookManager.hpp"
#include "SysPropHook.hpp"
#include "logger.hpp"
#include "raii.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>

void connectDaemon() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    const char* socket_name = "@gameunlocker_daemon";
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, socket_name + 1, sizeof(addr.sun_path) - 2);
    int len = offsetof(struct sockaddr_un, sun_path) + strlen(socket_name + 1) + 1;

    if (connect(sock, (struct sockaddr*)&addr, len) == 0) {
        char ping = 1;
        write(sock, &ping, 1);
        char buf[16];
        while (read(sock, buf, sizeof(buf)) > 0) {}
    }
    close(sock);
}

JavaVM* g_vm = nullptr;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
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
        configLoaded_ = ConfigManager::globalInit(ctx);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!api_ || !env_ || !args) return;

        if (!configLoaded_) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

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
        HookManager hookManager(ctx);

        if (ConfigManager::isAppBlacklisted(pkgStr)) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::optional<DeviceProfile> profileOpt = ConfigManager::getProfileForApp(pkgStr);
        bool appNeedsCpuSpoof = ConfigManager::isCpuSpoofApp(pkgStr);

        if (profileOpt.has_value() || appNeedsCpuSpoof) {
            LOGI("GameUnlocker Target Detected: %s [Profile: %s, CPU Spoof: %d]", pkgStr.c_str(), profileOpt.has_value() ? "Active" : "None", appNeedsCpuSpoof);
            
            std::thread(connectDaemon).detach();

            SysPropHook::setProfile(profileOpt, appNeedsCpuSpoof);
            hookManager.initialize();
            hookManager.enableHooks();

            if (!hookManager.hasActiveHooks()) {
                api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            }

            if (profileOpt.has_value()) {
                Spoofer spoofer(ctx);
                spoofer.applyDeviceSpoof(profileOpt.value());
            }
        } else {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool configLoaded_ = false;
};

}

REGISTER_ZYGISK_MODULE(gameunlocker::AppLifecycle)
