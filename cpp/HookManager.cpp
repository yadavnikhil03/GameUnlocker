#include "HookManager.hpp"
#include "HookRegistry.hpp"
#include "logger.hpp"

namespace gameunlocker {

HookManager::HookManager(const Context& ctx) : ctx_(ctx) {}

void HookManager::initialize(const std::optional<DeviceProfile>& profile) {

    for (const auto& factory : HookRegistry::getInstance().getFactories()) {
        auto hook = factory();
        if (hook->isSupported(ctx_)) {
            LOGI("HookManager: Initialized hook '%s'", hook->getName());
            activeHooks_.push_back(std::move(hook));
        } else {
            LOGW("HookManager: Hook '%s' is not supported on this device/profile", hook->getName());
        }
    }
}

void HookManager::enableHooks() {
    for (auto& hook : activeHooks_) {
        if (hook->onEnable(ctx_)) {
            LOGI("HookManager: Successfully enabled hook '%s'", hook->getName());
        } else {
            LOGE("HookManager: Failed to enable hook '%s'", hook->getName());
        }
    }
}

void HookManager::disableHooks() {
    for (auto& hook : activeHooks_) {
        hook->onDisable(ctx_);
        LOGI("HookManager: Disabled hook '%s'", hook->getName());
    }
    activeHooks_.clear();
}

bool HookManager::hasActiveHooks() const {
    return !activeHooks_.empty();
}

} 
