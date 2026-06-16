#pragma once

#include "IHook.hpp"
#include "HookRegistry.hpp"
#include "SysPropHook.hpp"

namespace gameunlocker {

class GpuHook : public IHook {
public:
    GpuHook() = default;
    ~GpuHook() override = default;

    const char* getName() const override {
        return "Adreno 750 GPU Spoof";
    }

    bool isSupported(const Context& ctx) const override {
        auto profile = SysPropHook::getProfile();
        if (profile.has_value()) {
            return profile.value().manufacturer != "Google"; 
        }
        return SysPropHook::isCpuSpoofOnly();
    }

    bool onEnable(const Context& ctx) override;

    void onDisable(const Context& ctx) override;
};

} 
