#pragma once

#include "../HardwareAccess.h"
#include "../../monitor/TemperatureProvider.h"

#include <cstdint>

namespace monitor {

struct AmdCpuIdentity {
    unsigned int family = 0;
    unsigned int model = 0;
    unsigned int stepping = 0;
};

AmdCpuIdentity DetectAmdCpuIdentity();
bool IsSupportedAmdZenIdentity(const AmdCpuIdentity& identity);
bool DecodeAmdZenTemperature(unsigned int raw, double offsetCelsius, double& celsius);

struct AmdTemperatureRegisterMap {
    std::uint8_t bus = 0;
    std::uint8_t device = 0;
    std::uint8_t function = 0;
    std::uint16_t offset = 0;
    std::uint8_t width = 0;
    double offsetCelsius = 0.0;
};

class AmdTemperatureProvider final : public ITemperatureProvider {
public:
    AmdTemperatureProvider(IHardwareAccess& access, AmdCpuIdentity identity,
                           AmdTemperatureRegisterMap map)
        : access_(access), identity_(identity), map_(map) {}
    bool ReadPackageTemperature(double& celsius) override;
private:
    IHardwareAccess& access_;
    AmdCpuIdentity identity_{};
    AmdTemperatureRegisterMap map_{};
};

} // namespace monitor
