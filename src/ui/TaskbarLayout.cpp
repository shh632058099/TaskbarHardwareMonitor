#include "TaskbarLayout.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
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
        ? LayoutWidths{36, 40, 46, 42}
        : LayoutWidths{60, 54, 52, 60};
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
void Add(TaskbarLayout& layout, int row, const wchar_t* label, std::wstring value, int width) {
    row = row == 1 ? 1 : 0;
    layout.cells.push_back({label, std::move(value), width, row});
    layout.rowWidths[row] += width;
    layout.width = std::max(layout.rowWidths[0], layout.rowWidths[1]);
}
bool FormatVariable(const std::wstring& name, const SensorSnapshot& snapshot,
                    std::wstring& value, std::wstring& stable) {
    if (name == L"cpu_temp") {
        value = FormatTemperatureValue(snapshot.cpuTemperature, snapshot.cpuTemperatureValid);
        stable = L"100\u00B0";
    } else if (name == L"cpu_usage") {
        value = FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid);
        stable = L"100%";
    } else if (name == L"gpu_temp") {
        value = FormatTemperatureValue(snapshot.gpuTemperature, snapshot.gpuTemperatureValid);
        stable = L"100\u00B0";
    } else if (name == L"disk_temp" || name == L"ssd_temp") {
        value = FormatTemperatureValue(snapshot.diskTemperature, snapshot.diskTemperatureValid);
        stable = L"100\u00B0";
    } else if (name == L"down") {
        value = snapshot.networkValid ? FormatNetworkSpeed(snapshot.downloadBytesPerSecond) : L"--";
        stable = L"99.9M";
    } else if (name == L"up") {
        value = snapshot.networkValid ? FormatNetworkSpeed(snapshot.uploadBytesPerSecond) : L"--";
        stable = L"99.9M";
    } else if (name == L"power") {
        value = FormatPower(snapshot.cpuPower, snapshot.cpuPowerValid);
        stable = L"999W";
    } else if (name == L"ram_usage") {
        value = FormatUsage(snapshot.memoryUsage, snapshot.memoryValid);
        stable = L"100%";
    } else if (name == L"ram_used") {
        value = FormatBytesCompact(snapshot.memoryUsedBytes, snapshot.memoryValid);
        stable = L"99.9G";
    } else if (name == L"ram_total") {
        value = FormatBytesCompact(snapshot.memoryTotalBytes, snapshot.memoryValid);
        stable = L"99.9G";
    } else if (name == L"gpu_usage") {
        value = FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid);
        stable = L"100%";
    } else if (name == L"vram") {
        value = FormatVram(snapshot);
        stable = L"99.9/99.9G";
    } else if (name == L"vram_used") {
        value = FormatBytesCompact(snapshot.gpuMemoryUsedBytes, snapshot.gpuMemoryValid);
        stable = L"99.9G";
    } else if (name == L"vram_total") {
        value = FormatBytesCompact(snapshot.gpuMemoryTotalBytes, snapshot.gpuMemoryValid);
        stable = L"99.9G";
    } else if (name == L"disk_read") {
        value = snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskReadBytesPerSecond) : L"--";
        stable = L"99.9M";
    } else if (name == L"disk_write") {
        value = snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskWriteBytesPerSecond) : L"--";
        stable = L"99.9M";
    } else if (name == L"cpu_clock") {
        value = FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid);
        stable = L"9.9G";
    } else if (name == L"gpu_power") {
        value = FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid);
        stable = L"999W";
    } else if (name == L"fan") {
        value = FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid);
        stable = L"100%";
    } else if (name == L"battery") {
        value = FormatBattery(snapshot, false);
        stable = L"100% FULL";
    } else if (name == L"battery_percent") {
        value = FormatUsage(snapshot.batteryPercent, snapshot.batteryValid);
        stable = L"100%";
    } else if (name == L"battery_status") {
        value = snapshot.batteryValid ? FormatBatteryStatus(snapshot.batteryState) : L"Unknown";
        stable = L"Fully charged";
    } else if (name == L"system_power") {
        value = FormatPower(snapshot.systemPower, snapshot.systemPowerValid);
        stable = L"999W";
    } else {
        return false;
    }
    return true;
}

void AddTextRun(TaskbarTextLayout& layout, int row, int column, std::wstring text,
                std::wstring stableText, bool value) {
    if (text.empty() && stableText.empty()) return;
    layout.runs.push_back({std::move(text), std::move(stableText), value, row, column});
    layout.columns = std::max(layout.columns, column + 1);
}
}

std::wstring FormatNetworkSpeed(std::uint64_t bytesPerSecond) {
    std::wstringstream stream;
    const double value = static_cast<double>(bytesPerSecond);
    stream << std::fixed;
    if (bytesPerSecond >= 1024ULL * 1024ULL * 1024ULL) {
        stream << std::setprecision(1) << value / (1024.0 * 1024.0 * 1024.0) << L"G";
    } else if (bytesPerSecond >= 1024ULL * 1024ULL) {
        stream << std::setprecision(1) << value / (1024.0 * 1024.0) << L"M";
    } else {
        stream << std::setprecision(0) << value / 1024.0 << L"K";
    }
    auto result = stream.str();
    if (result.size() <= 5) return result;
    if (bytesPerSecond >= 1024ULL * 1024ULL) {
        std::wstringstream compact;
        compact << std::fixed << std::setprecision(1)
                << (bytesPerSecond >= 1024ULL * 1024ULL * 1024ULL
                    ? value / (1024.0 * 1024.0 * 1024.0)
                    : value / (1024.0 * 1024.0))
                << (bytesPerSecond >= 1024ULL * 1024ULL * 1024ULL ? L"G" : L"M");
        result = compact.str();
        if (result.size() > 5 && result.find(L".0") != std::wstring::npos) {
            result.erase(result.find(L".0"), 2);
        }
    }
    if (result.size() <= 5) return result;
    if (result.size() > 1 && result[result.size() - 2] == L'.') {
        result.erase(result.size() - 2, 2);
    }
    return result.size() > 5 ? result.substr(0, 5) : result;
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
                widths.temperature);
        }
        if (config.showCpuUsage) {
            Add(layout, temperaturesRow, L"U", FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid),
                widths.usage);
        }
        if (config.showGpuTemperature) {
            Add(layout, temperaturesRow, L"G", FormatTemperatureValue(snapshot.gpuTemperature,
                                                       snapshot.gpuTemperatureValid),
                widths.temperature);
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
        if (config.showMemory) Add(layout, temperaturesRow, L"R", FormatUsage(snapshot.memoryUsage, snapshot.memoryValid), widths.usage);
        if (config.showGpuUsage) Add(layout, temperaturesRow, L"GU", FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid), widths.usage);
        if (config.showVram) Add(layout, temperaturesRow, L"V", FormatVram(snapshot), widths.network * 2);
        if (config.showDiskIo) {
            Add(layout, secondaryRow, L"DR", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskReadBytesPerSecond) : L"--", widths.network);
            Add(layout, secondaryRow, L"DW", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskWriteBytesPerSecond) : L"--", widths.network);
        }
        if (config.showCpuClock) Add(layout, temperaturesRow, L"F", FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid), widths.network);
        if (config.showGpuPower) Add(layout, secondaryRow, L"GP", FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid), widths.power);
        if (config.showFan) Add(layout, secondaryRow, L"FAN", FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid), widths.usage);
        if (config.showBattery) Add(layout, secondaryRow, L"B", FormatBattery(snapshot, true), widths.usage);
        if (config.showSystemPower) Add(layout, secondaryRow, L"SYS", FormatPower(snapshot.systemPower, snapshot.systemPowerValid), widths.power);
    } else {
        if (config.showCpuTemperature) {
            Add(layout, temperaturesRow, L"CPU", FormatTemperatureValue(snapshot.cpuTemperature,
                                                         snapshot.cpuTemperatureValid),
                widths.temperature);
        }
        if (config.showCpuUsage) {
            Add(layout, temperaturesRow, L"LOAD", FormatUsage(snapshot.cpuUsage, snapshot.cpuUsageValid),
                widths.usage);
        }
        if (config.showGpuTemperature) {
            Add(layout, temperaturesRow, L"GPU", FormatTemperatureValue(snapshot.gpuTemperature,
                                                         snapshot.gpuTemperatureValid),
                widths.temperature);
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
        if (config.showMemory) Add(layout, temperaturesRow, L"RAM", FormatUsage(snapshot.memoryUsage, snapshot.memoryValid), widths.usage);
        if (config.showGpuUsage) Add(layout, temperaturesRow, L"GLOAD", FormatUsage(snapshot.gpuUsage, snapshot.gpuUsageValid), widths.usage);
        if (config.showVram) Add(layout, temperaturesRow, L"VRAM", FormatVram(snapshot), widths.network * 2);
        if (config.showDiskIo) {
            Add(layout, secondaryRow, L"READ", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskReadBytesPerSecond) : L"--", widths.network);
            Add(layout, secondaryRow, L"WRITE", snapshot.diskIoValid ? FormatNetworkSpeed(snapshot.diskWriteBytesPerSecond) : L"--", widths.network);
        }
        if (config.showCpuClock) Add(layout, temperaturesRow, L"CLK", FormatClock(snapshot.cpuClockMHz, snapshot.cpuClockValid), widths.network);
        if (config.showGpuPower) Add(layout, secondaryRow, L"GPWR", FormatPower(snapshot.gpuPower, snapshot.gpuPowerValid), widths.power);
        if (config.showFan) Add(layout, secondaryRow, L"FAN", FormatUsage(snapshot.gpuFanPercent, snapshot.gpuFanValid), widths.usage);
        if (config.showBattery) Add(layout, secondaryRow, L"BAT", FormatBattery(snapshot, false), widths.usage * 2);
        if (config.showSystemPower) Add(layout, secondaryRow, L"SYS", FormatPower(snapshot.systemPower, snapshot.systemPowerValid), widths.power);
    }
    return layout;
}

TaskbarTextLayout BuildFormattedTaskbarLayout(const SensorSnapshot& snapshot,
                                               const std::wstring& format) {
    TaskbarTextLayout layout;
    int row = 0;
    int column = 0;
    std::wstring literal;
    auto flushLiteral = [&]() {
        if (literal.empty()) return;
        AddTextRun(layout, row, column, literal, literal, false);
        literal.clear();
    };

    for (std::size_t index = 0; index < format.size();) {
        const bool escapedNewline = format[index] == L'\\' && index + 1 < format.size() &&
                                    format[index + 1] == L'n';
        const bool escapedTab = format[index] == L'\\' && index + 1 < format.size() &&
                                format[index + 1] == L't';
        const bool realNewline = format[index] == L'\n' || format[index] == L'\r';
        const bool realTab = format[index] == L'\t';
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
                if (format[index] == L'\r' && index + 1 < format.size() && format[index + 1] == L'\n') ++index;
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

        if (format[index] == L'{') {
            const auto end = format.find(L'}', index + 1);
            if (end != std::wstring::npos) {
                const auto name = format.substr(index + 1, end - index - 1);
                std::wstring value;
                std::wstring stable;
                if (FormatVariable(name, snapshot, value, stable)) {
                    flushLiteral();
                    AddTextRun(layout, row, column, std::move(value), std::move(stable), true);
                    index = end + 1;
                    continue;
                }
            }
        }

        literal.push_back(format[index]);
        ++index;
    }
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
}
