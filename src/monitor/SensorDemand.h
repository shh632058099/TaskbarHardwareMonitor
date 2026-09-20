#pragma once

#include "../config/Config.h"

#include <cstdint>
#include <string>

namespace monitor {

enum SensorDemandFlag : std::uint32_t {
    DemandCpuTemperature = 1u << 0,
    DemandCpuUsage       = 1u << 1,
    DemandGpuTemperature = 1u << 2,
    DemandDiskTemperature= 1u << 3,
    DemandNetwork        = 1u << 4,
    DemandCpuPower       = 1u << 5,
    DemandMemory         = 1u << 6,
    DemandGpuUsage       = 1u << 7,
    DemandVram           = 1u << 8,
    DemandDiskIo         = 1u << 9,
    DemandCpuClock       = 1u << 10,
    DemandGpuPower       = 1u << 11,
    DemandFan            = 1u << 12,
    DemandBattery        = 1u << 13,
    DemandSystemPower    = 1u << 14,
    DemandCpuInternalTemperature = 1u << 15,
    DemandGpuInternalTemperature = 1u << 16,
    DemandCpuFanRpm         = 1u << 17,
    DemandGpuFanRpm         = 1u << 18,
};

constexpr std::uint32_t AllSensorDemand = DemandCpuTemperature | DemandCpuUsage |
    DemandGpuTemperature | DemandDiskTemperature | DemandNetwork | DemandCpuPower |
    DemandMemory | DemandGpuUsage | DemandVram | DemandDiskIo | DemandCpuClock |
    DemandGpuPower | DemandFan | DemandBattery | DemandSystemPower |
    DemandCpuInternalTemperature | DemandGpuInternalTemperature |
    DemandCpuFanRpm | DemandGpuFanRpm;

constexpr std::uint32_t DemandInternalThermoFans = DemandCpuInternalTemperature |
    DemandGpuInternalTemperature | DemandCpuFanRpm | DemandGpuFanRpm;

inline bool HasSensorDemand(std::uint32_t demand, SensorDemandFlag flag) {
    return (demand & static_cast<std::uint32_t>(flag)) != 0;
}

std::uint32_t SensorDemandFromMetrics(const Config& config);
std::uint32_t SensorDemandFromFormat(const std::wstring& format);
std::uint32_t SensorDemandFromConfig(const Config& config);
void ApplySensorDemandToMetrics(Config& config, std::uint32_t demand);
std::uint32_t SensorDemandForMetricIndex(int index);

} // namespace monitor
