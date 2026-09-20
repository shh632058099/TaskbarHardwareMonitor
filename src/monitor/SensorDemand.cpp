#include "SensorDemand.h"
#include <cwctype>
#include <functional>

namespace monitor {
namespace {

std::wstring BaseVariable(std::wstring name) {
    const auto question = name.find(L'?');
    if (question != std::wstring::npos) name.resize(question);
    const auto colon = name.find(L':');
    if (colon != std::wstring::npos) name.resize(colon);
    while (!name.empty() && iswspace(name.front())) name.erase(name.begin());
    while (!name.empty() && iswspace(name.back())) name.pop_back();
    return name;
}

std::uint32_t DemandForVariable(const std::wstring& spec) {
    const auto name = BaseVariable(spec);
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
    if (name == L"cpu_internal_temp") return DemandCpuInternalTemperature;
    if (name == L"gpu_internal_temp") return DemandGpuInternalTemperature;
    if (name == L"cpu_fan_rpm") return DemandCpuFanRpm;
    if (name == L"gpu_fan_rpm") return DemandGpuFanRpm;
    return 0;
}

std::size_t FindMatchingBrace(const std::wstring& text, std::size_t start) {
    int depth = 0;
    for (std::size_t index = start; index < text.size(); ++index) {
        if (text[index] == L'{') ++depth;
        else if (text[index] == L'}' && --depth == 0) return index;
    }
    return std::wstring::npos;
}

std::size_t FindTopLevelQuestion(const std::wstring& expression) {
    int depth = 0;
    for (std::size_t index = 0; index < expression.size(); ++index) {
        if (expression[index] == L'{') ++depth;
        else if (expression[index] == L'}') --depth;
        else if (expression[index] == L'?' && depth == 0) return index;
    }
    return std::wstring::npos;
}

} // namespace

std::uint32_t SensorDemandForMetricIndex(int index) {
    static constexpr std::uint32_t flags[] = {
        DemandCpuTemperature, DemandCpuUsage, DemandGpuTemperature, DemandDiskTemperature,
        DemandNetwork, DemandCpuPower, DemandMemory, DemandGpuUsage, DemandVram, DemandDiskIo,
        DemandCpuClock, DemandGpuPower, DemandFan, DemandBattery, DemandSystemPower,
        DemandCpuInternalTemperature, DemandGpuInternalTemperature,
        DemandCpuFanRpm, DemandGpuFanRpm};
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
    if (config.showCpuInternalTemperature) demand |= DemandCpuInternalTemperature;
    if (config.showGpuInternalTemperature) demand |= DemandGpuInternalTemperature;
    if (config.showCpuFanRpm) demand |= DemandCpuFanRpm;
    if (config.showGpuFanRpm) demand |= DemandGpuFanRpm;
    return demand;
}

std::uint32_t SensorDemandFromFormat(const std::wstring& format) {
    std::uint32_t demand = 0;
    std::function<void(const std::wstring&)> scan;
    scan = [&](const std::wstring& text) {
        for (std::size_t index = 0; index < text.size();) {
            if (text[index] != L'{') {
                ++index;
                continue;
            }
            const auto end = FindMatchingBrace(text, index);
            if (end == std::wstring::npos) break;
            const auto expression = text.substr(index + 1, end - index - 1);
            const auto question = FindTopLevelQuestion(expression);
            if (question != std::wstring::npos) {
                demand |= DemandForVariable(expression.substr(0, question));
                scan(expression.substr(question + 1));
            } else {
                demand |= DemandForVariable(expression);
            }
            index = end + 1;
        }
    };
    scan(format);
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
    config.showCpuInternalTemperature = HasSensorDemand(demand, DemandCpuInternalTemperature);
    config.showGpuInternalTemperature = HasSensorDemand(demand, DemandGpuInternalTemperature);
    config.showCpuFanRpm = HasSensorDemand(demand, DemandCpuFanRpm);
    config.showGpuFanRpm = HasSensorDemand(demand, DemandGpuFanRpm);
}

} // namespace monitor
