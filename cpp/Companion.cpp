#include "Companion.hpp"
#include "logger.hpp"
#include "raii.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <sys/stat.h>

namespace gameunlocker {

CompanionManager::CompanionManager(const Context& ctx) : ctx_(ctx) {}

std::string CompanionManager::resolveModulePath() const {
    FdWrapper dirfd(ctx_.getModuleDirFd());
    if (!dirfd.isValid()) return "";

    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", dirfd.get());

    char modulePath[PATH_MAX];
    ssize_t len = readlink(fdPath, modulePath, sizeof(modulePath) - 1);
    if (len <= 0) return "";

    modulePath[len] = '\0';
    return std::string(modulePath);
}

bool CompanionManager::mountCpuInfo(const std::string& modulePath) const {
    if (modulePath.empty()) return false;
    std::string spoofPath = modulePath + "/cpuinfo_spoof";
    return executeCompanionCommand("mount_spoof:" + spoofPath);
}

bool CompanionManager::unmountCpuInfo() const {
    return executeCompanionCommand("unmount_spoof");
}

bool CompanionManager::executeCompanionCommand(const std::string& command) const {
    if (!ctx_.getApi()) return false;

    FdWrapper fd_conn(ctx_.getApi()->connectCompanion());
    if (!fd_conn.isValid()) {
        LOGW("CompanionManager: Failed to connect to companion");
        return false;
    }
    ssize_t written = write(fd_conn.get(), command.c_str(), command.size());
    return written == static_cast<ssize_t>(command.size());
}

// ============================================================
// Companion handler — runs as ROOT in the zygote companion
// process. Performs privileged bind-mount of cpuinfo_spoof.
// ============================================================
void companionHandler(int fd) {
    char buffer[512];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) return;
    buffer[bytes] = '\0';

    std::string command(buffer, static_cast<size_t>(bytes));

    if (command == "unmount_spoof") {
        // Best-effort unmount; ignore errors (may not be mounted)
        system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
        LOGI("Companion: Unmounted /proc/cpuinfo");
        return;
    }

    if (command.rfind("mount_spoof:", 0) == 0) {
        // Extract path after "mount_spoof:" prefix
        std::string spoofPath = command.substr(12); // len("mount_spoof:") == 12

        // Security: only allow paths under /data/adb/modules/
        if (spoofPath.rfind("/data/adb/modules/", 0) != 0) {
            LOGW("Companion: Rejected path outside module dir: %s", spoofPath.c_str());
            return;
        }

        // Verify file exists
        if (access(spoofPath.c_str(), F_OK) != 0) {
            LOGW("Companion: cpuinfo_spoof not found at: %s", spoofPath.c_str());
            return;
        }

        // Unmount any existing bind-mount first
        system("/system/bin/umount /proc/cpuinfo 2>/dev/null");

        char mountCmd[600];
        snprintf(mountCmd, sizeof(mountCmd),
                 "/system/bin/mount --bind \"%s\" /proc/cpuinfo", spoofPath.c_str());
        int ret = system(mountCmd);
        if (ret == 0) {
            LOGI("Companion: Mounted %s -> /proc/cpuinfo", spoofPath.c_str());
        } else {
            LOGW("Companion: mount --bind failed (exit=%d) for path: %s", ret, spoofPath.c_str());
        }
        return;
    }

    LOGW("Companion: Unknown command: %s", command.c_str());
}

} // namespace gameunlocker
