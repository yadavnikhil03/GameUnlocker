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

static void parseFingerprintParts(const std::string& fp,
                                  std::string& buildId,
                                  std::string& display) {
    buildId.clear();
    display.clear();
    if (fp.empty()) return;

    size_t first = fp.find('/');
    if (first == std::string::npos) return;
    size_t second = fp.find('/', first + 1);
    if (second == std::string::npos) return;
    size_t third = fp.find('/', second + 1);
    if (third == std::string::npos) return;
    size_t fourth = fp.find('/', third + 1);
    if (fourth == std::string::npos) return;

    buildId.assign(fp, third + 1, fourth - third - 1);

    size_t lastSlash = fp.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < fp.size()) {
        display.assign(fp, lastSlash + 1, std::string::npos);
    }
}

void Spoofer::applyDeviceSpoof(const DeviceProfile& profile) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::applyDeviceSpoof failed: JNIEnv is null");
        return;
    }

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

    if (profile.brand_for_device.has_value()) {
        setStringField(buildClass, "BRAND_FOR_DEVICE", profile.brand_for_device.value());
    }

    std::string buildId, display;
    parseFingerprintParts(profile.fingerprint, buildId, display);
    if (!buildId.empty()) setStringField(buildClass, "ID", buildId);
    if (!display.empty()) setStringField(buildClass, "DISPLAY", display);

    LOGI("Spoofer: Successfully applied Java device spoofing profile (Model: %s)", profile.model.c_str());
}

}
