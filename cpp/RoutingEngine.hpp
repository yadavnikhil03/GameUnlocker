#pragma once

#include "RoutingModels.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <mutex>

namespace gameunlocker {

class RoutingEngine {
public:
    RoutingEngine() = default;

    void addRule(const RoutingRule& rule);

    void sortRules();

    std::optional<std::string> resolveProfile(std::string_view packageName);

private:
    std::vector<RoutingRule> rules_;
    std::unordered_map<std::string, std::string> cache_;
    std::mutex cacheMutex_;

    bool matchRule(const RoutingRule& rule, std::string_view pkg) const;
    bool isSystemPackage(std::string_view pkg) const;
    void putCache(const std::string& pkg, const std::string& profile);
};

} // namespace gameunlocker
