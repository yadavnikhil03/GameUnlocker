#include "Spoofer.hpp"
#include "raii.hpp"
#include "logger.hpp"
#include <cstring>

namespace gameunlocker {

Spoofer::Spoofer(const Context& ctx) : ctx_(ctx) {}

void Spoofer::setStringField(jclass buildClass, const char* fieldName, const std::string& value) {
    if (value.empty()) return;

    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::setStringField failed: JNIEnv is null for %s", fieldName);
        return;
    }

    jfieldID field = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
    if (!field) {
        env->ExceptionClear();
        return;
    }

    jstring js = env->NewStringUTF(value.c_str());
    if (!js) {
        env->ExceptionClear();
        return;
    }

    env->SetStaticObjectField(buildClass, field, js);
    env->DeleteLocalRef(js);
    env->ExceptionClear();
}

// Parse useful fields from a standard Android fingerprint string.
// Format: brand/product/device:version/buildId/incremental:type/tags
// Example: samsung/e3qxx/e3q:14/UP1A.231005.007/S928BXXS1AXBG:user/release-keys
static void parseFingerprintParts(const std::string& fp,
                                  std::string& buildId,
                                  std::string& display,
                                  std::string& versionRelease) {
    buildId.clear();
    display.clear();
    versionRelease.clear();
    if (fp.empty()) return;

    // Extract version from "device:VERSION/" segment
    size_t colonPos = fp.find(':');
    if (colonPos != std::string::npos) {
        size_t slashAfterVer = fp.find('/', colonPos + 1);
        if (slashAfterVer != std::string::npos) {
            versionRelease.assign(fp, colonPos + 1, slashAfterVer - colonPos - 1);
        }
    }

    size_t first = fp.find('/');
    if (first == std::string::npos) return;
    size_t second = fp.find('/', first + 1);
    if (second == std::string::npos) return;
    size_t third = fp.find('/', second + 1);
    if (third == std::string::npos) return;
    size_t fourth = fp.find('/', third + 1);
    if (fourth == std::string::npos) return;

    // buildId = segment between 3rd and 4th slash
    // e.g., "UP1A.231005.007" from samsung/e3qxx/e3q:14/UP1A.231005.007/...
    buildId.assign(fp, third + 1, fourth - third - 1);

    // DISPLAY = buildId (the human-readable build tag, not "release-keys")
    display = buildId;
}

void Spoofer::applyDeviceSpoof(const DeviceProfile& profile) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::applyDeviceSpoof failed: JNIEnv is null");
        return;
    }

    // --- android.os.Build fields ---
    jclass buildClass = env->FindClass("android/os/Build");
    if (!buildClass) {
        env->ExceptionClear();
        LOGE("Spoofer::applyDeviceSpoof failed: Could not find android/os/Build class");
        return;
    }
    ScopedLocalRef<jclass> scopedBuildClass(env, buildClass);

    setStringField(buildClass, "MANUFACTURER", profile.manufacturer);
    setStringField(buildClass, "BRAND", profile.brand);
    setStringField(buildClass, "MODEL", profile.model);
    setStringField(buildClass, "DEVICE", profile.device);
    setStringField(buildClass, "PRODUCT", profile.product);
    setStringField(buildClass, "FINGERPRINT", profile.fingerprint);

    if (!profile.board.empty()) {
        setStringField(buildClass, "BOARD", profile.board);
    }
    if (!profile.hardware.empty()) {
        setStringField(buildClass, "HARDWARE", profile.hardware);
    }

    if (profile.brand_for_device.has_value()) {
        setStringField(buildClass, "BRAND_FOR_DEVICE", profile.brand_for_device.value());
    }

    std::string buildId, display, versionRelease;
    parseFingerprintParts(profile.fingerprint, buildId, display, versionRelease);
    if (!buildId.empty()) setStringField(buildClass, "ID", buildId);
    if (!display.empty()) setStringField(buildClass, "DISPLAY", display);

    // --- android.os.Build$VERSION fields ---
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (versionClass) {
        ScopedLocalRef<jclass> scopedVersionClass(env, versionClass);

        if (!versionRelease.empty()) {
            setStringField(versionClass, "RELEASE", versionRelease);

            // Map RELEASE to SDK_INT
            int sdkInt = 0;
            if (versionRelease == "15") sdkInt = 35;
            else if (versionRelease == "14") sdkInt = 34;
            else if (versionRelease == "13") sdkInt = 33;
            else if (versionRelease == "12") sdkInt = 32;

            if (sdkInt > 0) {
                jfieldID sdkField = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
                if (sdkField) {
                    env->SetStaticIntField(versionClass, sdkField, sdkInt);
                }
                env->ExceptionClear();
            }
        }
    } else {
        env->ExceptionClear();
    }

    LOGI("Spoofer: Applied device spoof (Model: %s, Board: %s, Android: %s)",
         profile.model.c_str(),
         profile.board.empty() ? "(default)" : profile.board.c_str(),
         versionRelease.empty() ? "(default)" : versionRelease.c_str());
}

}
