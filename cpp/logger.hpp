#pragma once

#include <android/log.h>

#define LOG_TAG "GameUnlocker"

#define LOG_ENABLE 1 

#if LOG_ENABLE
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#define LOGD(...)
#endif
