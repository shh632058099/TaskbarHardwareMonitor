#pragma once

#include "SensorTypes.h"

namespace monitor {

class IGpuProvider {
public:
    virtual ~IGpuProvider() = default;
    virtual void Update(SensorSnapshot& snapshot, std::uint32_t demand) = 0;
};

} // namespace monitor
