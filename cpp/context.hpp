#pragma once

#include <jni.h>
#include "zygisk.hpp"

namespace gameunlocker {

class Context {
public:
    Context(zygisk::Api* api, JNIEnv* env) : api_(api), env_(env) {}

    zygisk::Api* getApi() const { return api_; }
    JNIEnv* getEnv() const { return env_; }

    int getModuleDirFd() const {
        if (api_) {
            return api_->getModuleDir();
        }
        return -1;
    }

private:
    zygisk::Api* api_;
    JNIEnv* env_;
};

} // namespace gameunlocker
