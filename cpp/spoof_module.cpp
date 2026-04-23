// Core Zygisk hooking implementation inspired by and adapted from COPG by AlirezaParsi
// Original source: https://github.com/AlirezaParsi/COPG

#include <jni.h>
#include <string>
#include <fstream>
#include <android/log.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>
#include <fcntl.h>
#include <thread>
#include <mutex>
#include <cstring>
#include <limits.h>
#include <cerrno>
#include <cstdlib>
#include "zygisk.hpp"

#define LOG_TAG "GameUnlocker"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

struct JniString {
    JNIEnv* env;
    jstring jstr;
    const char* chars;
    JniString(JNIEnv* e, jstring s) : env(e), jstr(s), chars(nullptr) {
        if (jstr) chars = env->GetStringUTFChars(jstr, nullptr);
    }
    ~JniString() {
        if (jstr && chars) env->ReleaseStringUTFChars(jstr, chars);
    }
    const char* get() const { return chars; }
};

static std::string normalizeProcessName(const std::string& processName) {
    auto pos = processName.find(':');
    if (pos == std::string::npos) return processName;
    return processName.substr(0, pos);
}

static bool readConfig(zygisk::Api* api, std::string& out) {
    out.clear();
    if (!api) return false;

    int dirfd = api->getModuleDir();
    if (dirfd < 0) return false;

    int fd = openat(dirfd, "config.json", O_RDONLY);
    if (fd < 0) return false;

    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes <= 0) return false;

    out.assign(buffer, static_cast<size_t>(bytes));
    return true;
}

static bool containsToken(const std::string& content, const std::string& token) {
    return !token.empty() && content.find("\"" + token + "\"") != std::string::npos;
}

static bool isAppInConfig(zygisk::Api* api, const std::string& appName) {
    std::string content;
    return readConfig(api, content) && containsToken(content, appName);
}

static bool isCpuSpoofApp(zygisk::Api* api, const std::string& appName) {
    std::string content;
    if (!readConfig(api, content)) return false;

    auto cpuSection = content.find("\"cpu_spoof\"");
    if (cpuSection == std::string::npos) return false;

    auto withCpu = content.find("\"with_cpu\"", cpuSection);
    if (withCpu == std::string::npos) return false;

    auto openBracket = content.find('[', withCpu);
    auto closeBracket = content.find(']', openBracket == std::string::npos ? withCpu : openBracket);
    if (openBracket == std::string::npos || closeBracket == std::string::npos || closeBracket <= openBracket) {
        return false;
    }

    std::string list = content.substr(openBracket, closeBracket - openBracket + 1);
    return containsToken(list, appName);
}

static std::string resolveModulePath(zygisk::Api* api) {
    if (!api) return "";

    int dirfd = api->getModuleDir();
    if (dirfd < 0) return "";

    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", dirfd);

    char modulePath[PATH_MAX];
    ssize_t len = readlink(fdPath, modulePath, sizeof(modulePath) - 1);
    if (len <= 0) return "";

    modulePath[len] = '\0';
    return std::string(modulePath);
}

static void companion(int fd) {
    char buffer[512];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::string command = buffer;
        if (command.rfind("mount_spoof:", 0) == 0) {
            std::string spoof_file_path = command.substr(strlen("mount_spoof:"));
            if (spoof_file_path.rfind("/data/adb/modules/", 0) != 0) {
                LOGW("Rejected invalid spoof path");
                return;
            }

            if (access(spoof_file_path.c_str(), F_OK) == 0) {
                system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
                char mount_cmd[512];
                snprintf(mount_cmd, sizeof(mount_cmd), "/system/bin/mount --bind %s /proc/cpuinfo", spoof_file_path.c_str());
                system(mount_cmd);
                LOGI("CPU Mount Companion Success");
            } else {
                LOGW("cpuinfo_spoof missing: %s", spoof_file_path.c_str());
            }
        }
    }
}

class GameUnlockerModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!api || !env || !args) {
            return;
        }

        JniString pkg(env, args->nice_name);
        const char* package_name = pkg.get();
        if (!package_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string pkgStr = normalizeProcessName(package_name);
        bool appInConfig = isAppInConfig(api, pkgStr);
        bool appNeedsCpuSpoof = isCpuSpoofApp(api, pkgStr);

        if (appInConfig || appNeedsCpuSpoof) {
            LOGI("GameUnlocker App Detected: %s", pkgStr.c_str());

            modulePath = resolveModulePath(api);
            if (appNeedsCpuSpoof && !modulePath.empty()) {
                std::string spoofPath = modulePath + "/cpuinfo_spoof";
                executeCompanionCommand("mount_spoof:" + spoofPath);
            }
        
            jclass buildClass = env->FindClass("android/os/Build");
            if (buildClass) {
                setStr(buildClass, "MANUFACTURER", "Asus");
                setStr(buildClass, "BRAND", "asus");
                setStr(buildClass, "MODEL", "ZS673KS");
                setStr(buildClass, "DEVICE", "ASUS_I005_1");
                setStr(buildClass, "PRODUCT", "WW_I005D");
                setStr(buildClass, "FINGERPRINT", "asus/WW_I005D/ASUS_I005_1:11/RKQ1.201022.002/18.0840.2103.26-0:user/release-keys");
                setStr(buildClass, "BRAND_FOR_DEVICE", "asus");
                env->DeleteLocalRef(buildClass);
            } else {
                env->ExceptionClear();
            }

            // We only patch static Build fields; library can be unloaded afterwards.
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    std::string modulePath;

    void setStr(jclass buildClass, const char* fieldName, const char* value) {
        jfieldID field = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
        if (field) {
            jstring js = env->NewStringUTF(value);
            env->SetStaticObjectField(buildClass, field, js);
            env->DeleteLocalRef(js);
        }
        env->ExceptionClear();
    }

    bool executeCompanionCommand(const std::string& command) {
        auto fd_conn = api->connectCompanion();
        if (fd_conn >= 0) {
            ssize_t written = write(fd_conn, command.c_str(), command.size());
            close(fd_conn);
            return written == static_cast<ssize_t>(command.size());
        }
        return false;
    }
};

REGISTER_ZYGISK_MODULE(GameUnlockerModule)
REGISTER_ZYGISK_COMPANION(companion)
