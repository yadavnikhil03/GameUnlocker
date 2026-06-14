#include "RoutingEngine.hpp"
#include <algorithm>

namespace gameunlocker {

void RoutingEngine::addRule(const RoutingRule& rule) {
    rules_.push_back(rule);
}

void RoutingEngine::sortRules() {
    std::sort(rules_.begin(), rules_.end());
}

bool RoutingEngine::matchRule(const RoutingRule& rule, std::string_view pkg) const {
    switch (rule.type) {
        case MatchType::EXACT:
            return pkg == rule.pattern;
        case MatchType::PREFIX:
            return pkg.length() >= rule.pattern.length() && 
                   pkg.substr(0, rule.pattern.length()) == rule.pattern;
        case MatchType::SUFFIX:
            return pkg.length() >= rule.pattern.length() &&
                   pkg.substr(pkg.length() - rule.pattern.length()) == rule.pattern;
        case MatchType::WILDCARD:
            if (rule.pattern == "*") return true;
            return pkg.find(rule.pattern) != std::string_view::npos;
        default:
            return false;
    }
}

std::optional<std::string> RoutingEngine::resolveProfile(std::string_view packageName) {
    std::string pkgStr(packageName);

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(pkgStr);
        if (it != cache_.end()) {
            if (it->second.empty()) return std::nullopt;
            return it->second;
        }
    }

    for (const auto& rule : rules_) {
        if (matchRule(rule, packageName)) {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[pkgStr] = rule.profile;
            return rule.profile;
        }
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_[pkgStr] = ""; 
    }
    
    return std::nullopt;
}

} // namespace gameunlocker
