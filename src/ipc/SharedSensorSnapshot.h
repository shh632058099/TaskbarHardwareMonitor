#pragma once

#include "../monitor/SensorTypes.h"
#include "../config/Config.h"

#include <cstdint>

namespace monitor {

constexpr wchar_t SharedSensorMappingName[] = L"Local\\TaskbarHardwareMonitor.SensorSnapshot.v2";
constexpr wchar_t SharedBandCommandMappingName[] = L"Local\\TaskbarHardwareMonitor.BandCommand.v1";

enum SnapshotDisplayFlags : std::uint32_t {
    TaskbarEnabled = 1u << 0,
    ShowCpuTemperature = 1u << 1,
    ShowGpuTemperature = 1u << 2,
    ShowDiskTemperature = 1u << 3,
    ShowNetwork = 1u << 4,
    ShowPower = 1u << 5,
    ShowCpuUsage = 1u << 6,
    ShowMemory = 1u << 7,
    ShowGpuUsage = 1u << 8,
    ShowVram = 1u << 9,
    ShowDiskIo = 1u << 10,
    ShowCpuClock = 1u << 11,
    ShowGpuPower = 1u << 12,
    ShowFan = 1u << 13,
    ShowBattery = 1u << 14,
    ShowSystemPower = 1u << 15,
};

inline std::uint32_t DisplayFlagsFromConfig(const Config& config) {
    std::uint32_t flags = config.taskbarEnabled ? TaskbarEnabled : 0;
    if (config.showCpuTemperature) flags |= ShowCpuTemperature;
    if (config.showCpuUsage) flags |= ShowCpuUsage;
    if (config.showGpuTemperature) flags |= ShowGpuTemperature;
    if (config.showDiskTemperature) flags |= ShowDiskTemperature;
    if (config.showNetwork) flags |= ShowNetwork;
    if (config.showPower) flags |= ShowPower;
    if (config.showMemory) flags |= ShowMemory;
    if (config.showGpuUsage) flags |= ShowGpuUsage;
    if (config.showVram) flags |= ShowVram;
    if (config.showDiskIo) flags |= ShowDiskIo;
    if (config.showCpuClock) flags |= ShowCpuClock;
    if (config.showGpuPower) flags |= ShowGpuPower;
    if (config.showFan) flags |= ShowFan;
    if (config.showBattery) flags |= ShowBattery;
    if (config.showSystemPower) flags |= ShowSystemPower;
    return flags;
}

struct SharedBandCommand {
    std::uint32_t version = 1;
    std::uint32_t sequence = 0;
    std::uint32_t action = 0;
    std::uint32_t displayMode = 0;
    std::uint32_t displayFlags = 0;
};

enum BandCommandAction : std::uint32_t {
    BandCommandSetDisplay = 1,
    BandCommandOpenSettings = 2,
    BandCommandExit = 3,
    BandCommandSetMetrics = 4,
};

struct SharedSensorSnapshot {
    std::uint32_t version = 2;
    std::uint32_t sequence = 0;
    double cpuTemperature = 0.0;
    double cpuUsage = 0.0;
    double gpuTemperature = 0.0;
    double diskTemperature = 0.0;
    double cpuPower = 0.0;
    double memoryUsage = 0.0;
    double cpuClockMHz = 0.0;
    double gpuUsage = 0.0;
    double gpuPower = 0.0;
    double gpuFanPercent = 0.0;
    double batteryPercent = 0.0;
    double systemPower = 0.0;
    std::uint64_t memoryUsedBytes = 0;
    std::uint64_t memoryTotalBytes = 0;
    std::uint64_t gpuMemoryUsedBytes = 0;
    std::uint64_t gpuMemoryTotalBytes = 0;
    std::uint64_t diskReadBytesPerSecond = 0;
    std::uint64_t diskWriteBytesPerSecond = 0;
    std::uint32_t batteryState = 0;
    std::uint64_t downloadBytesPerSecond = 0;
    std::uint64_t uploadBytesPerSecond = 0;
    std::uint64_t timestamp = 0;
    std::uint32_t validMask = 0;
    std::uint32_t displayMode = 0;
    std::uint32_t displayFlags = ShowCpuTemperature | ShowGpuTemperature |
        ShowDiskTemperature | ShowNetwork | ShowPower;
};

enum SnapshotValid : std::uint32_t {
    CpuTemperatureValid = 1u << 0,
    CpuUsageValid = 1u << 1,
    CpuPowerValid = 1u << 2,
    GpuTemperatureValid = 1u << 3,
    DiskTemperatureValid = 1u << 6,
    NetworkValid = 1u << 7,
    MemoryValid = 1u << 8,
    CpuClockValid = 1u << 9,
    GpuUsageValid = 1u << 10,
    GpuMemoryValid = 1u << 11,
    GpuPowerValid = 1u << 12,
    GpuFanValid = 1u << 13,
    DiskIoValid = 1u << 14,
    BatteryValid = 1u << 15,
    SystemPowerValid = 1u << 16,
};

inline SharedSensorSnapshot ToSharedSnapshot(const SensorSnapshot& source, const Config& config) {
    SharedSensorSnapshot target{};
    target.cpuTemperature = source.cpuTemperature;
    target.cpuUsage = source.cpuUsage;
    target.gpuTemperature = source.gpuTemperature;
    target.diskTemperature = source.diskTemperature;
    target.cpuPower = source.cpuPower;
    target.memoryUsage = source.memoryUsage;
    target.cpuClockMHz = source.cpuClockMHz;
    target.gpuUsage = source.gpuUsage;
    target.gpuPower = source.gpuPower;
    target.gpuFanPercent = source.gpuFanPercent;
    target.batteryPercent = source.batteryPercent;
    target.systemPower = source.systemPower;
    target.memoryUsedBytes = source.memoryUsedBytes;
    target.memoryTotalBytes = source.memoryTotalBytes;
    target.gpuMemoryUsedBytes = source.gpuMemoryUsedBytes;
    target.gpuMemoryTotalBytes = source.gpuMemoryTotalBytes;
    target.diskReadBytesPerSecond = source.diskReadBytesPerSecond;
    target.diskWriteBytesPerSecond = source.diskWriteBytesPerSecond;
    target.batteryState = source.batteryState;
    target.downloadBytesPerSecond = source.downloadBytesPerSecond;
    target.uploadBytesPerSecond = source.uploadBytesPerSecond;
    target.displayMode = config.displayMode == DisplayMode::Compact ? 1u : 0u;
    target.displayFlags = DisplayFlagsFromConfig(config);
    if (source.cpuTemperatureValid) target.validMask |= CpuTemperatureValid;
    if (source.cpuUsageValid) target.validMask |= CpuUsageValid;
    if (source.gpuTemperatureValid) target.validMask |= GpuTemperatureValid;
    if (source.diskTemperatureValid) target.validMask |= DiskTemperatureValid;
    if (source.cpuPowerValid) target.validMask |= CpuPowerValid;
    if (source.networkValid) target.validMask |= NetworkValid;
    if (source.memoryValid) target.validMask |= MemoryValid;
    if (source.cpuClockValid) target.validMask |= CpuClockValid;
    if (source.gpuUsageValid) target.validMask |= GpuUsageValid;
    if (source.gpuMemoryValid) target.validMask |= GpuMemoryValid;
    if (source.gpuPowerValid) target.validMask |= GpuPowerValid;
    if (source.gpuFanValid) target.validMask |= GpuFanValid;
    if (source.diskIoValid) target.validMask |= DiskIoValid;
    if (source.batteryValid) target.validMask |= BatteryValid;
    if (source.systemPowerValid) target.validMask |= SystemPowerValid;
    return target;
}

inline SensorSnapshot FromSharedSnapshot(const SharedSensorSnapshot& source) {
    SensorSnapshot target{};
    target.cpuTemperature = source.cpuTemperature;
    target.cpuUsage = source.cpuUsage;
    target.gpuTemperature = source.gpuTemperature;
    target.diskTemperature = source.diskTemperature;
    target.cpuPower = source.cpuPower;
    target.memoryUsage = source.memoryUsage;
    target.cpuClockMHz = source.cpuClockMHz;
    target.gpuUsage = source.gpuUsage;
    target.gpuPower = source.gpuPower;
    target.gpuFanPercent = source.gpuFanPercent;
    target.batteryPercent = source.batteryPercent;
    target.systemPower = source.systemPower;
    target.memoryUsedBytes = source.memoryUsedBytes;
    target.memoryTotalBytes = source.memoryTotalBytes;
    target.gpuMemoryUsedBytes = source.gpuMemoryUsedBytes;
    target.gpuMemoryTotalBytes = source.gpuMemoryTotalBytes;
    target.diskReadBytesPerSecond = source.diskReadBytesPerSecond;
    target.diskWriteBytesPerSecond = source.diskWriteBytesPerSecond;
    target.batteryState = source.batteryState;
    target.downloadBytesPerSecond = source.downloadBytesPerSecond;
    target.uploadBytesPerSecond = source.uploadBytesPerSecond;
    target.cpuTemperatureValid = (source.validMask & CpuTemperatureValid) != 0;
    target.cpuUsageValid = (source.validMask & CpuUsageValid) != 0;
    target.gpuTemperatureValid = (source.validMask & GpuTemperatureValid) != 0;
    target.diskTemperatureValid = (source.validMask & DiskTemperatureValid) != 0;
    target.cpuPowerValid = (source.validMask & CpuPowerValid) != 0;
    target.networkValid = (source.validMask & NetworkValid) != 0;
    target.memoryValid = (source.validMask & MemoryValid) != 0;
    target.cpuClockValid = (source.validMask & CpuClockValid) != 0;
    target.gpuUsageValid = (source.validMask & GpuUsageValid) != 0;
    target.gpuMemoryValid = (source.validMask & GpuMemoryValid) != 0;
    target.gpuPowerValid = (source.validMask & GpuPowerValid) != 0;
    target.gpuFanValid = (source.validMask & GpuFanValid) != 0;
    target.diskIoValid = (source.validMask & DiskIoValid) != 0;
    target.batteryValid = (source.validMask & BatteryValid) != 0;
    target.systemPowerValid = (source.validMask & SystemPowerValid) != 0;
    return target;
}

} // namespace monitor
