#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace gameunlocker {

struct DeviceProfile {
    // Core identity
    std::string manufacturer;
    std::string brand;
    std::string model;
    std::string device;
    std::string product;
    std::string fingerprint;
    std::optional<std::string> brand_for_device;

    // Hardware identifiers
    std::string board;           // ro.product.board / ro.board.platform
    std::string hardware;        // ro.hardware

    // GPU info
    std::string gl_vendor;       // GL_VENDOR string (e.g. "Qualcomm", "ARM")
    std::string gl_renderer;     // GL_RENDERER string (e.g. "Adreno (TM) 750")

    // SoC info
    std::string soc_model;       // ro.soc.model (e.g. "SM8650")
    std::string soc_manufacturer;// ro.soc.manufacturer (e.g. "Qualcomm")
    std::string soc_id;          // ro.vendor.qti.soc_id numeric string (e.g. "519")

    // Android version info (for ro.build.version.* spoofing)
    std::string android_version; // e.g. "14"
    std::string security_patch;  // e.g. "2024-03-05"
};

struct GameUnlockerConfig {
    std::unordered_map<std::string, DeviceProfile> profiles;
    std::unordered_set<std::string> blacklistedApps;
    std::unordered_set<std::string> cpuSpoofApps;
};

} // namespace gameunlocker
