#pragma once

#include "context.hpp"
#include "IHook.hpp"
#include "ConfigData.hpp"
#include <vector>
#include <memory>

namespace gameunlocker {

class HookManager {
public:
    explicit HookManager(const Context& ctx);

    void initialize(const std::optional<DeviceProfile>& profile);

    void enableHooks();

    void disableHooks();

    bool hasActiveHooks() const;

private:
    const Context& ctx_;
    std::vector<std::unique_ptr<IHook>> activeHooks_;
};

} // namespace gameunlocker
