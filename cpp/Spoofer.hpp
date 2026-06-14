#pragma once

#include "context.hpp"
#include "ConfigData.hpp"
#include <string>

namespace gameunlocker {

class Spoofer {
public:
    explicit Spoofer(const Context& ctx);

    void applyDeviceSpoof(const DeviceProfile& profile);

private:
    const Context& ctx_;

    void setStringField(jclass buildClass, const char* fieldName, const std::string& value);
};

} // namespace gameunlocker
