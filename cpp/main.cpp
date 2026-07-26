#include "zygisk.hpp"
#include "context.hpp"
#include "Config.hpp"
#include "Spoofer.hpp"
#include "HookManager.hpp"
#include "SysPropHook.hpp"
#include "Companion.hpp"
#include "logger.hpp"
#include "raii.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>

// connectDaemon — connects to the gu_controller performance daemon
// in postAppSpecialize so it can apply Qualcomm GPU perf mode while
// the game is running.
static void connectDaemon() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    const char* socket_name = "@gameunlocker_daemon";
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, socket_name + 1, sizeof(addr.sun_path) - 2);
    int len = offsetof(struct sockaddr_un, sun_path) + strlen(socket_name + 1) + 1;

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), len) == 0) {
        char ping = 1;
        write(sock, &ping, 1);
        char buf[16];
        // Block until daemon closes connection (keeps perf mode active)
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
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!api_ || !env_ || !args) return;

        Context ctx(api_, env_);

        if (!ConfigManager::globalInit(ctx)) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        JniString pkg(env_, args->nice_name);
        const char* package_name = pkg.get();
        if (!package_name) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        // Strip process suffix (e.g. "com.example.app:service" -> "com.example.app")
        std::string pkgStr = package_name;
        auto pos = pkgStr.find(':');
        if (pos != std::string::npos) {
            pkgStr = pkgStr.substr(0, pos);
        }

        // Check blacklist first — these apps must never be spoofed
        if (ConfigManager::isAppBlacklisted(pkgStr)) {
            LOGI("AppLifecycle: blacklisted app '%s' — skipping", pkgStr.c_str());
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::optional<DeviceProfile> profileOpt  = ConfigManager::getProfileForApp(pkgStr);
        bool                         cpuSpoofApp = ConfigManager::isCpuSpoofApp(pkgStr);

        if (!profileOpt.has_value() && !cpuSpoofApp) {
            // Not a target app — unload to save memory
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGI("AppLifecycle: target '%s' [profile=%s cpu_spoof=%d]",
             pkgStr.c_str(),
             profileOpt.has_value() ? profileOpt.value().model.c_str() : "none",
             cpuSpoofApp ? 1 : 0);

        isTargetApp_ = true;

        // --- CPU /proc/cpuinfo bind-mount via root companion process ---
        if (cpuSpoofApp) {
            CompanionManager companion(ctx);
            std::string modPath = companion.resolveModulePath();
            if (!modPath.empty()) {
                if (companion.mountCpuInfo(modPath)) {
                    LOGI("AppLifecycle: CPU spoof mount requested for '%s'", pkgStr.c_str());
                } else {
                    LOGW("AppLifecycle: CPU spoof mount FAILED for '%s'", pkgStr.c_str());
                }
            } else {
                LOGW("AppLifecycle: Could not resolve module path for CPU spoof");
            }
        }

        // --- System property hook setup ---
        SysPropHook::setProfile(profileOpt, cpuSpoofApp && !profileOpt.has_value());

        // --- Register and enable all hooks ---
        HookManager hookManager(ctx);
        hookManager.initialize();
        hookManager.enableHooks();

        // ---------------------------------------------------------------
        // CRITICAL: Do NOT call DLCLOSE_MODULE_LIBRARY for target apps!
        //
        // The bytehook trampolines for __system_property_read_callback and
        // the JNI native method hooks for glGetString all reference code
        // that lives inside this shared library (.so).
        //
        // If we unload the .so here, those function pointers become
        // dangling — the next time the app calls getprop or glGetString,
        // the process will segfault. This was the primary bug preventing
        // spoofing from working.
        //
        // For non-target apps, DLCLOSE is called above (before we reach
        // this point), so memory usage stays low for everything else.
        // ---------------------------------------------------------------
        if (!hookManager.hasActiveHooks()) {
            // No hooks were installed at all — safe to unload
            LOGW("AppLifecycle: no active hooks for '%s' — unloading", pkgStr.c_str());
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            isTargetApp_ = false;
            return;
        }

        // --- JNI-level Build field spoofing ---
        if (profileOpt.has_value()) {
            Spoofer spoofer(ctx);
            spoofer.applyDeviceSpoof(profileOpt.value());
        }

        LOGI("AppLifecycle: hooks active for '%s' — module stays loaded", pkgStr.c_str());
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        if (isTargetApp_) {
            // Connect to gu_controller daemon to activate performance mode
            std::thread(connectDaemon).detach();
        }
    }

private:
    zygisk::Api* api_        = nullptr;
    JNIEnv*      env_        = nullptr;
    bool         isTargetApp_ = false;
};

} // namespace gameunlocker

REGISTER_ZYGISK_MODULE(gameunlocker::AppLifecycle)

// Register the companion handler that runs as ROOT in the zygote companion
// process and performs privileged bind-mount of /proc/cpuinfo.
REGISTER_ZYGISK_COMPANION(gameunlocker::companionHandler)
