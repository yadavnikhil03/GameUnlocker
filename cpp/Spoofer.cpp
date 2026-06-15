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
        LOGE("Spoofer::setStringField failed: Could not find field %s", fieldName);
        env->ExceptionClear();
        return;
    }

    jstring js = env->NewStringUTF(value.c_str());
    if (!js) {
        LOGE("Spoofer::setStringField failed: Could not allocate string for %s", fieldName);
        env->ExceptionClear();
        return;
    }

    env->SetStaticObjectField(buildClass, field, js);
    env->DeleteLocalRef(js);
    env->ExceptionClear();
}

void Spoofer::applyDeviceSpoof(const DeviceProfile& profile) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::applyDeviceSpoof failed: JNIEnv is null");
        return;
    }

    jclass buildClass = env->FindClass("android/os/Build");
    if (!buildClass) {
        LOGE("Spoofer::applyDeviceSpoof failed: Could not find android/os/Build class");
        env->ExceptionClear();
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
}

} 
