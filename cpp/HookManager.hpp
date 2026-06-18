#pragma once

#include "context.hpp"
#include "IHook.hpp"
#include <vector>
#include <memory>

namespace gameunlocker {

class HookManager {
public:
    explicit HookManager(const Context& ctx);

    void initialize();

    void enableHooks();

    bool hasActiveHooks() const;

private:
    const Context& ctx_;
    std::vector<std::unique_ptr<IHook>> activeHooks_;
};

} // namespace gameunlocker
