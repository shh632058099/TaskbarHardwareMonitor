#pragma once

#include "SensorTypes.h"

namespace monitor {

class SystemMonitor {
public:
    void UpdateMemory(SensorSnapshot& snapshot) const;
    void UpdateCpuClock(SensorSnapshot& snapshot) const;
    void UpdateBattery(SensorSnapshot& snapshot, bool collectSystemPower) const;
};

} // namespace monitor
