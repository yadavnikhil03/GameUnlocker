#include "GpuHook.hpp"
#include "logger.hpp"
#include <dlfcn.h>

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

#include "SysPropHook.hpp"

namespace gameunlocker {

static jstring (*orig_GLES20_glGetString)(JNIEnv*, jclass, jint);
static jstring (*orig_GLES30_glGetString)(JNIEnv*, jclass, jint);
static jstring (*orig_GLES31_glGetString)(JNIEnv*, jclass, jint);
static jstring (*orig_GLES32_glGetString)(JNIEnv*, jclass, jint);

static const char* spoofed_gl_vendor() {
    auto profileOpt = SysPropHook::getProfile();
    if (profileOpt.has_value() && !profileOpt.value().gl_vendor.empty()) {
        return profileOpt.value().gl_vendor.c_str();
    }
    return "Qualcomm";
}

static const char* spoofed_gl_renderer() {
    auto profileOpt = SysPropHook::getProfile();
    if (profileOpt.has_value() && !profileOpt.value().gl_renderer.empty()) {
        return profileOpt.value().gl_renderer.c_str();
    }
    return "Adreno (TM) 750";
}

static jstring my_GLES20_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF(spoofed_gl_vendor());
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF(spoofed_gl_renderer());
    }
    if (orig_GLES20_glGetString) {
        return orig_GLES20_glGetString(env, clazz, name);
    }
    return nullptr;
}

static jstring my_GLES30_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF(spoofed_gl_vendor());
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF(spoofed_gl_renderer());
    }
    if (orig_GLES30_glGetString) {
        return orig_GLES30_glGetString(env, clazz, name);
    }
    return nullptr;
}

static jstring my_GLES31_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF(spoofed_gl_vendor());
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF(spoofed_gl_renderer());
    }
    if (orig_GLES31_glGetString) {
        return orig_GLES31_glGetString(env, clazz, name);
    }
    return nullptr;
}

static jstring my_GLES32_glGetString(JNIEnv* env, jclass clazz, jint name) {
    if (name == GL_VENDOR) {
        return env->NewStringUTF(spoofed_gl_vendor());
    } else if (name == GL_RENDERER) {
        return env->NewStringUTF(spoofed_gl_renderer());
    }
    if (orig_GLES32_glGetString) {
        return orig_GLES32_glGetString(env, clazz, name);
    }
    return nullptr;
}

bool GpuHook::onEnable(const Context& ctx) {
    JNIEnv* env = ctx.getEnv();
    zygisk::Api* api = ctx.getApi();

    if (!env || !api) return false;

    JNINativeMethod gles20_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES20_glGetString}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES20", gles20_methods, 1);
    if (gles20_methods[0].fnPtr) {
        *(void **)&orig_GLES20_glGetString = gles20_methods[0].fnPtr;
    } else {
        LOGW("GpuHook: GLES20 glGetString hook target not found");
    }

    JNINativeMethod gles30_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES30_glGetString}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES30", gles30_methods, 1);
    if (gles30_methods[0].fnPtr) {
        *(void **)&orig_GLES30_glGetString = gles30_methods[0].fnPtr;
    } else {
        LOGW("GpuHook: GLES30 glGetString hook target not found");
    }

    JNINativeMethod gles31_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES31_glGetString}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES31", gles31_methods, 1);
    if (gles31_methods[0].fnPtr) {
        *(void **)&orig_GLES31_glGetString = gles31_methods[0].fnPtr;
    }

    JNINativeMethod gles32_methods[] = {
        {"glGetString", "(I)Ljava/lang/String;", (void*)my_GLES32_glGetString}
    };
    api->hookJniNativeMethods(env, "android/opengl/GLES32", gles32_methods, 1);
    if (gles32_methods[0].fnPtr) {
        *(void **)&orig_GLES32_glGetString = gles32_methods[0].fnPtr;
    }

    return orig_GLES20_glGetString != nullptr || orig_GLES30_glGetString != nullptr ||
           orig_GLES31_glGetString != nullptr || orig_GLES32_glGetString != nullptr;
}



REGISTER_HOOK(GpuHook);

} 
