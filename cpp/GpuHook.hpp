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
        return SysPropHook::getProfile().has_value() || SysPropHook::isCpuSpoofOnly();
    }

    bool onEnable(const Context& ctx) override;


};

} 
