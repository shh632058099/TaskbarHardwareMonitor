#include "../src/config/Config.h"
#include "../src/monitor/CpuTemperature.h"
#include "../src/monitor/PawnIoTemperature.h"
#include "../src/monitor/SensorTypes.h"
#include "../src/monitor/SensorDemand.h"
#include "../src/ui/TaskbarLayout.h"
#include "../src/ipc/SharedSensorSnapshot.h"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char* expression) {
    if (!condition) {
        std::cerr << "FAIL: " << expression << '\n';
        ++failures;
    }
}

void TestTemperatureConversion() {
    double celsius = 0.0;
    Check(monitor::DecodeTemperatureTenthsKelvin(2981, celsius), "valid ACPI temperature");
    Check(std::abs(celsius - 24.95) < 0.01, "ACPI temperature conversion");
    Check(!monitor::DecodeTemperatureTenthsKelvin(0, celsius), "reject invalid ACPI temperature");
    Check(monitor::DecodeTemperatureCelsius(52, celsius), "valid Celsius temperature");
    Check(std::abs(celsius - 52.0) < 0.01, "Celsius temperature conversion");
    Check(!monitor::DecodeTemperatureCelsius(500, celsius), "reject invalid Celsius temperature");
}

void TestIntelMsrTemperature() {
    double celsius = 0.0;
    Check(monitor::DecodeIntelThermalStatus(0x80000000u | (48u << 16), 100, celsius), "valid Intel MSR temperature");
    Check(std::abs(celsius - 52.0) < 0.01, "Intel MSR temperature conversion");
    Check(!monitor::DecodeIntelThermalStatus(48u << 16, 100, celsius), "reject invalid Intel MSR status");
}

void TestIntelRaplPower() {
    const double unit = monitor::DecodeIntelRaplEnergyUnit(14ull << 8);
    Check(std::abs(unit - (1.0 / 16384.0)) < 1e-12,
          "Intel RAPL energy unit is decoded from MSR_RAPL_POWER_UNIT");

    double watts = 0.0;
    Check(monitor::ComputeIntelPackagePower(1000u, 165536u, unit, 1.0, watts),
          "Intel RAPL package power accepts a normal sample");
    Check(std::abs(watts - 10.04) < 0.05,
          "Intel RAPL package power converts energy delta to watts");

    const unsigned int previous = 0xFFFFFF00u;
    const unsigned int current = 0x00003F00u;
    Check(monitor::ComputeIntelPackagePower(previous, current, unit, 1.0, watts),
          "Intel RAPL package power handles 32-bit counter wraparound");
    Check(std::abs(watts - 1.0) < 0.01,
          "Intel RAPL wraparound delta is computed modulo 32 bits");
}

void TestTaskbarBandLayoutTracksValueWidth() {
    monitor::Config config;
    config.displayMode = monitor::DisplayMode::Full;
    config.taskbarRows = 1;
    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 52;
    snapshot.cpuTemperatureValid = true;
    snapshot.gpuTemperature = 46;
    snapshot.gpuTemperatureValid = true;
    snapshot.diskTemperature = 39;
    snapshot.diskTemperatureValid = true;
    snapshot.networkValid = true;
    snapshot.downloadBytesPerSecond = 12ULL * 1024 * 1024;
    snapshot.uploadBytesPerSecond = 1024 * 1024;
    snapshot.cpuPower = 32;
    snapshot.cpuPowerValid = true;
    const auto first = monitor::BuildTaskbarLayout(snapshot, config);

    snapshot.cpuTemperature = 8;
    snapshot.gpuTemperatureValid = false;
    snapshot.downloadBytesPerSecond = 999ULL * 1024 * 1024;
    const auto second = monitor::BuildTaskbarLayout(snapshot, config);
    Check(first.width != second.width, "full band width follows the actual value width");
    Check(first.cells.size() == second.cells.size(), "missing values preserve full fields");
    Check(first.cells[0].label == L"CPU" && second.cells[0].label == L"CPU",
          "full labels remain fixed");
    Check(second.cells[1].value == L"--\u00B0", "missing GPU uses a placeholder");
}

void TestTwoRowTaskbarLayout() {
    monitor::Config config;
    config.displayMode = monitor::DisplayMode::Full;
    config.taskbarRows = 2;
    const auto layout = monitor::BuildTaskbarLayout(monitor::SensorSnapshot{}, config);
    Check(layout.rows == 2, "two-row layout exposes two rows");
    Check(layout.cells[0].row == 0, "temperature metrics use first row");
    Check(layout.cells[3].row == 1, "network metrics use second row");
    Check(layout.width == std::max(layout.rowWidths[0], layout.rowWidths[1]),
          "two-row width is the widest row rather than both rows combined");
}

void TestCompactTaskbarBandLayout() {
    monitor::Config config;
    config.displayMode = monitor::DisplayMode::Compact;
    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 52;
    snapshot.cpuTemperatureValid = true;
    snapshot.networkValid = true;
    snapshot.downloadBytesPerSecond = 12ULL * 1024 * 1024;
    snapshot.uploadBytesPerSecond = 1024 * 1024;
    const auto layout = monitor::BuildTaskbarLayout(snapshot, config);
    Check(layout.cells[0].label == L"C", "compact CPU label is fixed");
    Check(layout.cells[0].value == L"52\u00B0", "compact temperature has compact unit");
    Check(layout.cells[3].value == L"12.0 MB/s", "compact network value includes the byte-per-second unit");
    Check(layout.width > 0, "compact layout has a fixed width");
}

void TestNetworkSpeedUsesBoundedUnits() {
    Check(monitor::FormatNetworkSpeed(9 * 1024) == L"9 KB/s", "network speed uses KB/s unit");
    Check(monitor::FormatNetworkSpeed(12ULL * 1024 * 1024) == L"12.0 MB/s",
          "network speed uses one decimal MB/s unit");
    Check(monitor::FormatNetworkSpeed(2ULL * 1024 * 1024 * 1024) == L"2.0 GB/s",
          "network speed uses GB/s unit");
}

void TestTemperatureDisplayFormatting() {
    Check(monitor::FormatTemperature(52.4, true) == L"52\u00B0",
          "valid temperatures are rounded consistently");
    Check(monitor::FormatTemperature(52.6, true) == L"53\u00B0",
          "temperature rounding is deterministic");
    Check(monitor::FormatTemperature(0.0, false) == L"--\u00B0",
          "invalid temperatures use a fixed placeholder");
}

void TestSharedDisplayConfiguration() {
    monitor::Config config;
    config.displayMode = monitor::DisplayMode::Compact;
    config.showGpuTemperature = false;
    config.showNetwork = false;
    monitor::SensorSnapshot snapshot;
    const auto shared = monitor::ToSharedSnapshot(snapshot, config);
    Check(shared.displayMode == 1, "shared snapshot carries compact mode");
    Check((shared.displayFlags & monitor::ShowGpuTemperature) == 0,
          "shared snapshot carries disabled GPU metric");
    Check((shared.displayFlags & monitor::ShowNetwork) == 0,
          "shared snapshot carries disabled network metric");
    Check((shared.displayFlags & monitor::ShowCpuTemperature) != 0,
          "shared snapshot carries enabled CPU metric");
}

void TestBandCommandCarriesDisplayState() {
    monitor::SharedBandCommand command{};
    command.action = monitor::BandCommandSetDisplay;
    command.displayMode = 1;
    command.displayFlags = monitor::ShowCpuTemperature | monitor::ShowPower;
    Check(command.version == 1, "band command has a version");
    Check(command.action == monitor::BandCommandSetDisplay,
          "band command identifies display updates");
    Check((command.displayFlags & monitor::ShowPower) != 0,
          "band command carries enabled metrics");
}

void TestTaskbarConfigurationDefaults() {
    const monitor::Config config;
    Check(config.taskbarEnabled, "taskbar is enabled by default");
    Check(config.displayMode == monitor::DisplayMode::Full,
          "taskbar uses full mode by default");
    Check(config.taskbarRows == 1, "taskbar uses one row by default");
    Check(config.taskbarFormat.empty(), "custom taskbar format is disabled by default");
    Check(config.storageDriveIndex == -1, "storage temperature source uses auto selection by default");
    Check(config.showCpuTemperature && config.showGpuTemperature &&
              config.showDiskTemperature && config.showNetwork && config.showPower,
          "all taskbar metrics are enabled by default");
}

void TestCustomTaskbarFormat() {
    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 52;
    snapshot.cpuTemperatureValid = true;
    snapshot.cpuUsage = 37;
    snapshot.cpuUsageValid = true;
    snapshot.networkValid = true;
    snapshot.downloadBytesPerSecond = 12ULL * 1024ULL * 1024ULL;
    snapshot.uploadBytesPerSecond = 2ULL * 1024ULL * 1024ULL;

    const auto layout = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"CPU:{cpu_temp} U:{cpu_usage}\\nD{down} U{up}");
    Check(layout.rows == 2, "custom taskbar format supports two rows");
    Check(layout.runs.size() == 8, "custom taskbar format splits literals and values");
    Check(layout.runs[1].value && layout.runs[1].text == L"52\u00B0",
          "custom CPU temperature variable is expanded");
    Check(layout.runs[3].value && layout.runs[3].text == L"37%",
          "custom CPU usage variable is expanded");
    Check(layout.runs[5].row == 1 && layout.runs[5].text == L"12.0 MB/s",
          "custom download variable uses second row");
    Check(layout.runs[1].stableText == L"100\u00B0",
          "custom format keeps stable width templates");
}

void TestCustomTaskbarColumns() {
    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 54.0;
    snapshot.cpuTemperatureValid = true;
    snapshot.cpuUsage = 6.0;
    snapshot.cpuUsageValid = true;
    snapshot.memoryUsage = 35.0;
    snapshot.memoryValid = true;
    snapshot.networkValid = true;
    snapshot.uploadBytesPerSecond = 3ULL * 1024ULL;
    snapshot.downloadBytesPerSecond = 6ULL * 1024ULL;
    snapshot.batteryPercent = 79.0;
    snapshot.batteryValid = true;
    snapshot.batteryState = monitor::BatteryOnAc;

    const auto layout = monitor::BuildFormattedTaskbarLayout(
        snapshot,
        L"温度:{cpu_temp}\\t占用:{cpu_usage}\\t内存:{ram_usage}\\n"
        L"上行:{up}\\t下行:{down}\\t电池:{battery}");
    Check(layout.rows == 2, "column format keeps two rows");
    Check(layout.columns == 3, "tab separators create three aligned columns");
    Check(layout.runs[0].column == 0, "first field stays in column zero");
    bool sawSecondColumn = false;
    bool sawThirdColumn = false;
    for (const auto& run : layout.runs) {
        if (run.column == 1) sawSecondColumn = true;
        if (run.column == 2) sawThirdColumn = true;
    }
    Check(sawSecondColumn && sawThirdColumn, "runs retain aligned column metadata");
}

void TestBatteryFormatting() {
    monitor::SensorSnapshot snapshot;
    snapshot.batteryPercent = 78.0;
    snapshot.batteryValid = true;
    snapshot.batteryState = monitor::BatteryCharging;
    Check(monitor::FormatBattery(snapshot, false) == L"78% CHG",
          "full battery format shows charging state");
    Check(monitor::FormatBattery(snapshot, true) == L"78%+",
          "compact battery format uses a short charging marker");
    Check(monitor::FormatBatteryStatus(snapshot.batteryState) == L"Charging",
          "battery status exposes charging text");

    snapshot.batteryPercent = 100.0;
    snapshot.batteryState = monitor::BatteryFull;
    Check(monitor::FormatBattery(snapshot, false) == L"100% FULL",
          "full battery format shows fully charged state");
    Check(monitor::FormatBatteryStatus(snapshot.batteryState) == L"Fully charged",
          "battery status exposes fully charged text");

    snapshot.batteryPercent = 80.0;
    snapshot.batteryState = monitor::BatteryOnAc;
    Check(monitor::FormatBattery(snapshot, false) == L"80% AC",
          "battery protection mode is shown as AC rather than full");

    const auto custom = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"{battery}|{battery_status}|{battery_percent}");
    Check(custom.runs.size() == 5, "battery custom variables are expanded");
    Check(custom.runs[0].text == L"80% AC", "battery variable includes status");
    Check(custom.runs[2].text == L"On AC", "battery_status variable is expanded");
}

void TestUnicodeUiText() {
    const wchar_t chinese[] = L"中文";
    Check(chinese[0] == 0x4E2D && chinese[1] == 0x6587,
          "UTF-8 source literals compile to the expected Unicode code points");

    monitor::SensorSnapshot snapshot;
    snapshot.cpuUsage = 42.0;
    snapshot.cpuUsageValid = true;
    const auto layout = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"处理器 {cpu_usage} 使用中");
    Check(layout.runs.size() == 3, "Chinese custom-format literals are preserved around variables");
    Check(layout.runs[0].text == L"处理器 ", "Chinese prefix text is preserved");
    Check(layout.runs[2].text == L" 使用中", "Chinese suffix text is preserved");
}

void TestSensorDemandSelection() {
    monitor::Config config;
    config.showCpuTemperature = false;
    config.showCpuUsage = false;
    config.showGpuTemperature = false;
    config.showDiskTemperature = false;
    config.showNetwork = false;
    config.showPower = false;
    config.showMemory = false;
    config.showGpuUsage = false;
    config.showVram = false;
    config.showDiskIo = false;
    config.showCpuClock = false;
    config.showGpuPower = false;
    config.showFan = false;
    config.showBattery = false;
    config.showSystemPower = false;

    Check(monitor::SensorDemandFromConfig(config) == 0,
          "no selected metrics means no sensor demand");

    config.showGpuUsage = true;
    config.showNetwork = true;
    auto demand = monitor::SensorDemandFromConfig(config);
    Check((demand & monitor::DemandGpuUsage) != 0 && (demand & monitor::DemandNetwork) != 0,
          "metric selections create matching sensor demand");
    Check((demand & monitor::DemandCpuTemperature) == 0,
          "unselected CPU temperature is not demanded");

    config.taskbarFormat = L"温度:{cpu_temp}\\t内存:{ram_usage}\\n电池:{battery_status}";
    demand = monitor::SensorDemandFromConfig(config);
    const std::uint32_t expected = monitor::DemandCpuTemperature |
                                   monitor::DemandMemory |
                                   monitor::DemandBattery;
    Check(demand == expected,
          "custom format overrides metric selections and only demands referenced sensors");

    monitor::Config synced;
    monitor::ApplySensorDemandToMetrics(synced, demand);
    Check(synced.showCpuTemperature && synced.showMemory && synced.showBattery,
          "format demand synchronizes matching metric checkboxes");
    Check(!synced.showGpuUsage && !synced.showNetwork && !synced.showPower,
          "format synchronization clears unrelated metrics");

    const auto aliases = monitor::SensorDemandFromFormat(
        L"{ssd_temp} {vram_used} {disk_write} {battery_percent}");
    Check((aliases & monitor::DemandDiskTemperature) != 0 &&
          (aliases & monitor::DemandVram) != 0 &&
          (aliases & monitor::DemandDiskIo) != 0 &&
          (aliases & monitor::DemandBattery) != 0,
          "format aliases map to their actual sensor groups");

    config.taskbarEnabled = false;
    Check(monitor::SensorDemandFromConfig(config) == 0,
          "disabled taskbar suspends all sensor collection");

    config.taskbarEnabled = true;
    config.taskbarFormat = L"仅显示固定文字";
    Check(monitor::SensorDemandFromConfig(config) == 0,
          "literal-only custom format performs no sensor collection");
}

void TestTaskbarWidthUsesOneLayoutDefinition() {
    monitor::Config config;
    config.displayMode = monitor::DisplayMode::Full;
    const auto layout = monitor::BuildTaskbarLayout(monitor::SensorSnapshot{}, config);
    Check(monitor::TaskbarWidth(config) == layout.width + 4,
          "band width is derived from the layout definition");

    config.displayMode = monitor::DisplayMode::Compact;
    config.showNetwork = false;
    config.showPower = false;
    const auto compactLayout = monitor::BuildTaskbarLayout(monitor::SensorSnapshot{}, config);
    Check(monitor::TaskbarWidth(config) == compactLayout.width + 4,
          "compact band width is derived from the layout definition");
}

} // namespace

int main() {
    TestTemperatureConversion();
    TestIntelMsrTemperature();
    TestIntelRaplPower();
    TestTaskbarBandLayoutTracksValueWidth();
    TestTwoRowTaskbarLayout();
    TestCompactTaskbarBandLayout();
    TestNetworkSpeedUsesBoundedUnits();
    TestTemperatureDisplayFormatting();
    TestSharedDisplayConfiguration();
    TestBandCommandCarriesDisplayState();
    TestTaskbarConfigurationDefaults();
    TestCustomTaskbarFormat();
    TestCustomTaskbarColumns();
    TestBatteryFormatting();
    TestUnicodeUiText();
    TestSensorDemandSelection();
    TestTaskbarWidthUsesOneLayoutDefinition();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
    }
    return failures == 0 ? 0 : 1;
}
