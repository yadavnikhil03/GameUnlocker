#pragma once
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>

#define GU_LOG_TAG "GameUnlocker/SELinux"
#define GU_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  GU_LOG_TAG, __VA_ARGS__)
#define GU_LOGW(...) __android_log_print(ANDROID_LOG_WARN,  GU_LOG_TAG, __VA_ARGS__)
#define GU_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, GU_LOG_TAG, __VA_ARGS__)

namespace gameunlocker {

namespace selinux {
enum class SysfsWriteResult {
    OK,           
    SKIPPED,      
    DENIED,       
    ERROR,        
};
inline SysfsWriteResult safe_sysfs_write(const char* path, const char* value) {
    if (access(path, F_OK) != 0) {
        return SysfsWriteResult::SKIPPED;
    }

    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            GU_LOGW("SELinux denial writing %s (errno=%d %s)", path, errno, strerror(errno));
            return SysfsWriteResult::DENIED;
        }
        GU_LOGE("Cannot open %s: errno=%d %s", path, errno, strerror(errno));
        return SysfsWriteResult::ERROR;
    }
    const size_t len = strlen(value);
    ssize_t written = write(fd, value, len);

    int save_errno = errno;
    close(fd);
    if (written < 0) {
        if (save_errno == EACCES || save_errno == EPERM) {
            GU_LOGW("SELinux denial on write to %s", path);
            return SysfsWriteResult::DENIED;
        }
        GU_LOGE("Write failed: %s (errno=%d %s)", path, save_errno, strerror(save_errno));
        return SysfsWriteResult::ERROR;
    }
    GU_LOGI("sysfs[%s] = '%s' (%zd bytes written)", path, value, written);
    return SysfsWriteResult::OK;
}
inline SysfsWriteResult safe_sysfs_write_int(const char* path, long value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", value);
    return safe_sysfs_write(path, buf);
}
inline bool safe_sysfs_read(const char* path, char* out, size_t out_size) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';
    if (access(path, R_OK) != 0) return false;

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            GU_LOGW("SELinux denial reading %s", path);
        }
        return false;
    }
    ssize_t n = read(fd, out, out_size - 1);
    close(fd);
    if (n <= 0) return false;
    out[n] = '\0';
    if (n > 0 && out[n - 1] == '\n') out[n - 1] = '\0';
    return true;
}
inline bool probe_sysfs_write(const char* path) {
    char current[64];
    if (!safe_sysfs_read(path, current, sizeof(current))) return false;
    auto result = safe_sysfs_write(path, current);
    return result == SysfsWriteResult::OK;
}
} 
} 