#pragma once

#include "IHook.hpp"
#include "HookRegistry.hpp"

namespace gameunlocker {

class GpuHook : public IHook {
public:
    GpuHook() = default;
    ~GpuHook() override = default;

    const char* getName() const override {
        return "Adreno 750 GPU Spoof";
    }

    bool isSupported(const Context& ctx) const override {
        return true; 
    }

    bool onEnable(const Context& ctx) override;

    void onDisable(const Context& ctx) override;
};

} // namespace gameunlocker
