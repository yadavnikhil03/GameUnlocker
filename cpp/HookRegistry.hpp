#pragma once

#include "IHook.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace gameunlocker {

using HookFactory = std::function<std::unique_ptr<IHook>()>;

class HookRegistry {
public:
    static HookRegistry& getInstance() {
        static HookRegistry instance;
        return instance;
    }

    void registerHook(HookFactory factory) {
        factories_.push_back(std::move(factory));
    }

    const std::vector<HookFactory>& getFactories() const {
        return factories_;
    }

private:
    HookRegistry() = default;
    std::vector<HookFactory> factories_;
};

class HookRegistrar {
public:
    explicit HookRegistrar(HookFactory factory) {
        HookRegistry::getInstance().registerHook(std::move(factory));
    }
};

} 
#define REGISTER_HOOK(HookClass) \
    static ::gameunlocker::HookRegistrar __registrar_##HookClass([]() { \
        return std::make_unique<HookClass>(); \
    });
