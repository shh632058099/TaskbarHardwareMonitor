#include "AmdTemperature.h"

#include "../bus/PciAccess.h"

namespace monitor {

bool IsSupportedAmdZenIdentity(const AmdCpuIdentity& identity) {
    return identity.family == 0x17 && identity.model != 0;
}

bool DecodeAmdZenTemperature(unsigned int raw, double offsetCelsius, double& celsius) {
    const unsigned int sensor = (raw >> 21) & 0x7FFu;
    if (sensor == 0 || sensor > 1200u) return false;
    celsius = static_cast<double>(sensor) / 8.0 - offsetCelsius;
    return celsius > -20.0 && celsius < 150.0;
}

bool AmdTemperatureProvider::ReadPackageTemperature(double& celsius) {
    if (!IsSupportedAmdZenIdentity(identity_) || map_.width == 0) return false;
    std::uint32_t raw = 0;
    return ReadPciConfigValue(access_, map_.bus, map_.device, map_.function,
                              map_.offset, map_.width, raw) &&
           DecodeAmdZenTemperature(raw, map_.offsetCelsius, celsius);
}

} // namespace monitor
