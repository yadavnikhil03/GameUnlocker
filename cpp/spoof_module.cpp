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

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

static jstring (*orig_GLES20_glGetString)(JNIEnv*, jclass, jint);
static jstring my_GLES20_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF("Qualcomm");
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF("Adreno (TM) 750");
    }
    if (orig_GLES20_glGetString) {
        return orig_GLES20_glGetString(env, clazz, name);
    }
    return nullptr;
}

static jstring (*orig_GLES30_glGetString)(JNIEnv*, jclass, jint);
static jstring my_GLES30_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF("Qualcomm");
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF("Adreno (TM) 750");
    }
    if (orig_GLES30_glGetString) {
        return orig_GLES30_glGetString(env, clazz, name);
    }
    return nullptr;
}

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

    char buffer[16384]; 
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

static bool isAppBlacklisted(const std::string& content, const std::string& appName) {
    size_t cpuSection = content.find("\"cpu_spoof\"");
    if (cpuSection == std::string::npos) return false;

    size_t blacklistSection = content.find("\"blacklist\"", cpuSection);
    if (blacklistSection == std::string::npos) return false;

    size_t openBracket = content.find('[', blacklistSection);
    size_t closeBracket = content.find(']', openBracket);
    if (openBracket != std::string::npos && closeBracket != std::string::npos) {
        std::string list = content.substr(openBracket, closeBracket - openBracket + 1);
        if (containsToken(list, appName)) return true;
    }
    return false;
}

static bool isCpuSpoofApp(const std::string& content, const std::string& appName) {
    size_t cpuSection = content.find("\"cpu_spoof\"");
    if (cpuSection == std::string::npos) return false;

    size_t withCpu = content.find("\"with_cpu\"", cpuSection);
    if (withCpu == std::string::npos) return false;

    size_t openBracket = content.find('[', withCpu);
    size_t closeBracket = content.find(']', openBracket);
    if (openBracket != std::string::npos && closeBracket != std::string::npos) {
        std::string list = content.substr(openBracket, closeBracket - openBracket + 1);
        if (containsToken(list, appName)) return true;
    }
    return false;
}

static std::string extractDeviceProfile(const std::string& content, const std::string& appName) {
    std::vector<std::string> profiles = {
        "SAMSUNG_S24_ULTRA",
        "REDMAGIC_9_PRO",
        "XIAOMI_11T_PRO",
        "PIXEL_9_PRO"
    };
    
    for (const auto& profile : profiles) {
        std::string arrayKey = "\"PACKAGES_" + profile + "\":";
        size_t arrayStart = content.find(arrayKey);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = content.find("]", arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayContent = content.substr(arrayStart, arrayEnd - arrayStart);
                if (containsToken(arrayContent, appName)) {
                    return profile;
                }
            }
        }
    }
    return "";
}

static std::string extractDeviceValue(const std::string& content, const std::string& profile, const std::string& key) {
    std::string deviceKey = "\"PACKAGES_" + profile + "_DEVICE\":";
    size_t devStart = content.find(deviceKey);
    if (devStart != std::string::npos) {
        size_t objEnd = content.find("}", devStart);
        if (objEnd != std::string::npos) {
            std::string objContent = content.substr(devStart, objEnd - devStart);
            std::string searchKey = "\"" + key + "\":";
            size_t keyStart = objContent.find(searchKey);
            if (keyStart != std::string::npos) {
                size_t valStart = objContent.find("\"", keyStart + searchKey.length());
                if (valStart != std::string::npos) {
                    size_t valEnd = objContent.find("\"", valStart + 1);
                    if (valEnd != std::string::npos) {
                        return objContent.substr(valStart + 1, valEnd - valStart - 1);
                    }
                }
            }
        }
    }
    return "";
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
        if (command == "unmount_spoof") {
            system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
            LOGI("CPU Mount Companion: Unmounted CPU info");
        } else if (command.rfind("mount_spoof:", 0) == 0) {
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
                LOGI("CPU Mount Companion: Mounted CPU info");
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
        std::string content;
        
        if (!readConfig(api, content)) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (isAppBlacklisted(content, pkgStr)) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string profile = extractDeviceProfile(content, pkgStr);
        bool appNeedsCpuSpoof = isCpuSpoofApp(content, pkgStr);

        if (!profile.empty() || appNeedsCpuSpoof) {
            LOGI("GameUnlocker Target Detected: %s [Profile: %s]", pkgStr.c_str(), profile.empty() ? "None" : profile.c_str());

            modulePath = resolveModulePath(api);
            if (appNeedsCpuSpoof && !modulePath.empty()) {
                std::string spoofPath = modulePath + "/cpuinfo_spoof";
                executeCompanionCommand("mount_spoof:" + spoofPath);
            } else if (!appNeedsCpuSpoof) {
                // For non-game apps, explicitly unmount just in case it persisted
                executeCompanionCommand("unmount_spoof");
            }
        
            if (!profile.empty()) {
                jclass buildClass = env->FindClass("android/os/Build");
                if (buildClass) {
                    setStr(buildClass, "MANUFACTURER", extractDeviceValue(content, profile, "MANUFACTURER").c_str());
                    setStr(buildClass, "BRAND", extractDeviceValue(content, profile, "BRAND").c_str());
                    setStr(buildClass, "MODEL", extractDeviceValue(content, profile, "MODEL").c_str());
                    setStr(buildClass, "DEVICE", extractDeviceValue(content, profile, "DEVICE").c_str());
                    setStr(buildClass, "PRODUCT", extractDeviceValue(content, profile, "PRODUCT").c_str());
                    setStr(buildClass, "FINGERPRINT", extractDeviceValue(content, profile, "FINGERPRINT").c_str());
                    setStr(buildClass, "BRAND_FOR_DEVICE", extractDeviceValue(content, profile, "BRAND").c_str());
                    env->DeleteLocalRef(buildClass);
                } else {
                    env->ExceptionClear();
                }

                LOGI("Injecting Advanced GPU Spoofing (Adreno 750)");
                JNINativeMethod gles20_methods[] = {
                    {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES20_glGetString}
                };
                api->hookJniNativeMethods(env, "android/opengl/GLES20", gles20_methods, 1);
                *(void **)&orig_GLES20_glGetString = gles20_methods[0].fnPtr;

                JNINativeMethod gles30_methods[] = {
                    {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES30_glGetString}
                };
                api->hookJniNativeMethods(env, "android/opengl/GLES30", gles30_methods, 1);
                *(void **)&orig_GLES30_glGetString = gles30_methods[0].fnPtr;
            }

            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        } else {
            executeCompanionCommand("unmount_spoof");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    std::string modulePath;

    void setStr(jclass buildClass, const char* fieldName, const char* value) {
        if (!value || strlen(value) == 0) return;
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
