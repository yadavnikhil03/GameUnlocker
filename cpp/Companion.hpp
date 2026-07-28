#pragma once

#include <string>
#include "context.hpp"

namespace gameunlocker {

class CompanionManager {
public:
    explicit CompanionManager(const Context& ctx);

    // Resolves the on-disk module path via /proc/self/fd/<dirfd>
    std::string resolveModulePath() const;

    bool mountCpuInfo(const std::string& modulePath, const std::string& hardware) const;

    bool unmountCpuInfo() const;

    bool whitelistDaemon(uid_t targetUid) const;

private:
    const Context& ctx_;

    bool executeCompanionCommand(const std::string& command) const;
};

// Companion handler registered via REGISTER_ZYGISK_COMPANION.
// Runs as root in the zygote companion process and performs privileged
// bind-mount operations that the app process cannot do itself.
void companionHandler(int fd);

} // namespace gameunlocker
