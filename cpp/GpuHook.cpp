#include "GpuHook.hpp"
#include "logger.hpp"
#include <dlfcn.h>

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

namespace gameunlocker {

static jstring (*orig_GLES20_glGetString)(JNIEnv*, jclass, jint);
static jstring (*orig_GLES30_glGetString)(JNIEnv*, jclass, jint);

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

bool GpuHook::onEnable(const Context& ctx) {
    JNIEnv* env = ctx.getEnv();
    zygisk::Api* api = ctx.getApi();

    if (!env || !api) return false;

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

    return true;
}

void GpuHook::onDisable(const Context& ctx) {
}

REGISTER_HOOK(GpuHook);

} 
