#include "Config.hpp"
#include "raii.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <cstring>
#include <vector>

using json = nlohmann::json;

namespace gameunlocker {

GameUnlockerConfig ConfigManager::config_;
RoutingEngine ConfigManager::routingEngine_;
bool ConfigManager::isLoaded_ = false;

bool ConfigManager::globalInit(const Context& ctx) {
    if (isLoaded_) return true;

    int dirfd = ctx.getModuleDirFd();
    if (dirfd < 0) {
        LOGE("ConfigManager::globalInit failed: module dir fd is invalid");
        return false;
    }

    FdWrapper fd(openat(dirfd, "config.json", O_RDONLY));
    if (!fd.isValid()) {
        LOGE("ConfigManager::globalInit failed: could not open config.json");
        return false;
    }

    std::string raw;
    raw.reserve(16384);
    char chunk[4096];
    ssize_t bytes;
    const size_t kMaxConfigSize = 1048576;

    while ((bytes = read(fd.get(), chunk, sizeof(chunk))) > 0) {
        raw.append(chunk, static_cast<size_t>(bytes));
        if (raw.size() > kMaxConfigSize) {
            LOGE("ConfigManager::globalInit failed: config.json exceeds size limit");
            return false;
        }
    }

    if (raw.empty()) {
        LOGE("ConfigManager::globalInit failed: config.json is empty or read error");
        return false;
    }

    isLoaded_ = parseJson(raw);
    return isLoaded_;
}

bool ConfigManager::parseJson(const std::string& jsonString) {
    try {
        json j = json::parse(jsonString);

        if (j.contains("profiles") && j["profiles"].is_object()) {
            for (auto& [key, val] : j["profiles"].items()) {
                if (!val.is_object()) continue;

                DeviceProfile profile;
                if (!val.contains("MANUFACTURER") || !val["MANUFACTURER"].is_string() ||
                    !val.contains("BRAND") || !val["BRAND"].is_string() ||
                    !val.contains("MODEL") || !val["MODEL"].is_string() ||
                    !val.contains("DEVICE") || !val["DEVICE"].is_string() ||
                    !val.contains("PRODUCT") || !val["PRODUCT"].is_string() ||
                    !val.contains("FINGERPRINT") || !val["FINGERPRINT"].is_string()) {
                    continue;
                }

                profile.manufacturer = val["MANUFACTURER"].get<std::string>();
                profile.brand = val["BRAND"].get<std::string>();
                profile.model = val["MODEL"].get<std::string>();
                profile.device = val["DEVICE"].get<std::string>();
                profile.product = val["PRODUCT"].get<std::string>();
                profile.fingerprint = val["FINGERPRINT"].get<std::string>();

                if (val.contains("BRAND_FOR_DEVICE") && val["BRAND_FOR_DEVICE"].is_string()) {
                    profile.brand_for_device = val["BRAND_FOR_DEVICE"].get<std::string>();
                }
                if (val.contains("BOARD") && val["BOARD"].is_string()) {
                    profile.board = val["BOARD"].get<std::string>();
                }
                if (val.contains("HARDWARE") && val["HARDWARE"].is_string()) {
                    profile.hardware = val["HARDWARE"].get<std::string>();
                }

                config_.profiles[key] = profile;
            }
        }

        if (j.contains("routing_rules") && j["routing_rules"].is_array()) {
            for (const auto& ruleJson : j["routing_rules"]) {
                if (!ruleJson.is_object() || !ruleJson.contains("type") || !ruleJson["type"].is_string() ||
                    !ruleJson.contains("pattern") || !ruleJson["pattern"].is_string() ||
                    !ruleJson.contains("profile") || !ruleJson["profile"].is_string() ||
                    !ruleJson.contains("priority") || !ruleJson["priority"].is_number()) {
                    continue;
                }

                RoutingRule rule;
                std::string typeStr = ruleJson["type"].get<std::string>();

                if (typeStr == "exact") rule.type = MatchType::EXACT;
                else if (typeStr == "prefix") rule.type = MatchType::PREFIX;
                else if (typeStr == "suffix") rule.type = MatchType::SUFFIX;
                else if (typeStr == "wildcard") rule.type = MatchType::WILDCARD;
                else continue;

                rule.pattern = ruleJson["pattern"].get<std::string>();
                rule.profile = ruleJson["profile"].get<std::string>();
                rule.priority = ruleJson["priority"].get<int>();

                routingEngine_.addRule(rule);
            }
            routingEngine_.sortRules();
        }

        if (j.contains("cpu_spoof") && j["cpu_spoof"].is_object()) {
            if (j["cpu_spoof"].contains("with_cpu") && j["cpu_spoof"]["with_cpu"].is_array()) {
                for (const auto& pkg : j["cpu_spoof"]["with_cpu"]) {
                    if (pkg.is_string()) {
                        config_.cpuSpoofApps.insert(pkg.get<std::string>());
                    }
                }
            }
            if (j["cpu_spoof"].contains("blacklist") && j["cpu_spoof"]["blacklist"].is_array()) {
                for (const auto& pkg : j["cpu_spoof"]["blacklist"]) {
                    if (pkg.is_string()) {
                        config_.blacklistedApps.insert(pkg.get<std::string>());
                    }
                }
            }
        }

        return !config_.profiles.empty() || !config_.cpuSpoofApps.empty() || !config_.blacklistedApps.empty() || j.contains("routing_rules");
    } catch (const json::parse_error& e) {
        LOGE("Failed to parse config.json: %s", e.what());
        return false;
    } catch (const json::type_error& e) {
        LOGE("Type error in config.json: %s", e.what());
        return false;
    } catch (const std::exception& e) {
        LOGE("Error processing config.json: %s", e.what());
        return false;
    }
}

bool ConfigManager::isAppBlacklisted(const std::string& appName) {
    if (!isLoaded_) return false;
    return config_.blacklistedApps.find(appName) != config_.blacklistedApps.end();
}

bool ConfigManager::isCpuSpoofApp(const std::string& appName) {
    if (!isLoaded_) return false;
    return config_.cpuSpoofApps.find(appName) != config_.cpuSpoofApps.end();
}

std::optional<DeviceProfile> ConfigManager::getProfileForApp(const std::string& appName) {
    if (!isLoaded_) return std::nullopt;
    auto profileNameOpt = routingEngine_.resolveProfile(appName);
    if (profileNameOpt.has_value()) {
        auto profileIt = config_.profiles.find(profileNameOpt.value());
        if (profileIt != config_.profiles.end()) {
            return profileIt->second;
        }
    }
    return std::nullopt;
}

} 
