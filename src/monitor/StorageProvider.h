#pragma once

#include "SensorTypes.h"

namespace monitor {

class IStorageProvider {
public:
    virtual ~IStorageProvider() = default;
    virtual void Update(SensorSnapshot& snapshot, bool temperature, bool io) = 0;
    virtual void ForceRefresh() = 0;
    virtual void ResetIoSampling() = 0;
};

} // namespace monitor
