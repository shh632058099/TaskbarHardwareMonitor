#pragma once
#include "SensorTypes.h"
#include "SensorDemand.h"
#include "GpuProvider.h"
#include <cstdint>

namespace monitor {
struct NvmlUtilization { unsigned int gpu; unsigned int memory; };
struct NvmlMemory { unsigned long long total; unsigned long long free; unsigned long long used; };
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
    using TemperatureFunction = int (*)(void*, int, unsigned int*);
    using UtilizationFunction = int (*)(void*, NvmlUtilization*);
    using MemoryFunction = int (*)(void*, NvmlMemory*);
    using PowerFunction = int (*)(void*, unsigned int*);
    using FanFunction = int (*)(void*, unsigned int*);
    TemperatureFunction temperatureFunction_ = nullptr;
    UtilizationFunction utilizationFunction_ = nullptr;
    MemoryFunction memoryFunction_ = nullptr;
    PowerFunction powerFunction_ = nullptr;
    FanFunction fanFunction_ = nullptr;
};
}
