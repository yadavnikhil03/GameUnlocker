#include "Spoofer.hpp"
#include "raii.hpp"
#include "logger.hpp"
#include <cstring>

namespace gameunlocker {

Spoofer::Spoofer(const Context& ctx) : ctx_(ctx) {}

void Spoofer::setStringField(jclass clazz, const char* fieldName, const std::string& value) {
    if (value.empty()) return;

    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::setStringField: JNIEnv is null (field=%s)", fieldName);
        return;
    }

    jfieldID field = env->GetStaticFieldID(clazz, fieldName, "Ljava/lang/String;");
    if (!field) {
        env->ExceptionClear();
        return;
    }

    jstring js = env->NewStringUTF(value.c_str());
    if (!js) {
        env->ExceptionClear();
        return;
    }

    env->SetStaticObjectField(clazz, field, js);
    env->DeleteLocalRef(js);
    env->ExceptionClear();
}

void Spoofer::setIntField(jclass clazz, const char* fieldName, jint value) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) return;

    jfieldID field = env->GetStaticFieldID(clazz, fieldName, "I");
    if (!field) {
        env->ExceptionClear();
        return;
    }
    env->SetStaticIntField(clazz, field, value);
    env->ExceptionClear();
}

// Parse fingerprint: brand/product/device:VERSION/BUILD_ID/INCREMENTAL:type/tags
// Extracts: buildId (3rd→4th slash), versionRelease (after ':' to next '/'),
//           buildType ("user"/"userdebug"), buildTags ("release-keys")
static void parseFingerprintParts(const std::string& fp,
                                  std::string& buildId,
                                  std::string& versionRelease,
                                  std::string& buildType,
                                  std::string& buildTags) {
    buildId.clear();
    versionRelease.clear();
    buildType.clear();
    buildTags.clear();

    if (fp.empty()) return;

    // Version: between first ':' and the '/' that follows it
    size_t colonPos = fp.find(':');
    if (colonPos != std::string::npos) {
        size_t slashAfterVer = fp.find('/', colonPos + 1);
        if (slashAfterVer != std::string::npos) {
            versionRelease.assign(fp, colonPos + 1, slashAfterVer - colonPos - 1);
        }
    }

    // Build ID: between 3rd and 4th slash
    size_t f1 = fp.find('/');
    if (f1 == std::string::npos) return;
    size_t f2 = fp.find('/', f1 + 1);
    if (f2 == std::string::npos) return;
    size_t f3 = fp.find('/', f2 + 1);
    if (f3 == std::string::npos) return;
    size_t f4 = fp.find('/', f3 + 1);
    if (f4 == std::string::npos) {
        buildId.assign(fp, f3 + 1, std::string::npos);
        return;
    }
    buildId.assign(fp, f3 + 1, f4 - f3 - 1);

    // Build type and tags: "INCREMENTAL:type/tags" — 5th segment onward
    // Find the second ':' (the one after INCREMENTAL, before type)
    size_t f5 = fp.find('/', f4 + 1);
    if (f5 != std::string::npos) {
        // Between f4+1 and f5 is "INCREMENTAL:type"
        size_t colon2 = fp.find(':', f4 + 1);
        if (colon2 != std::string::npos && colon2 < f5) {
            buildType.assign(fp, colon2 + 1, f5 - colon2 - 1);
        }
        // After f5 is the tags string
        buildTags.assign(fp, f5 + 1, std::string::npos);
    }
}


void Spoofer::applyDeviceSpoof(const DeviceProfile& profile) {
    JNIEnv* env = ctx_.getEnv();
    if (!env) {
        LOGE("Spoofer::applyDeviceSpoof: JNIEnv is null");
        return;
    }

    // ---------------------------------------------------------------
    // android.os.Build fields
    // ---------------------------------------------------------------
    jclass buildClass = env->FindClass("android/os/Build");
    if (!buildClass) {
        env->ExceptionClear();
        LOGE("Spoofer: could not find android/os/Build");
        return;
    }
    ScopedLocalRef<jclass> scopedBuild(env, buildClass);

    setStringField(buildClass, "MANUFACTURER", profile.manufacturer);
    setStringField(buildClass, "BRAND",        profile.brand);
    setStringField(buildClass, "MODEL",        profile.model);
    setStringField(buildClass, "DEVICE",       profile.device);
    setStringField(buildClass, "PRODUCT",      profile.product);
    setStringField(buildClass, "FINGERPRINT",  profile.fingerprint);

    if (!profile.board.empty())    setStringField(buildClass, "BOARD",    profile.board);
    if (!profile.hardware.empty()) setStringField(buildClass, "HARDWARE", profile.hardware);

    if (profile.brand_for_device.has_value()) {
        setStringField(buildClass, "BRAND_FOR_DEVICE", profile.brand_for_device.value());
    }

    // Parse fingerprint for build metadata
    std::string buildId, versionRelease, buildType, buildTags;
    parseFingerprintParts(profile.fingerprint, buildId, versionRelease, buildType, buildTags);

    if (!buildId.empty()) {
        setStringField(buildClass, "ID",      buildId);
        setStringField(buildClass, "DISPLAY", buildId);
    }

    // Build type / tags from fingerprint (or sensible defaults)
    std::string type = buildType.empty() ? "user" : buildType;
    std::string tags = buildTags.empty() ? "release-keys" : buildTags;
    setStringField(buildClass, "TYPE", type);
    setStringField(buildClass, "TAGS", tags);

    // ---------------------------------------------------------------
    // android.os.Build$VERSION fields
    // ---------------------------------------------------------------
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (!versionClass) {
        env->ExceptionClear();
        LOGW("Spoofer: could not find android/os/Build$VERSION");
    } else {
        ScopedLocalRef<jclass> scopedVersion(env, versionClass);
        if (!profile.security_patch.empty()) {
            setStringField(versionClass, "SECURITY_PATCH", profile.security_patch);
        }
    }

    LOGI("Spoofer: applied spoof — model=%s board=%s patch=%s",
         profile.model.c_str(),
         profile.board.empty()           ? "(none)" : profile.board.c_str(),
         profile.security_patch.empty()  ? "(none)" : profile.security_patch.c_str());
}

} // namespace gameunlocker
