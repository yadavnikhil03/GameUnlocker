#pragma once

#include <jni.h>
#include "zygisk.hpp"

extern JavaVM* g_vm;

namespace gameunlocker {

class Context {
public:
    Context(zygisk::Api* api, JNIEnv* env) : api_(api), env_(env) {}
    
    ~Context() {
        if (g_vm && attached_) {
            g_vm->DetachCurrentThread();
        }
    }

    zygisk::Api* getApi() const { return api_; }
    
    JNIEnv* getEnv() const { 
        if (g_vm) {
            JNIEnv* env = nullptr;
            if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
                return env;
            }
            if (g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached_ = true;
                return env;
            }
        }
        return env_; 
    }

    int getModuleDirFd() const {
        if (api_) {
            return api_->getModuleDir();
        }
        return -1;
    }

private:
    zygisk::Api* api_;
    JNIEnv* env_;
    mutable bool attached_ = false;
};

} // namespace gameunlocker
