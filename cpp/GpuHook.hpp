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
        // GPU spoof applies whenever a device profile is active (the profile is
        // the spoofed target identity, not the real device hardware).
        // We also enable it for cpu_spoof-only apps so they get the Qualcomm
        // renderer string that unlocks GPU-gated frame-rate tiers.
        return SysPropHook::getProfile().has_value() || SysPropHook::isCpuSpoofOnly();
    }

    bool onEnable(const Context& ctx) override;


};

} 
