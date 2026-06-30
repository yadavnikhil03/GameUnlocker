#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace gameunlocker {

struct DeviceProfile {
    std::string manufacturer;
    std::string brand;
    std::string model;
    std::string device;
    std::string product;
    std::string fingerprint;
    std::optional<std::string> brand_for_device;
    std::string board;      
    std::string hardware;    
};

struct GameUnlockerConfig {
    std::unordered_map<std::string, DeviceProfile> profiles;
    std::unordered_set<std::string> blacklistedApps;
    std::unordered_set<std::string> cpuSpoofApps;
};

} 
