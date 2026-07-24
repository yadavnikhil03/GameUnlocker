#include "GpuHook.hpp"
#include "logger.hpp"
#include "SysPropHook.hpp"
#include <dlfcn.h>
#include <bytehook.h>

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

namespace gameunlocker {

// ----------------------------------------------------------------
// Cached spoofed GL strings (read from active profile)
// Using static storage so the returned c_str() survives the function.
// The profile is set once during preAppSpecialize and never changes,
// so a one-shot cache is fine.
// ----------------------------------------------------------------

static const char* spoofed_gl_vendor() {
    static std::string cached;
    if (cached.empty()) {
        auto profileOpt = SysPropHook::getProfile();
        if (profileOpt.has_value() && !profileOpt.value().gl_vendor.empty()) {
            cached = profileOpt.value().gl_vendor;
        } else {
            cached = "Qualcomm";
        }
    }
    return cached.c_str();
}

static const char* spoofed_gl_renderer() {
    static std::string cached;
    if (cached.empty()) {
        auto profileOpt = SysPropHook::getProfile();
        if (profileOpt.has_value() && !profileOpt.value().gl_renderer.empty()) {
            cached = profileOpt.value().gl_renderer;
        } else {
            cached = "Adreno (TM) 750";
        }
    }
    return cached.c_str();
}

// ----------------------------------------------------------------
// JNI-layer hooks — intercept android.opengl.GLES{20,30,31,32}
// ----------------------------------------------------------------

static jstring (*orig_GLES20_glGetString)(JNIEnv*, jclass, jint) = nullptr;
static jstring (*orig_GLES30_glGetString)(JNIEnv*, jclass, jint) = nullptr;
static jstring (*orig_GLES31_glGetString)(JNIEnv*, jclass, jint) = nullptr;
static jstring (*orig_GLES32_glGetString)(JNIEnv*, jclass, jint) = nullptr;

static jstring my_GLES20_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES20_glGetString) return orig_GLES20_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES30_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES30_glGetString) return orig_GLES30_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES31_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES31_glGetString) return orig_GLES31_glGetString(env, clazz, name);
    return nullptr;
}

static jstring my_GLES32_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR)   return env->NewStringUTF(spoofed_gl_vendor());
    if (name == GL_RENDERER) return env->NewStringUTF(spoofed_gl_renderer());
    if (orig_GLES32_glGetString) return orig_GLES32_glGetString(env, clazz, name);
    return nullptr;
}

// ----------------------------------------------------------------
// Native-layer hook — intercepts glGetString from libGLESv2.so
// Games using NDK directly (EGL path) bypass the JNI layer entirely.
// ----------------------------------------------------------------

typedef const unsigned char* (*native_glGetString_t)(unsigned int);
static native_glGetString_t orig_native_glGetString = nullptr;

static const unsigned char* my_native_glGetString(unsigned int name) {
    if (name == GL_VENDOR) {
        return reinterpret_cast<const unsigned char*>(spoofed_gl_vendor());
    }
    if (name == GL_RENDERER) {
        return reinterpret_cast<const unsigned char*>(spoofed_gl_renderer());
    }
    if (orig_native_glGetString) {
        return orig_native_glGetString(name);
    }
    BYTEHOOK_CALL_PREV(my_native_glGetString, name);
    return nullptr;
}

static void on_native_gl_hooked(bytehook_stub_t stub, int status, const char* caller_path,
                                 const char* sym_name, void* new_func, void* prev_func, void* arg) {
    if (status == BYTEHOOK_STATUS_CODE_OK) {
        if (prev_func && !orig_native_glGetString) {
            orig_native_glGetString = reinterpret_cast<native_glGetString_t>(prev_func);
        }
        LOGI("GpuHook: native glGetString hook OK in '%s' (prev=%p)",
             caller_path ? caller_path : "?", prev_func);
    } else {
        LOGW("GpuHook: native glGetString hook failed in '%s' (status=%d)",
             caller_path ? caller_path : "?", status);
    }
}

// ----------------------------------------------------------------
// onEnable
// ----------------------------------------------------------------

bool GpuHook::onEnable(const Context& ctx) {
    JNIEnv* env = ctx.getEnv();
    zygisk::Api* api = ctx.getApi();

    if (!env || !api) {
        LOGE("GpuHook: JNIEnv or Api is null");
        return false;
    }

    bool anyJniHooked = false;

    // Hook GLES20
    JNINativeMethod gles20_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES20_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES20", gles20_methods, 1);
    if (gles20_methods[0].fnPtr) {
        orig_GLES20_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles20_methods[0].fnPtr);
        anyJniHooked = true;
    } else {
        LOGW("GpuHook: GLES20 glGetString hook target not found");
    }

    // Hook GLES30
    JNINativeMethod gles30_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES30_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES30", gles30_methods, 1);
    if (gles30_methods[0].fnPtr) {
        orig_GLES30_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles30_methods[0].fnPtr);
        anyJniHooked = true;
    } else {
        LOGW("GpuHook: GLES30 glGetString hook target not found");
    }

    // Hook GLES31
    JNINativeMethod gles31_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES31_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES31", gles31_methods, 1);
    if (gles31_methods[0].fnPtr) {
        orig_GLES31_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles31_methods[0].fnPtr);
        anyJniHooked = true;
    }

    // Hook GLES32
    JNINativeMethod gles32_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", reinterpret_cast<void*>(my_GLES32_glGetString)}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES32", gles32_methods, 1);
    if (gles32_methods[0].fnPtr) {
        orig_GLES32_glGetString = reinterpret_cast<jstring (*)(JNIEnv*, jclass, jint)>(gles32_methods[0].fnPtr);
        anyJniHooked = true;
    }

    // ---- Native (EGL path) hook via bytehook ----
    // bytehook_init is already called by SysPropHook which registers first;
    // calling again is safe (idempotent).
    bytehook_init(BYTEHOOK_MODE_AUTOMATIC, false);

    bytehook_hook_all(nullptr, "glGetString",
                      reinterpret_cast<void*>(my_native_glGetString),
                      on_native_gl_hooked, nullptr);

    LOGI("GpuHook: vendor='%s' renderer='%s' (JNI hooked=%d, native hook requested)",
         spoofed_gl_vendor(), spoofed_gl_renderer(), anyJniHooked ? 1 : 0);

    // Return true even if JNI hooks weren't found — native hook may still work
    return true;
}

REGISTER_HOOK(GpuHook);

} // namespace gameunlocker
