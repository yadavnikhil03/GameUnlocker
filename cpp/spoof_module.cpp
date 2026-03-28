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
#include "zygisk.hpp"

#define LOG_TAG "GameUnlocker"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static const char* spoof_file_path = "/data/adb/modules/Game-Unlocker/cpuinfo_spoof";

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

bool isAppInConfig(zygisk::Api* api, const std::string& appName) {
    if (!api) return false;
    int dirfd = api->getModuleDir();
    if (dirfd < 0) return false;
    
    int fd = openat(dirfd, "config.json", O_RDONLY);
    if (fd < 0) return false;
    
    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = read(fd, buffer, sizeof(buffer)-1);
    close(fd);
    
    if (bytes <= 0) return false;
    
    std::string content(buffer);
    return content.find("\"" + appName + "\"") != std::string::npos;
}

static void companion(int fd) {
    char buffer[256];
    ssize_t bytes = read(fd, buffer, sizeof(buffer)-1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::string command = buffer;
        if (command == "mount_spoof") {
            if (access(spoof_file_path, F_OK) == 0) {
                system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
                char mount_cmd[512];
                snprintf(mount_cmd, sizeof(mount_cmd), "/system/bin/mount --bind %s /proc/cpuinfo", spoof_file_path);
                system(mount_cmd);
                LOGI("CPU Mount Companion Success");
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
        JniString pkg(env, args->nice_name);
        const char* package_name = pkg.get();
        if (!package_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string pkgStr(package_name);
        if (isAppInConfig(api, pkgStr)) {
            LOGI("GameUnlocker App Detected: %s", package_name);
            executeCompanionCommand("mount_spoof");
            
            // Apply Spoof to iQOO
            jclass buildClass = env->FindClass("android/os/Build");
            if (buildClass) {
                setStr(buildClass, "MANUFACTURER", "vivo");
                setStr(buildClass, "BRAND", "vivo");
                setStr(buildClass, "MODEL", "V2302A");
                setStr(buildClass, "DEVICE", "V2302A");
                setStr(buildClass, "PRODUCT", "V2302A");
                setStr(buildClass, "FINGERPRINT", "vivo/V2302A/V2302A:13/TP1A.220624.014/compil02251912:user/release-keys");
            }
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api;
    JNIEnv* env;

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
            write(fd_conn, command.c_str(), command.size());
            close(fd_conn);
            return true;
        }
        return false;
    }
};

REGISTER_ZYGISK_MODULE(GameUnlockerModule)
REGISTER_ZYGISK_COMPANION(companion)
