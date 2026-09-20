#include "TaskbarLayout.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <functional>
#include <sstream>
#include <utility>

namespace monitor {
namespace {
struct LayoutWidths {
    int temperature;
    int usage;
    int network;
    int power;
};

LayoutWidths WidthsFor(DisplayMode mode) {
    return mode == DisplayMode::Compact
        ? LayoutWidths{36, 40, 0, 42}
        : LayoutWidths{60, 54, 0, 60};
}

std::wstring Integer(double value) {
    return std::to_wstring(static_cast<int>(std::lround(value)));
}
std::wstring FormatTemperatureValue(double value, bool valid) {
    return valid ? Integer(value) + L"\u00B0" : L"--\u00B0";
}
std::wstring FormatUsage(double value, bool valid) {
    return valid ? Integer(value) + L"%" : L"--%";
}
std::wstring FormatPower(double value, bool valid) {
    return valid ? Integer(value) + L"W" : L"--W";
}
std::wstring FormatBytesCompact(std::uint64_t bytes, bool valid) {
    if (!valid) return L"--";
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << L"G";
    } else {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << L"M";
    }
    return stream.str();
}
std::wstring FormatClock(double mhz, bool valid) {
    if (!valid) return L"--";
    std::wstringstream stream;
    if (mhz >= 1000.0) {
        stream << std::fixed << std::setprecision(1) << mhz / 1000.0 << L"G";
    } else {
        stream << static_cast<int>(std::lround(mhz)) << L"M";
    }
    return stream.str();
}
std::wstring FormatVram(const SensorSnapshot& snapshot) {
    if (!snapshot.gpuMemoryValid || snapshot.gpuMemoryTotalBytes == 0) return L"--";
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(1)
           << static_cast<double>(snapshot.gpuMemoryUsedBytes) / (1024.0 * 1024.0 * 1024.0)
           << L"/"
           << static_cast<double>(snapshot.gpuMemoryTotalBytes) / (1024.0 * 1024.0 * 1024.0)
           << L"G";
    return stream.str();
}

std::wstring BatterySuffix(std::uint32_t state, bool compact) {
    switch (state) {
    case BatteryCharging: return compact ? L"+" : L" CHG";
    case BatteryFull: return compact ? L"\u2713" : L" FULL";
    case BatteryOnAc: return compact ? L"A" : L" AC";
    default: return {};
    }
}
void Add(TaskbarLayout& layout, int row, const wchar_t* label, std::wstring value, int width,
         AlertSeverity severity = AlertSeverity::Normal) {
    row = row == 1 ? 1 : 0;
    if (width == 0) {
        width = static_cast<int>(label ? std::wcslen(label) : 0) * 8 +
                static_cast<int>(value.size()) * 8 + 12;
    }
    layout.cells.push_back({label, std::move(value), width, row, severity});
    layout.rowWidths[row] += width;
    layout.width = std::max(layout.rowWidths[0], layout.rowWidths[1]);
}

std::wstring Trim(std::wstring value) {
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    return value;
}

std::size_t FindTopLevelQuestion(const std::wstring& expression);

void SplitVariableSpec(const std::wstring& spec, std::wstring& name, std::wstring& modifier) {
    const auto colon = spec.find(L':');
    name = Trim(colon == std::wstring::npos ? spec : spec.substr(0, colon));
    modifier = colon == std::wstring::npos ? std::wstring{} : Trim(spec.substr(colon + 1));
}

bool IsKnownFormatVariable(const std::wstring& name) {
    return name == L"cpu_temp" || name == L"cpu_usage" || name == L"gpu_temp" ||
           name == L"disk_temp" || name == L"ssd_temp" || name == L"down" || name == L"up" ||
           name == L"power" || name == L"ram_usage" || name == L"ram_used" ||
           name == L"ram_total" || name == L"gpu_usage" || name == L"vram" ||
           name == L"vram_used" || name == L"vram_total" || name == L"disk_read" ||
           name == L"disk_write" || name == L"cpu_clock" || name == L"gpu_power" ||
           name == L"fan" || name == L"battery" || name == L"battery_percent" ||
           name == L"battery_status" || name == L"system_power";
}

bool IsValidFormatModifier(const std::wstring& name, const std::wstring& modifier) {
    if (modifier.empty()) return true;
    if (modifier == L"1") {
        return name == L"cpu_temp" || name == L"cpu_usage" || name == L"gpu_temp" ||
               name == L"disk_temp" || name == L"ssd_temp" || name == L"power" ||
               name == L"ram_usage" || name == L"gpu_usage" || name == L"gpu_power" ||
               name == L"fan" || name == L"battery_percent" || name == L"system_power";
    }
    if (modifier == L"short") return name == L"down" || name == L"up" ||
                                        name == L"disk_read" || name == L"disk_write" ||
                                        name == L"battery";
    if (modifier == L"kb" || modifier == L"mb" || modifier == L"gb") {
        return name == L"down" || name == L"up" || name == L"disk_read" ||
               name == L"disk_write" || name == L"ram_used" || name == L"ram_total" ||
               name == L"vram_used" || name == L"vram_total";
    }
    return (modifier == L"ghz" || modifier == L"mhz") && name == L"cpu_clock";
}

std::wstring ValidateFormatExpression(const std::wstring& expression) {
    const auto question = FindTopLevelQuestion(expression);
    const std::wstring variable = Trim(expression.substr(0, question));
    std::wstring name;
    std::wstring modifier;
    SplitVariableSpec(variable, name, modifier);
    if (name.empty()) return L"Variable name is empty";
    if (!IsKnownFormatVariable(name)) return L"Unknown variable: " + name;
    if (!IsValidFormatModifier(name, modifier)) return L"Invalid modifier: " + modifier;
    if (question != std::wstring::npos && Trim(expression.substr(question + 1)).empty()) {
        return L"Conditional section is empty";
    }
    return {};
}

std::wstring FormatFixed(double value, bool valid, int precision, const wchar_t* unit) {
    if (!valid) return L"--" + std::wstring(unit);
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(precision) << value << unit;
    return stream.str();
}

std::wstring FormatBytesForced(std::uint64_t bytes, bool valid, const std::wstring& modifier) {
    if (!valid) return L"--";
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (modifier == L"gb") {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << L" GB";
    } else if (modifier == L"mb") {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << L" MB";
    } else {
        return FormatBytesCompact(bytes, valid);
    }
    return stream.str();
}

std::wstring FormatRateForced(std::uint64_t bytes, bool valid, const std::wstring& modifier) {
    if (!valid) return L"--";
    std::wstringstream stream;
    stream << std::fixed;
    if (modifier == L"short") {
        if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
            const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
            stream << std::setprecision(gb < 10.0 ? 1 : 0) << gb << L"G";
        } else if (bytes >= 1024ULL * 1024ULL) {
            const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
            stream << std::setprecision(mb < 100.0 ? 1 : 0) << mb << L"M";
        } else {
            stream << std::setprecision(0) << static_cast<double>(bytes) / 1024.0 << L"K";
        }
    } else if (modifier == L"kb") {
        stream << std::setprecision(0) << static_cast<double>(bytes) / 1024.0 << L" KB/s";
    } else if (modifier == L"mb") {
        stream << std::setprecision(1) << static_cast<double>(bytes) / (1024.0 * 1024.0) << L" MB/s";
    } else if (modifier == L"gb") {
        stream << std::setprecision(1) << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << L" GB/s";
    } else {
        return FormatNetworkSpeed(bytes);
    }
    return stream.str();
}

bool FormatVariable(const std::wstring& spec, const SensorSnapshot& snapshot,
                    std::wstring& value, std::wstring& stable) {
    std::wstring name;
    std::wstring modifier;
    SplitVariableSpec(spec, name, modifier);
    const bool oneDecimal = modifier == L"1";
    if (name == L"cpu_temp") {
        value = oneDecimal ? FormatFixed(snapshot.cpuTemperature, snapshot.cpuTemperatureValid, 1, L"\u00B0")
                           : FormatTemperatureValue(snapshot.cpuTemperature, snapshot.cpuTemperatureValid);
        stable = oneDecimal ? L"100.0\u00B0" : L"100\u00B0";
    } else if (name == L"cpu_usage") {
        value = oneDecimal ? FormatFixed(snapshot.cpuUsage, snapshot.cpuUsageValid, 1, L"%")
                           : FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid);
        stable = oneDecimal ? L"100.0%" : L"100%";
    } else if (name == L"gpu_temp") {
        value = oneDecimal ? FormatFixed(snapshot.gpuTemperature, snapshot.gpuTemperatureValid, 1, L"\u00B0")
                           : FormatTemperatureValue(snapshot.gpuTemperature, snapshot.gpuTemperatureValid);
        stable = oneDecimal ? L"100.0\u00B0" : L"100\u00B0";
    } else if (name == L"disk_temp" || name == L"ssd_temp") {
        value = oneDecimal ? FormatFixed(snapshot.diskTemperature, snapshot.diskTemperatureValid, 1, L"\u00B0")
                           : FormatTemperatureValue(snapshot.diskTemperature, snapshot.diskTemperatureValid);
        stable = oneDecimal ? L"100.0\u00B0" : L"100\u00B0";
    } else if (name == L"down" || name == L"up") {
        const auto bytes = name == L"down" ? snapshot.downloadBytesPerSecond : snapshot.uploadBytesPerSecond;
        value = FormatRateForced(bytes, snapshot.networkValid, modifier);
        stable = (modifier == L"short") ? L"1023M" :
                 (modifier == L"kb") ? L"99999 KB/s" :
                 (modifier == L"gb") ? L"99.9 GB/s" : L"999.9 MB/s";
    } else if (name == L"power") {
        value = oneDecimal ? FormatFixed(snapshot.cpuPower, snapshot.cpuPowerValid, 1, L"W")
                           : FormatPower(snapshot.cpuPower, snapshot.cpuPowerValid);
        stable = oneDecimal ? L"999.9W" : L"999W";
    } else if (name == L"ram_usage") {
        value = oneDecimal ? FormatFixed(snapshot.memoryUsage, snapshot.memoryValid, 1, L"%")
                           : FormatUsage(snapshot.memoryUsage, snapshot.memoryValid);
        stable = oneDecimal ? L"100.0%" : L"100%";
    } else if (name == L"ram_used") {
        value = FormatBytesForced(snapshot.memoryUsedBytes, snapshot.memoryValid, modifier);
        stable = modifier == L"mb" ? L"99999.9 MB" : modifier == L"gb" ? L"999.9 GB" : L"99.9G";
    } else if (name == L"ram_total") {
        value = FormatBytesForced(snapshot.memoryTotalBytes, snapshot.memoryValid, modifier);
        stable = modifier == L"mb" ? L"99999.9 MB" : modifier == L"gb" ? L"999.9 GB" : L"99.9G";
    } else if (name == L"gpu_usage") {
        value = oneDecimal ? FormatFixed(snapshot.gpuUsage, snapshot.gpuUsageValid, 1, L"%")
                           : FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid);
        stable = oneDecimal ? L"100.0%" : L"100%";
    } else if (name == L"vram") {
        value = FormatVram(snapshot);
        stable = L"99.9/99.9G";
    } else if (name == L"vram_used") {
        value = FormatBytesForced(snapshot.gpuMemoryUsedBytes, snapshot.gpuMemoryValid, modifier);
        stable = modifier == L"mb" ? L"99999.9 MB" : modifier == L"gb" ? L"999.9 GB" : L"99.9G";
    } else if (name == L"vram_total") {
        value = FormatBytesForced(snapshot.gpuMemoryTotalBytes, snapshot.gpuMemoryValid, modifier);
        stable = modifier == L"mb" ? L"99999.9 MB" : modifier == L"gb" ? L"999.9 GB" : L"99.9G";
    } else if (name == L"disk_read" || name == L"disk_write") {
        const auto bytes = name == L"disk_read" ? snapshot.diskReadBytesPerSecond : snapshot.diskWriteBytesPerSecond;
        value = FormatRateForced(bytes, snapshot.diskIoValid, modifier);
        stable = (modifier == L"short") ? L"1023M" :
                 (modifier == L"kb") ? L"99999 KB/s" :
                 (modifier == L"gb") ? L"99.9 GB/s" : L"999.9 MB/s";
    } else if (name == L"cpu_clock") {
        if (modifier == L"ghz") value = FormatFixed(snapshot.cpuClockMHz / 1000.0, snapshot.cpuClockValid, 1, L" GHz");
        else if (modifier == L"mhz") value = FormatFixed(snapshot.cpuClockMHz, snapshot.cpuClockValid, 0, L" MHz");
        else value = FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid);
        stable = modifier == L"ghz" ? L"9.9 GHz" : modifier == L"mhz" ? L"9999 MHz" : L"9.9G";
    } else if (name == L"gpu_power") {
        value = oneDecimal ? FormatFixed(snapshot.gpuPower, snapshot.gpuPowerValid, 1, L"W")
                           : FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid);
        stable = oneDecimal ? L"999.9W" : L"999W";
    } else if (name == L"fan") {
        value = oneDecimal ? FormatFixed(snapshot.gpuFanPercent, snapshot.gpuFanValid, 1, L"%")
                           : FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid);
        stable = oneDecimal ? L"100.0%" : L"100%";
    } else if (name == L"battery") {
        const bool shortBattery = modifier == L"short";
        value = FormatBattery(snapshot, shortBattery);
        stable = shortBattery ? L"100%\u2713" : L"100% FULL";
    } else if (name == L"battery_percent") {
        value = oneDecimal ? FormatFixed(snapshot.batteryPercent, snapshot.batteryValid, 1, L"%")
                           : FormatUsage(snapshot.batteryPercent, snapshot.batteryValid);
        stable = oneDecimal ? L"100.0%" : L"100%";
    } else if (name == L"battery_status") {
        value = snapshot.batteryValid ? FormatBatteryStatus(snapshot.batteryState) : L"Unknown";
        stable = L"Fully charged";
    } else if (name == L"system_power") {
        value = oneDecimal ? FormatFixed(snapshot.systemPower, snapshot.systemPowerValid, 1, L"W")
                           : FormatPower(snapshot.systemPower, snapshot.systemPowerValid);
        stable = oneDecimal ? L"999.9W" : L"999W";
    } else {
        return false;
    }
    return true;
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

void AddTextRun(TaskbarTextLayout& layout, int row, int column, std::wstring text,
                std::wstring stableText, bool value,
                AlertSeverity severity = AlertSeverity::Normal) {
    if (text.empty() && stableText.empty()) return;
    layout.runs.push_back({std::move(text), std::move(stableText), value, row, column, severity});
    layout.columns = std::max(layout.columns, column + 1);
}
}

bool IsFormatVariableAvailable(const std::wstring& spec, const SensorSnapshot& snapshot) {
    std::wstring name;
    std::wstring modifier;
    SplitVariableSpec(spec, name, modifier);
    if (name == L"cpu_temp") return snapshot.cpuTemperatureValid;
    if (name == L"cpu_usage") return snapshot.cpuUsageValid;
    if (name == L"gpu_temp") return snapshot.gpuTemperatureValid;
    if (name == L"disk_temp" || name == L"ssd_temp") return snapshot.diskTemperatureValid;
    if (name == L"down" || name == L"up") return snapshot.networkValid;
    if (name == L"power") return snapshot.cpuPowerValid;
    if (name == L"ram_usage" || name == L"ram_used" || name == L"ram_total") return snapshot.memoryValid;
    if (name == L"gpu_usage") return snapshot.gpuUsageValid;
    if (name == L"vram" || name == L"vram_used" || name == L"vram_total") return snapshot.gpuMemoryValid;
    if (name == L"disk_read" || name == L"disk_write") return snapshot.diskIoValid;
    if (name == L"cpu_clock") return snapshot.cpuClockValid;
    if (name == L"gpu_power") return snapshot.gpuPowerValid;
    if (name == L"fan") return snapshot.gpuFanValid;
    if (name == L"battery" || name == L"battery_percent" || name == L"battery_status") return snapshot.batteryValid;
    if (name == L"system_power") return snapshot.systemPowerValid;
    return false;
}

AlertSeverity AlertSeverityForVariable(const std::wstring& spec,
                                       const SensorSnapshot& snapshot,
                                       const Config& config) {
    if (!config.thresholdColorsEnabled) return AlertSeverity::Normal;
    std::wstring name;
    std::wstring modifier;
    SplitVariableSpec(spec, name, modifier);
    auto highSeverity = [](double value, bool valid, int warning, int critical) {
        if (!valid) return AlertSeverity::Normal;
        if (value >= critical) return AlertSeverity::Critical;
        if (value >= warning) return AlertSeverity::Warning;
        return AlertSeverity::Normal;
    };
    auto lowSeverity = [](double value, bool valid, int warning, int critical) {
        if (!valid) return AlertSeverity::Normal;
        if (value <= critical) return AlertSeverity::Critical;
        if (value <= warning) return AlertSeverity::Warning;
        return AlertSeverity::Normal;
    };
    if (name == L"cpu_temp")
        return highSeverity(snapshot.cpuTemperature, snapshot.cpuTemperatureValid,
                            config.cpuTempWarning, config.cpuTempCritical);
    if (name == L"gpu_temp")
        return highSeverity(snapshot.gpuTemperature, snapshot.gpuTemperatureValid,
                            config.gpuTempWarning, config.gpuTempCritical);
    if (name == L"ram_usage" || name == L"ram_used" || name == L"ram_total")
        return highSeverity(snapshot.memoryUsage, snapshot.memoryValid,
                            config.ramWarning, config.ramCritical);
    if (name == L"battery" || name == L"battery_percent" || name == L"battery_status")
        return lowSeverity(snapshot.batteryPercent, snapshot.batteryValid,
                           config.batteryWarning, config.batteryCritical);
    return AlertSeverity::Normal;
}

std::wstring FormatNetworkSpeed(std::uint64_t bytesPerSecond) {
    std::wstringstream stream;
    const double value = static_cast<double>(bytesPerSecond);
    stream << std::fixed;
    if (bytesPerSecond >= 1024ULL * 1024ULL * 1024ULL) {
        stream << std::setprecision(1) << value / (1024.0 * 1024.0 * 1024.0) << L" GB/s";
    } else if (bytesPerSecond >= 1024ULL * 1024ULL) {
        stream << std::setprecision(1) << value / (1024.0 * 1024.0) << L" MB/s";
    } else {
        stream << std::setprecision(0) << value / 1024.0 << L" KB/s";
    }
    return stream.str();
}

TaskbarLayout BuildTaskbarLayout(const SensorSnapshot& snapshot, const Config& config) {
    TaskbarLayout layout;
    layout.rows = config.taskbarRows == 2 ? 2 : 1;
    const auto widths = WidthsFor(config.displayMode);
    const int temperaturesRow = 0;
    const int secondaryRow = layout.rows == 2 ? 1 : 0;
    if (config.displayMode == DisplayMode::Compact) {
        if (config.showCpuTemperature) {
            Add(layout, temperaturesRow, L"C", FormatTemperatureValue(snapshot.cpuTemperature,
                                                       snapshot.cpuTemperatureValid),
                widths.temperature, AlertSeverityForVariable(L"cpu_temp", snapshot, config));
        }
        if (config.showCpuUsage) {
            Add(layout, temperaturesRow, L"U", FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid),
                widths.usage);
        }
        if (config.showGpuTemperature) {
            Add(layout, temperaturesRow, L"G", FormatTemperatureValue(snapshot.gpuTemperature,
                                                       snapshot.gpuTemperatureValid),
                widths.temperature, AlertSeverityForVariable(L"gpu_temp", snapshot, config));
        }
        if (config.showDiskTemperature) {
            Add(layout, temperaturesRow, L"D", FormatTemperatureValue(snapshot.diskTemperature,
                                                       snapshot.diskTemperatureValid),
                widths.temperature);
        }
        if (config.showNetwork) {
            Add(layout, secondaryRow, L"\u2193", snapshot.networkValid
                    ? FormatNetworkSpeed(snapshot.downloadBytesPerSecond) : L"--",
                widths.network);
            Add(layout, secondaryRow, L"\u2191", snapshot.networkValid
                    ? FormatNetworkSpeed(snapshot.uploadBytesPerSecond) : L"--",
                widths.network);
        }
        if (config.showPower) {
            Add(layout, secondaryRow, L"P", FormatPower(snapshot.cpuPower, snapshot.cpuPowerValid),
                widths.power);
        }
        if (config.showMemory) Add(layout, temperaturesRow, L"R", FormatUsage(snapshot.memoryUsage, snapshot.memoryValid), widths.usage,
                                   AlertSeverityForVariable(L"ram_usage", snapshot, config));
        if (config.showGpuUsage) Add(layout, temperaturesRow, L"GU", FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid), widths.usage);
        if (config.showVram) Add(layout, temperaturesRow, L"V", FormatVram(snapshot), widths.network * 2);
        if (config.showDiskIo) {
            Add(layout, secondaryRow, L"DR", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskReadBytesPerSecond) : L"--", widths.network);
            Add(layout, secondaryRow, L"DW", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskWriteBytesPerSecond) : L"--", widths.network);
        }
        if (config.showCpuClock) Add(layout, temperaturesRow, L"F", FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid), widths.network);
        if (config.showGpuPower) Add(layout, secondaryRow, L"GP", FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid), widths.power);
        if (config.showFan) Add(layout, secondaryRow, L"FAN", FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid), widths.usage);
        if (config.showBattery) Add(layout, secondaryRow, L"B", FormatBattery(snapshot, true), widths.usage,
                                    AlertSeverityForVariable(L"battery", snapshot, config));
        if (config.showSystemPower) Add(layout, secondaryRow, L"SYS", FormatPower(snapshot.systemPower, snapshot.systemPowerValid), widths.power);
    } else {
        if (config.showCpuTemperature) {
            Add(layout, temperaturesRow, L"CPU", FormatTemperatureValue(snapshot.cpuTemperature,
                                                         snapshot.cpuTemperatureValid),
                widths.temperature, AlertSeverityForVariable(L"cpu_temp", snapshot, config));
        }
        if (config.showCpuUsage) {
            Add(layout, temperaturesRow, L"LOAD", FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid),
                widths.usage);
        }
        if (config.showGpuTemperature) {
            Add(layout, temperaturesRow, L"GPU", FormatTemperatureValue(snapshot.gpuTemperature,
                                                         snapshot.gpuTemperatureValid),
                widths.temperature, AlertSeverityForVariable(L"gpu_temp", snapshot, config));
        }
        if (config.showDiskTemperature) {
            Add(layout, temperaturesRow, L"DISK", FormatTemperatureValue(snapshot.diskTemperature,
                                                         snapshot.diskTemperatureValid),
                widths.temperature);
        }
        if (config.showNetwork) {
            Add(layout, secondaryRow, L"\u2193", snapshot.networkValid
                    ? FormatNetworkSpeed(snapshot.downloadBytesPerSecond) : L"--",
                widths.network);
            Add(layout, secondaryRow, L"\u2191", snapshot.networkValid
                    ? FormatNetworkSpeed(snapshot.uploadBytesPerSecond) : L"--",
                widths.network);
        }
        if (config.showPower) {
            Add(layout, secondaryRow, L"PWR", FormatPower(snapshot.cpuPower, snapshot.cpuPowerValid),
                widths.power);
        }
        if (config.showMemory) Add(layout, temperaturesRow, L"RAM", FormatUsage(snapshot.memoryUsage, snapshot.memoryValid), widths.usage,
                                   AlertSeverityForVariable(L"ram_usage", snapshot, config));
        if (config.showGpuUsage) Add(layout, temperaturesRow, L"GLOAD", FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid), widths.usage);
        if (config.showVram) Add(layout, temperaturesRow, L"VRAM", FormatVram(snapshot), widths.network * 2);
        if (config.showDiskIo) {
            Add(layout, secondaryRow, L"READ", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskReadBytesPerSecond) : L"--", widths.network);
            Add(layout, secondaryRow, L"WRITE", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskWriteBytesPerSecond) : L"--", widths.network);
        }
        if (config.showCpuClock) Add(layout, temperaturesRow, L"CLK", FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid), widths.network);
        if (config.showGpuPower) Add(layout, secondaryRow, L"GPWR", FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid), widths.power);
        if (config.showFan) Add(layout, secondaryRow, L"FAN", FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid), widths.usage);
        if (config.showBattery) Add(layout, secondaryRow, L"BAT", FormatBattery(snapshot, false), widths.usage * 2,
                                    AlertSeverityForVariable(L"battery", snapshot, config));
        if (config.showSystemPower) Add(layout, secondaryRow, L"SYS", FormatPower(snapshot.systemPower, snapshot.systemPowerValid), widths.power);
    }
    return layout;
}

TaskbarTextLayout BuildFormattedTaskbarLayout(const SensorSnapshot& snapshot,
                                               const std::wstring& format,
                                               const Config* config) {
    TaskbarTextLayout layout;
    int row = 0;
    int column = 0;
    std::wstring literal;
    auto flushLiteral = [&]() {
        if (literal.empty()) return;
        AddTextRun(layout, row, column, literal, literal, false);
        literal.clear();
    };

    std::function<void(const std::wstring&)> process;
    process = [&](const std::wstring& text) {
        for (std::size_t index = 0; index < text.size();) {
            const bool escapedNewline = text[index] == L'\\' && index + 1 < text.size() &&
                                        text[index + 1] == L'n';
            const bool escapedTab = text[index] == L'\\' && index + 1 < text.size() &&
                                    text[index + 1] == L't';
            const bool realNewline = text[index] == L'\n' || text[index] == L'\r';
            const bool realTab = text[index] == L'\t';
            if (escapedNewline || realNewline) {
                flushLiteral();
                if (row == 0) {
                    row = 1;
                    column = 0;
                    layout.rows = 2;
                } else {
                    literal.push_back(L' ');
                }
                if (escapedNewline) index += 2;
                else {
                    if (text[index] == L'\r' && index + 1 < text.size() && text[index + 1] == L'\n') ++index;
                    ++index;
                }
                continue;
            }

            if (escapedTab || realTab) {
                flushLiteral();
                ++column;
                layout.columns = std::max(layout.columns, column + 1);
                index += escapedTab ? 2 : 1;
                continue;
            }

            if (text[index] == L'{') {
                const auto end = FindMatchingBrace(text, index);
                if (end != std::wstring::npos) {
                    const auto expression = text.substr(index + 1, end - index - 1);
                    const auto question = FindTopLevelQuestion(expression);
                    if (question != std::wstring::npos) {
                        const auto condition = Trim(expression.substr(0, question));
                        const auto body = expression.substr(question + 1);
                        flushLiteral();
                        if (IsFormatVariableAvailable(condition, snapshot)) process(body);
                        index = end + 1;
                        continue;
                    }

                    std::wstring value;
                    std::wstring stable;
                    if (FormatVariable(expression, snapshot, value, stable)) {
                        flushLiteral();
                        const AlertSeverity severity = config
                            ? AlertSeverityForVariable(expression, snapshot, *config)
                            : AlertSeverity::Normal;
                        AddTextRun(layout, row, column, std::move(value), std::move(stable),
                                   true, severity);
                        index = end + 1;
                        continue;
                    }
                    literal.append(text, index, end - index + 1);
                    index = end + 1;
                    continue;
                }
            }

            literal.push_back(text[index]);
            ++index;
        }
    };

    process(format);
    flushLiteral();
    return layout;
}

int TaskbarWidth(const Config& config) {
    return BuildTaskbarLayout(SensorSnapshot{}, config).width + 4;
}

std::wstring FormatBatteryStatus(std::uint32_t state) {
    switch (state) {
    case BatteryCharging: return L"Charging";
    case BatteryFull: return L"Fully charged";
    case BatteryOnAc: return L"On AC";
    case BatteryDischarging: return L"Discharging";
    default: return L"Unknown";
    }
}

std::wstring FormatBattery(const SensorSnapshot& snapshot, bool compact) {
    if (!snapshot.batteryValid) return L"--%";
    return FormatUsage(snapshot.batteryPercent, true) + BatterySuffix(snapshot.batteryState, compact);
}

std::wstring FormatTemperature(double value, bool valid) {
    return FormatTemperatureValue(value, valid);
}

std::wstring ValidateTaskbarFormat(const std::wstring& format) {
    for (std::size_t index = 0; index < format.size();) {
        if (format[index] == L'}') return L"Unexpected }";
        if (format[index] != L'{') {
            ++index;
            continue;
        }
        const auto end = FindMatchingBrace(format, index);
        if (end == std::wstring::npos) return L"Missing closing }";
        const auto error = ValidateFormatExpression(format.substr(index + 1, end - index - 1));
        if (!error.empty()) return error;
        const auto question = FindTopLevelQuestion(format.substr(index + 1, end - index - 1));
        if (question != std::wstring::npos) {
            const auto expression = format.substr(index + 1, end - index - 1);
            const auto nested = ValidateTaskbarFormat(expression.substr(question + 1));
            if (!nested.empty()) return nested;
        }
        index = end + 1;
    }
    return {};
}

std::wstring BuildDiagnosticsText(const SensorSnapshot& snapshot) {
    const auto state = [](bool valid, const wchar_t* unavailable) {
        return valid ? std::wstring(L"Available") : std::wstring(unavailable);
    };
    std::wstring text;
    auto append = [&](const wchar_t* label, const std::wstring& value) {
        if (!text.empty()) text += L"\r\n";
        text += label;
        text += L": ";
        text += value;
    };
    append(L"CPU temperature", state(snapshot.cpuTemperatureValid, L"Unavailable"));
    append(L"CPU usage", state(snapshot.cpuUsageValid, L"Unavailable"));
    append(L"CPU power", state(snapshot.cpuPowerValid, L"Unavailable"));
    append(L"Memory", state(snapshot.memoryValid, L"Unavailable"));
    append(L"CPU clock", state(snapshot.cpuClockValid, L"Unavailable"));
    append(L"Network", state(snapshot.networkValid, L"Unavailable"));
    append(L"GPU", state(snapshot.gpuTemperatureValid || snapshot.gpuUsageValid ||
                           snapshot.gpuMemoryValid || snapshot.gpuPowerValid || snapshot.gpuFanValid,
                           L"Unavailable (driver or supported GPU not found)"));
    append(L"Disk temperature", state(snapshot.diskTemperatureValid,
                                       L"Unavailable (no supported drive)"));
    append(L"Disk I/O", state(snapshot.diskIoValid,
                                L"Unavailable (needs a second sample, or disk performance counters are unavailable)"));
    append(L"Battery", state(snapshot.batteryValid, L"Unavailable (no battery or unsupported state)"));
    append(L"System power", state(snapshot.systemPowerValid,
                                   L"Unavailable (no battery or unsupported state)"));
    return text;
}
}
