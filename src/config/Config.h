#pragma once
#include <string>

namespace monitor {
enum class DisplayMode { Full, Compact };
struct Config {
    int refreshIntervalMs = 1000;
    bool startWithWindows = false;
    std::wstring networkAdapter = L"auto";
    int storageDriveIndex = -1;
    bool taskbarEnabled = true;
    DisplayMode displayMode = DisplayMode::Full;
    int taskbarRows = 1;
    bool taskbarValueColorCustom = false;
    unsigned int taskbarValueColor = 0x00F5F5F5u;
    bool thresholdColorsEnabled = false;
    int cpuTempWarning = 75;
    int cpuTempCritical = 90;
    int gpuTempWarning = 75;
    int gpuTempCritical = 90;
    int ramWarning = 85;
    int ramCritical = 95;
    int batteryWarning = 20;
    int batteryCritical = 10;
    unsigned int warningColor = 0x0000BEFFu;
    unsigned int criticalColor = 0x005050FFu;
    std::wstring taskbarFontName = L"Segoe UI";
    int taskbarFontSize = 11;
    int taskbarFontWeight = 400;
    std::wstring taskbarFormat;
    bool showCpuTemperature = true;
    bool showCpuUsage = false;
    bool showGpuTemperature = true;
    bool showDiskTemperature = true;
    bool showNetwork = true;
    bool showPower = true;
    bool showMemory = false;
    bool showGpuUsage = false;
    bool showVram = false;
    bool showDiskIo = false;
    bool showCpuClock = false;
    bool showGpuPower = false;
    bool showFan = false;
    bool showBattery = false;
    bool showSystemPower = false;
    bool showCpuInternalTemperature = false;
    bool showGpuInternalTemperature = false;
    bool showCpuFanRpm = false;
    bool showGpuFanRpm = false;
    bool fixedWidth = true;
    bool tabularNumbers = true;
    std::wstring path;
    bool Load();
    bool SaveFile() const;
    bool Save() const;
    bool ApplyStartupSetting() const;
};
}
