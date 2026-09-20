#pragma once

#include "SensorTypes.h"

namespace monitor {

class INetworkProvider {
public:
    virtual ~INetworkProvider() = default;
    virtual void Update(SensorSnapshot& snapshot) = 0;
    virtual void ResetSampling() = 0;
};

} // namespace monitor
