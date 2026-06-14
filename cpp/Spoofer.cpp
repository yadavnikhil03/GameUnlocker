#include "Spoofer.hpp"
#include "raii.hpp"
#include <cstring>

namespace gameunlocker {

Spoofer::Spoofer(const Context& ctx) : ctx_(ctx) {}

void Spoofer::setStringField(jclass buildClass, const char* fieldName, const std::string& value) {
    if (value.empty()) return;

    JNIEnv* env = ctx_.getEnv();
    if (!env) return;

    jfieldID field = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");
    if (field) {
        jstring js = env->NewStringUTF(value.c_str());
        env->SetStaticObjectField(buildClass, field, js);
        env->DeleteLocalRef(js);
    }
    env->ExceptionClear();
}

void Spoofer::applyDeviceSpoof(const DeviceProfile& profile) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) return;

    jclass buildClass = env->FindClass("android/os/Build");
    if (!buildClass) {
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
