#pragma once
#include <string>
#include "context.hpp"

namespace gameunlocker {

class CompanionManager {
public:
    explicit CompanionManager(const Context& ctx);

    std::string resolveModulePath() const;

    bool mountCpuInfo(const std::string& modulePath, const std::string& hardware) const;

    bool unmountCpuInfo() const;

    bool whitelistDaemon(uid_t targetUid) const;
private:
    const Context& ctx_;

    bool executeCompanionCommand(const std::string& command) const;
};

void companionHandler(int fd);
} 