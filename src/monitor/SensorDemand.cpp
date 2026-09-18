#include "SensorDemand.h"

namespace monitor {
namespace {

std::uint32_t DemandForVariable(const std::wstring& name) {
    if (name == L"cpu_temp") return DemandCpuTemperature;
    if (name == L"cpu_usage") return DemandCpuUsage;
    if (name == L"gpu_temp") return DemandGpuTemperature;
    if (name == L"disk_temp" || name == L"ssd_temp") return DemandDiskTemperature;
    if (name == L"down" || name == L"up") return DemandNetwork;
    if (name == L"power") return DemandCpuPower;
    if (name == L"ram_usage" || name == L"ram_used" || name == L"ram_total") return DemandMemory;
    if (name == L"gpu_usage") return DemandGpuUsage;
    if (name == L"vram" || name == L"vram_used" || name == L"vram_total") return DemandVram;
    if (name == L"disk_read" || name == L"disk_write") return DemandDiskIo;
    if (name == L"cpu_clock") return DemandCpuClock;
    if (name == L"gpu_power") return DemandGpuPower;
    if (name == L"fan") return DemandFan;
    if (name == L"battery" || name == L"battery_percent" || name == L"battery_status") return DemandBattery;
    if (name == L"system_power") return DemandSystemPower;
    return 0;
}

} // namespace

std::uint32_t SensorDemandForMetricIndex(int index) {
    static constexpr std::uint32_t flags[] = {
        DemandCpuTemperature, DemandCpuUsage, DemandGpuTemperature, DemandDiskTemperature,
        DemandNetwork, DemandCpuPower, DemandMemory, DemandGpuUsage, DemandVram, DemandDiskIo,
        DemandCpuClock, DemandGpuPower, DemandFan, DemandBattery, DemandSystemPower};
    return index >= 0 && index < static_cast<int>(sizeof(flags) / sizeof(flags[0]))
        ? flags[index] : 0;
}

std::uint32_t SensorDemandFromMetrics(const Config& config) {
    std::uint32_t demand = 0;
    if (config.showCpuTemperature) demand |= DemandCpuTemperature;
    if (config.showCpuUsage) demand |= DemandCpuUsage;
    if (config.showGpuTemperature) demand |= DemandGpuTemperature;
    if (config.showDiskTemperature) demand |= DemandDiskTemperature;
    if (config.showNetwork) demand |= DemandNetwork;
    if (config.showPower) demand |= DemandCpuPower;
    if (config.showMemory) demand |= DemandMemory;
    if (config.showGpuUsage) demand |= DemandGpuUsage;
    if (config.showVram) demand |= DemandVram;
    if (config.showDiskIo) demand |= DemandDiskIo;
    if (config.showCpuClock) demand |= DemandCpuClock;
    if (config.showGpuPower) demand |= DemandGpuPower;
    if (config.showFan) demand |= DemandFan;
    if (config.showBattery) demand |= DemandBattery;
    if (config.showSystemPower) demand |= DemandSystemPower;
    return demand;
}

std::uint32_t SensorDemandFromFormat(const std::wstring& format) {
    std::uint32_t demand = 0;
    for (std::size_t index = 0; index < format.size();) {
        if (format[index] != L'{') {
            ++index;
            continue;
        }
        const auto end = format.find(L'}', index + 1);
        if (end == std::wstring::npos) break;
        demand |= DemandForVariable(format.substr(index + 1, end - index - 1));
        index = end + 1;
    }
    return demand;
}

std::uint32_t SensorDemandFromConfig(const Config& config) {
    if (!config.taskbarEnabled) return 0;
    return config.taskbarFormat.empty()
        ? SensorDemandFromMetrics(config)
        : SensorDemandFromFormat(config.taskbarFormat);
}

void ApplySensorDemandToMetrics(Config& config, std::uint32_t demand) {
    config.showCpuTemperature = HasSensorDemand(demand, DemandCpuTemperature);
    config.showCpuUsage = HasSensorDemand(demand, DemandCpuUsage);
    config.showGpuTemperature = HasSensorDemand(demand, DemandGpuTemperature);
    config.showDiskTemperature = HasSensorDemand(demand, DemandDiskTemperature);
    config.showNetwork = HasSensorDemand(demand, DemandNetwork);
    config.showPower = HasSensorDemand(demand, DemandCpuPower);
    config.showMemory = HasSensorDemand(demand, DemandMemory);
    config.showGpuUsage = HasSensorDemand(demand, DemandGpuUsage);
    config.showVram = HasSensorDemand(demand, DemandVram);
    config.showDiskIo = HasSensorDemand(demand, DemandDiskIo);
    config.showCpuClock = HasSensorDemand(demand, DemandCpuClock);
    config.showGpuPower = HasSensorDemand(demand, DemandGpuPower);
    config.showFan = HasSensorDemand(demand, DemandFan);
    config.showBattery = HasSensorDemand(demand, DemandBattery);
    config.showSystemPower = HasSensorDemand(demand, DemandSystemPower);
}

} // namespace monitor
