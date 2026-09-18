#pragma once

#include "../config/Config.h"
#include "../monitor/SensorTypes.h"

#include <string>
#include <cstdint>
#include <vector>

namespace monitor {
struct TaskbarCell {
    std::wstring label;
    std::wstring value;
    int width = 0;
    int row = 0;
};
struct TaskbarLayout {
    std::vector<TaskbarCell> cells;
    int width = 0;
    int rows = 1;
    int rowWidths[2]{};
};
struct TaskbarTextRun {
    std::wstring text;
    std::wstring stableText;
    bool value = false;
    int row = 0;
    int column = 0;
};
struct TaskbarTextLayout {
    std::vector<TaskbarTextRun> runs;
    int rows = 1;
    int columns = 1;
};
std::wstring FormatNetworkSpeed(std::uint64_t bytesPerSecond);
std::wstring FormatTemperature(double value, bool valid);
std::wstring FormatBattery(const SensorSnapshot&, bool compact = false);
std::wstring FormatBatteryStatus(std::uint32_t state);
TaskbarLayout BuildTaskbarLayout(const SensorSnapshot&, const Config&);
TaskbarTextLayout BuildFormattedTaskbarLayout(const SensorSnapshot&, const std::wstring& format);
int TaskbarWidth(const Config&);
}
