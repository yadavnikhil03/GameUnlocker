#pragma once

#include <string>
#include <string_view>

namespace gameunlocker {

enum class MatchType {
    EXACT,
    PREFIX,
    SUFFIX,
    WILDCARD
};

struct RoutingRule {
    MatchType type;
    std::string pattern;
    std::string profile;
    int priority;

    bool operator<(const RoutingRule& other) const {
        return priority > other.priority; 
    }
};

}
