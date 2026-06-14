#pragma once

#include <string>
#include <optional>
#include "context.hpp"
#include "ConfigData.hpp"
#include "RoutingEngine.hpp"

namespace gameunlocker {

class ConfigManager {
public:
    explicit ConfigManager(const Context& ctx);

    bool load();

    bool isAppBlacklisted(const std::string& appName) const;

    bool isCpuSpoofApp(const std::string& appName) const;

    std::optional<DeviceProfile> getProfileForApp(const std::string& appName);

private:
    const Context& ctx_;
    GameUnlockerConfig config_;
    RoutingEngine routingEngine_;

    bool parseJson(const std::string& jsonString);
};

} // namespace gameunlocker
