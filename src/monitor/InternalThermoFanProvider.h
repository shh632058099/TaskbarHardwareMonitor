#pragma once

#include "SensorTypes.h"

#include "../hardware/Sensor.h"

#include <cstdint>
#include <vector>

namespace monitor {

class IInternalThermoFanProvider {
public:
    virtual ~IInternalThermoFanProvider() = default;
    virtual bool Read(SensorSnapshot& snapshot, std::uint64_t timestamp,
                      SensorCollection& sensors) = 0;
};

class InternalThermoFanProviderManager final : public IInternalThermoFanProvider {
public:
    explicit InternalThermoFanProviderManager(
        std::vector<IInternalThermoFanProvider*> providers);
    bool Read(SensorSnapshot& snapshot, std::uint64_t timestamp,
              SensorCollection& sensors) override;
private:
    std::vector<IInternalThermoFanProvider*> providers_;
};

} // namespace monitor
