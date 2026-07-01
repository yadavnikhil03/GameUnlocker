#pragma once

#include <jni.h>
#include <unistd.h>

namespace gameunlocker {

class JniString {
public:
    JniString(JNIEnv* env, jstring s) : env_(env), jstr_(s), chars_(nullptr) {
        if (jstr_) {
            chars_ = env_->GetStringUTFChars(jstr_, nullptr);
        }
    }

    ~JniString() {
        if (jstr_ && chars_) {
            env_->ReleaseStringUTFChars(jstr_, chars_);
        }
    }

    const char* get() const { return chars_; }

    JniString(const JniString&) = delete;
    JniString& operator=(const JniString&) = delete;

private:
    JNIEnv* env_;
    jstring jstr_;
    const char* chars_;
};

class FdWrapper {
public:
    explicit FdWrapper(int fd) : fd_(fd) {}

    ~FdWrapper() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int get() const { return fd_; }
    bool isValid() const { return fd_ >= 0; }

    FdWrapper(const FdWrapper&) = delete;
    FdWrapper& operator=(const FdWrapper&) = delete;

private:
    int fd_;
};

template<typename T>
class ScopedLocalRef {
public:
    ScopedLocalRef(JNIEnv* env, T ref) : env_(env), ref_(ref) {}

    ~ScopedLocalRef() {
        if (ref_) {
            env_->DeleteLocalRef(ref_);
        }
    }

    T get() const { return ref_; }
    bool isValid() const { return ref_ != nullptr; }

    ScopedLocalRef(const ScopedLocalRef&) = delete;
    ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

private:
    JNIEnv* env_;
    T ref_;
};

} 
