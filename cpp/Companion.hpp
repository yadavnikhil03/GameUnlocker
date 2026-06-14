#pragma once

#include <string>
#include "context.hpp"

namespace gameunlocker {

class CompanionManager {
public:
    explicit CompanionManager(const Context& ctx);

    std::string resolveModulePath() const;

    bool mountSpoof(const std::string& spoofFilePath) const;

    bool unmountSpoof() const;

private:
    const Context& ctx_;

    bool executeCompanionCommand(const std::string& command) const;
};

void companionHandler(int fd);

} // namespace gameunlocker
