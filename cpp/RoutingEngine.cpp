#include "RoutingEngine.hpp"
#include <algorithm>

namespace gameunlocker {

static constexpr size_t kMaxCacheEntries = 512;

void RoutingEngine::putCache(const std::string& pkg, const std::string& profile) {
    if (cache_.size() >= kMaxCacheEntries) {
        cache_.clear();
    }
    cache_[pkg] = profile;
}

void RoutingEngine::addRule(const RoutingRule& rule) {
    rules_.push_back(rule);
}

void RoutingEngine::sortRules() {
    std::sort(rules_.begin(), rules_.end());
}

// Glob-style wildcard match: '*' matches any run of characters, '?' matches a
// single character. Everything else matches literally. This replaces the old
// substring test, which silently broke patterns like "com.foo.*".
static bool wildcardMatch(std::string_view pattern, std::string_view text) {
    size_t p = 0, t = 0, star = std::string_view::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == text[t] || pattern[p] == '?')) {
            ++p; ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
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
            return wildcardMatch(rule.pattern, pkg);
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

    // Only treat launcher/systemui as system when they appear as an actual
    // dot-separated segment, so a game like "com.foo.gamelauncher" is not
    // misclassified as a system package.
    auto hasSegment = [](std::string_view pkg, std::string_view seg) {
        if (pkg == seg) return true;
        std::string_view dotSeg = seg;
        std::string_view needle = dotSeg;
        size_t pos = 0;
        while ((pos = pkg.find(needle, pos)) != std::string_view::npos) {
            bool leftOk = (pos == 0) || (pkg[pos - 1] == '.');
            bool rightOk = (pos + needle.size() == pkg.size()) ||
                           (pkg[pos + needle.size()] == '.');
            if (leftOk && rightOk) return true;
            ++pos;
        }
        return false;
    };

    if (hasSegment(pkg, "launcher")) return true;
    if (hasSegment(pkg, "systemui")) return true;

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
            putCache(pkgStr, rule.profile);
            return rule.profile;
        }
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        putCache(pkgStr, "");
    }

    return std::nullopt;
}

} // namespace gameunlocker
