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

void companionHandler(int fd) {
}

} 
