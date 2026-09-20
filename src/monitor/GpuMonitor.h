#pragma once
#include "SensorTypes.h"
#include "SensorDemand.h"
#include "GpuProvider.h"
#include <cstdint>

namespace monitor {
class GpuMonitor : public IGpuProvider {
public:
    ~GpuMonitor();
    void Update(SensorSnapshot& snapshot, std::uint32_t demand) override;
private:
    void Initialize();
    void* module_ = nullptr;
    void* device_ = nullptr;
    std::uint64_t nextRetryTick_ = 0;
    std::uint64_t temperatureSuccessTick_ = 0;
    double temperature_ = 0.0;
    bool temperatureValid_ = false;
};
}
