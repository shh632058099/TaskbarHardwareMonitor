#pragma once

#include "SensorDemand.h"
#include "SensorTypes.h"
#include "../hardware/Sensor.h"

namespace monitor {

void AddSnapshotSensors(const SensorSnapshot& snapshot, std::uint32_t demand,
                        std::uint64_t timestamp, SensorCollection& sensors);

} // namespace monitor
