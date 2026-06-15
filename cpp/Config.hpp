#pragma once

#include <string>
#include <optional>
#include "context.hpp"
#include "ConfigData.hpp"
#include "RoutingEngine.hpp"

namespace gameunlocker {

class ConfigManager {
public:
    static bool globalInit(const Context& ctx);

    static bool isAppBlacklisted(const std::string& appName);
    static bool isCpuSpoofApp(const std::string& appName);
    static std::optional<DeviceProfile> getProfileForApp(const std::string& appName);

private:
    static GameUnlockerConfig config_;
    static RoutingEngine routingEngine_;
    static bool isLoaded_;

    static bool parseJson(const std::string& jsonString);
};

} // namespace gameunlocker
