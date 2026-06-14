#include "Companion.hpp"
#include "logger.hpp"
#include "raii.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <limits.h>

namespace gameunlocker {

CompanionManager::CompanionManager(const Context& ctx) : ctx_(ctx) {}

std::string CompanionManager::resolveModulePath() const {
    int dirfd = ctx_.getModuleDirFd();
    if (dirfd < 0) return "";

    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", dirfd);

    char modulePath[PATH_MAX];
    ssize_t len = readlink(fdPath, modulePath, sizeof(modulePath) - 1);
    if (len <= 0) return "";

    modulePath[len] = '\0';
    return std::string(modulePath);
}

bool CompanionManager::executeCompanionCommand(const std::string& command) const {
    if (!ctx_.getApi()) return false;

    FdWrapper fd_conn(ctx_.getApi()->connectCompanion());
    if (fd_conn.isValid()) {
        ssize_t written = write(fd_conn.get(), command.c_str(), command.size());
        return written == static_cast<ssize_t>(command.size());
    }
    return false;
}

bool CompanionManager::mountSpoof(const std::string& spoofFilePath) const {
    return executeCompanionCommand("mount_spoof:" + spoofFilePath);
}

bool CompanionManager::unmountSpoof() const {
    return executeCompanionCommand("unmount_spoof");
}

void companionHandler(int fd) {
    char buffer[512];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::string command = buffer;
        if (command == "unmount_spoof") {
            system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
            LOGI("CPU Mount Companion: Unmounted CPU info");
        } else if (command.rfind("mount_spoof:", 0) == 0) {
            std::string spoof_file_path = command.substr(strlen("mount_spoof:"));
            if (spoof_file_path.rfind("/data/adb/modules/", 0) != 0) {
                LOGW("Rejected invalid spoof path");
                return;
            }

            if (access(spoof_file_path.c_str(), F_OK) == 0) {
                system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
                char mount_cmd[512];
                snprintf(mount_cmd, sizeof(mount_cmd), "/system/bin/mount --bind %s /proc/cpuinfo", spoof_file_path.c_str());
                system(mount_cmd);
                LOGI("CPU Mount Companion: Mounted CPU info");
            } else {
                LOGW("cpuinfo_spoof missing: %s", spoof_file_path.c_str());
            }
        }
    }
}

} // namespace gameunlocker
