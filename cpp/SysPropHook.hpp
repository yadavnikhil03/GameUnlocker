#pragma once

#include "IHook.hpp"
#include "ConfigData.hpp"
#include "zygisk.hpp"

namespace gameunlocker {

class SysPropHook : public IHook {
public:
    const char* getName() const override {
        return "SysPropHook";
    }

    bool onEnable(const Context& ctx) override;
    void onDisable(const Context& ctx) override;

    static void setProfile(const std::optional<DeviceProfile>& profile);
    static std::optional<DeviceProfile> getProfile();

private:
    static std::optional<DeviceProfile> activeProfile_;
};

} // namespace gameunlocker
