#include "SnapshotSensorProvider.h"

namespace monitor {
namespace {

void Add(SensorCollection& sensors, const wchar_t* identifier, const wchar_t* name,
         SensorType type, double value, bool valid, std::uint64_t timestamp) {
    sensors.push_back({identifier, name, type, value, valid, timestamp});
}

} // namespace

void AddSnapshotSensors(const SensorSnapshot& snapshot, std::uint32_t demand,
                        std::uint64_t timestamp, SensorCollection& sensors) {
    if (HasSensorDemand(demand, DemandCpuTemperature))
        Add(sensors, L"cpu.package.temperature", L"CPU Package", SensorType::Temperature,
            snapshot.cpuTemperature, snapshot.cpuTemperatureValid, timestamp);
    if (HasSensorDemand(demand, DemandCpuUsage))
        Add(sensors, L"cpu.usage", L"CPU Usage", SensorType::Load,
            snapshot.cpuUsage, snapshot.cpuUsageValid, timestamp);
    if (HasSensorDemand(demand, DemandCpuPower))
        Add(sensors, L"cpu.package.power", L"CPU Package Power", SensorType::Power,
            snapshot.cpuPower, snapshot.cpuPowerValid, timestamp);
    if (HasSensorDemand(demand, DemandCpuClock))
        Add(sensors, L"cpu.clock", L"CPU Clock", SensorType::Clock,
            snapshot.cpuClockMHz, snapshot.cpuClockValid, timestamp);
    if (HasSensorDemand(demand, DemandMemory)) {
        Add(sensors, L"memory.usage", L"Memory Usage", SensorType::Load,
            snapshot.memoryUsage, snapshot.memoryValid, timestamp);
        Add(sensors, L"memory.used", L"Memory Used", SensorType::Data,
            static_cast<double>(snapshot.memoryUsedBytes), snapshot.memoryValid, timestamp);
    }
    if (HasSensorDemand(demand, DemandNetwork)) {
        Add(sensors, L"network.download", L"Network Download", SensorType::Data,
            static_cast<double>(snapshot.downloadBytesPerSecond), snapshot.networkValid, timestamp);
        Add(sensors, L"network.upload", L"Network Upload", SensorType::Data,
            static_cast<double>(snapshot.uploadBytesPerSecond), snapshot.networkValid, timestamp);
    }
    if (HasSensorDemand(demand, DemandDiskTemperature))
        Add(sensors, L"storage.temperature", L"Storage Temperature", SensorType::Temperature,
            snapshot.diskTemperature, snapshot.diskTemperatureValid, timestamp);
    if (HasSensorDemand(demand, DemandDiskIo))
        Add(sensors, L"storage.read", L"Storage Read", SensorType::Data,
            static_cast<double>(snapshot.diskReadBytesPerSecond), snapshot.diskIoValid, timestamp);
    if (HasSensorDemand(demand, DemandDiskIo))
        Add(sensors, L"storage.write", L"Storage Write", SensorType::Data,
            static_cast<double>(snapshot.diskWriteBytesPerSecond), snapshot.diskIoValid, timestamp);
    if (HasSensorDemand(demand, DemandGpuTemperature))
        Add(sensors, L"gpu.temperature", L"GPU Temperature", SensorType::Temperature,
            snapshot.gpuTemperature, snapshot.gpuTemperatureValid, timestamp);
    if (HasSensorDemand(demand, DemandGpuUsage))
        Add(sensors, L"gpu.usage", L"GPU Usage", SensorType::Load,
            snapshot.gpuUsage, snapshot.gpuUsageValid, timestamp);
    if (HasSensorDemand(demand, DemandVram)) {
        Add(sensors, L"gpu.memory.used", L"GPU Memory Used", SensorType::Data,
            static_cast<double>(snapshot.gpuMemoryUsedBytes), snapshot.gpuMemoryValid, timestamp);
        Add(sensors, L"gpu.memory.total", L"GPU Memory Total", SensorType::Data,
            static_cast<double>(snapshot.gpuMemoryTotalBytes), snapshot.gpuMemoryValid, timestamp);
    }
    if (HasSensorDemand(demand, DemandGpuPower))
        Add(sensors, L"gpu.power", L"GPU Power", SensorType::Power,
            snapshot.gpuPower, snapshot.gpuPowerValid, timestamp);
    if (HasSensorDemand(demand, DemandFan))
        Add(sensors, L"gpu.fan", L"GPU Fan", SensorType::Fan,
            snapshot.gpuFanPercent, snapshot.gpuFanValid, timestamp);
    if (HasSensorDemand(demand, DemandBattery)) {
        Add(sensors, L"battery.percent", L"Battery", SensorType::Data,
            snapshot.batteryPercent, snapshot.batteryValid, timestamp);
        Add(sensors, L"battery.state", L"Battery State", SensorType::Data,
            static_cast<double>(snapshot.batteryState), snapshot.batteryValid, timestamp);
    }
    if (HasSensorDemand(demand, DemandSystemPower))
        Add(sensors, L"system.power", L"System Power", SensorType::Power,
            snapshot.systemPower, snapshot.systemPowerValid, timestamp);
}

} // namespace monitor
