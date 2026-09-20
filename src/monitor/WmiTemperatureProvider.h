#pragma once

#include "TemperatureProvider.h"

#include <cstdint>

namespace monitor {

constexpr std::uint64_t WmiRetryBackoffMilliseconds = 30000;

inline std::uint64_t WmiRetryDelayMilliseconds(std::uint64_t now,
                                                std::uint64_t retryAfter) {
    return now >= retryAfter ? 0 : retryAfter - now;
}

class WmiTemperatureProvider final : public ITemperatureProvider {
public:
    bool ReadPackageTemperature(double& celsius) override;
    bool ForceReadPackageTemperature(double& celsius);
private:
    bool ReadPackageTemperature(double& celsius, bool force);
    std::uint64_t retryAfter_ = 0;
};

} // namespace monitor
