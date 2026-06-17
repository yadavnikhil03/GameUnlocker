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

bool RoutingEngine::isSystemPackage(std::string_view pkg) const {
    if (pkg.empty()) return true;
    if (pkg[0] == '/') return true;
    if (pkg.find('.') == std::string_view::npos) return true;

    static constexpr std::string_view kReservedPrefixes[] = {
        "android", "com.android", "com.google.android", "com.miui", "com.xiaomi",
        "com.android.systemui", "com.android.settings", "com.android.phone",
        "com.android.permissioncontroller", "com.android.vending", "com.sec.android",
        "com.huawei", "com.oppo", "com.coloros", "com.heytap",
        "com.vivo", "com.iqoo", "com.bbk", "com.tencent.qqlive",
        "miui", "org.lineageos", "com.android.shell", "com.android.providers",
        "com.android.inputmethod", "com.android.contacts", "com.android.mms",
        "com.android.calendar", "com.android.camera", "com.android.deskclock",
        "com.android.email", "com.android.calculator", "com.android.nfc",
        "com.android.bluetooth", "com.android.wallpaper"
    };

    for (auto prefix : kReservedPrefixes) {
        if (pkg == prefix) return true;
        if (pkg.size() > prefix.size() &&
            pkg.compare(0, prefix.size(), prefix) == 0 &&
            pkg[prefix.size()] == '.') return true;
    }

    if (pkg.find("launcher") != std::string_view::npos) return true;
    if (pkg.find("systemui") != std::string_view::npos) return true;

    return false;
}

std::optional<std::string> RoutingEngine::resolveProfile(std::string_view packageName) {
    std::string pkgStr(packageName);

    if (isSystemPackage(packageName)) {
        return std::nullopt;
    }

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
