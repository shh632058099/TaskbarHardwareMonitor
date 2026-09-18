#pragma once
#include <cstdint>

namespace monitor {
enum BatteryState : std::uint32_t {
    BatteryUnknown = 0,
    BatteryDischarging = 1,
    BatteryCharging = 2,
    BatteryOnAc = 3,
    BatteryFull = 4,
};

struct SensorSnapshot {
    double cpuTemperature = 0.0;
    bool cpuTemperatureValid = false;
    double cpuUsage = 0.0;
    bool cpuUsageValid = false;
    std::uint64_t downloadBytesPerSecond = 0;
    std::uint64_t uploadBytesPerSecond = 0;
    bool networkValid = false;
    double gpuTemperature = 0.0;
    bool gpuTemperatureValid = false;
    double diskTemperature = 0.0;
    bool diskTemperatureValid = false;
    double cpuPower = 0.0;
    bool cpuPowerValid = false;
    double memoryUsage = 0.0;
    std::uint64_t memoryUsedBytes = 0;
    std::uint64_t memoryTotalBytes = 0;
    bool memoryValid = false;
    double cpuClockMHz = 0.0;
    bool cpuClockValid = false;
    double gpuUsage = 0.0;
    bool gpuUsageValid = false;
    std::uint64_t gpuMemoryUsedBytes = 0;
    std::uint64_t gpuMemoryTotalBytes = 0;
    bool gpuMemoryValid = false;
    double gpuPower = 0.0;
    bool gpuPowerValid = false;
    double gpuFanPercent = 0.0;
    bool gpuFanValid = false;
    std::uint64_t diskReadBytesPerSecond = 0;
    std::uint64_t diskWriteBytesPerSecond = 0;
    bool diskIoValid = false;
    double batteryPercent = 0.0;
    bool batteryValid = false;
    std::uint32_t batteryState = 0;
    double systemPower = 0.0;
    bool systemPowerValid = false;
};
}
