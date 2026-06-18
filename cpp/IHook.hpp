#pragma once

#include "context.hpp"
#include <string>

namespace gameunlocker {

class IHook {
public:
    virtual ~IHook() = default;

    virtual const char* getName() const = 0;

    virtual bool isSupported(const Context& ctx) const { return true; }

    virtual bool onEnable(const Context& ctx) = 0;
};

} 
