#include "GpuHook.hpp"
#include "logger.hpp"
#include "SysPropHook.hpp"
#include <dlfcn.h>
#include <bytehook.h>

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256

struct VkPhysicalDeviceProperties {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    uint8_t pipelineCacheUUID[16];
};

struct VkPhysicalDeviceProperties2 {
    uint32_t sType;
    void* pNext;
    VkPhysicalDeviceProperties properties;
};

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

static uint32_t spoofed_vk_vendor_id() {
    std::string vendor = spoofed_gl_vendor();
    if (vendor.find("Qualcomm") != std::string::npos) return 0x5143;
    if (vendor.find("ARM") != std::string::npos) return 0x13B5;
    if (vendor.find("Imagination") != std::string::npos) return 0x1010;
    return 0x5143; 
}

static uint32_t spoofed_vk_device_id() {
    std::string vendor = spoofed_gl_vendor();
    if (vendor.find("Qualcomm") != std::string::npos) return 0x07050000; 
    if (vendor.find("ARM") != std::string::npos) return 0x7150; // Generic Mali ID
    return 0x0;
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

typedef void (*native_vkGetPhysicalDeviceProperties_t)(void*, VkPhysicalDeviceProperties*);
static native_vkGetPhysicalDeviceProperties_t orig_vkGetPhysicalDeviceProperties = nullptr;

static void my_vkGetPhysicalDeviceProperties(void* physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    if (orig_vkGetPhysicalDeviceProperties) {
        orig_vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
    } else {
        BYTEHOOK_CALL_PREV(my_vkGetPhysicalDeviceProperties, physicalDevice, pProperties);
    }
    
    if (pProperties) {
        strncpy(pProperties->deviceName, spoofed_gl_renderer(), VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
        pProperties->deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';
        pProperties->vendorID = spoofed_vk_vendor_id();
        pProperties->deviceID = spoofed_vk_device_id();
    }
}

typedef void (*native_vkGetPhysicalDeviceProperties2_t)(void*, VkPhysicalDeviceProperties2*);
static native_vkGetPhysicalDeviceProperties2_t orig_vkGetPhysicalDeviceProperties2 = nullptr;

static void my_vkGetPhysicalDeviceProperties2(void* physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    if (orig_vkGetPhysicalDeviceProperties2) {
        orig_vkGetPhysicalDeviceProperties2(physicalDevice, pProperties);
    } else {
        BYTEHOOK_CALL_PREV(my_vkGetPhysicalDeviceProperties2, physicalDevice, pProperties);
    }
    
    if (pProperties) {
        strncpy(pProperties->properties.deviceName, spoofed_gl_renderer(), VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);
        pProperties->properties.deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1] = '\0';
        pProperties->properties.vendorID = spoofed_vk_vendor_id();
        pProperties->properties.deviceID = spoofed_vk_device_id();
    }
}

static void on_native_vk_hooked(bytehook_stub_t stub, int status, const char* caller_path,
                                 const char* sym_name, void* new_func, void* prev_func, void* arg) {
    if (status == BYTEHOOK_STATUS_CODE_OK) {
        if (prev_func) {
            if (strcmp(sym_name, "vkGetPhysicalDeviceProperties") == 0 && !orig_vkGetPhysicalDeviceProperties) {
                orig_vkGetPhysicalDeviceProperties = reinterpret_cast<native_vkGetPhysicalDeviceProperties_t>(prev_func);
            } else if ((strcmp(sym_name, "vkGetPhysicalDeviceProperties2") == 0 || strcmp(sym_name, "vkGetPhysicalDeviceProperties2KHR") == 0) && !orig_vkGetPhysicalDeviceProperties2) {
                orig_vkGetPhysicalDeviceProperties2 = reinterpret_cast<native_vkGetPhysicalDeviceProperties2_t>(prev_func);
            }
        }
        LOGI("GpuHook: native %s hook OK in '%s'", sym_name, caller_path ? caller_path : "?");
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

    bytehook_hook_all(nullptr, "vkGetPhysicalDeviceProperties",
                      reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties),
                      on_native_vk_hooked, nullptr);
                      
    bytehook_hook_all(nullptr, "vkGetPhysicalDeviceProperties2",
                      reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties2),
                      on_native_vk_hooked, nullptr);
                      
    bytehook_hook_all(nullptr, "vkGetPhysicalDeviceProperties2KHR",
                      reinterpret_cast<void*>(my_vkGetPhysicalDeviceProperties2),
                      on_native_vk_hooked, nullptr);

    LOGI("GpuHook: vendor='%s' renderer='%s' (JNI hooked=%d, native hooks requested)",
         spoofed_gl_vendor(), spoofed_gl_renderer(), anyJniHooked ? 1 : 0);

    // Return true even if JNI hooks weren't found — native hook may still work
    return true;
}

REGISTER_HOOK(GpuHook);

}
