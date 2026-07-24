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
        // GPU spoofing requires a full device profile with GL_VENDOR/GL_RENDERER.
        // For cpu-spoof-only mode there is no GPU data — do not inject.
        return SysPropHook::getProfile().has_value();
    }

    bool onEnable(const Context& ctx) override;


};

} 
