#include "../src/config/Config.h"
#include "../src/monitor/CpuTemperature.h"
#include "../src/monitor/PawnIoTemperature.h"
#include "../src/monitor/TemperatureProvider.h"
#include "../src/monitor/PowerProvider.h"
#include "../src/hardware/amd/AmdTemperature.h"
#include "../src/hardware/SensorRegistry.h"
#include "../src/monitor/SnapshotSensorProvider.h"
#include "../src/monitor/WmiTemperatureProvider.h"
#include "../src/monitor/SensorTypes.h"
#include "../src/monitor/SensorDemand.h"
#include "../src/ui/TaskbarLayout.h"
#include "../src/ipc/SharedSensorSnapshot.h"
#include "../src/app/App.h"

#include <cmath>
#include <iostream>
#include <windows.h>

namespace {

int failures = 0;

void Check(bool condition, const char* expression) {
    if (!condition) {
        std::cerr << "FAIL: " << expression << '\n';
        ++failures;
    }
}

class FakeTemperatureProvider final : public monitor::ITemperatureProvider {
public:
    FakeTemperatureProvider(bool succeeds, double celsius)
        : succeeds_(succeeds), celsius_(celsius) {}

    bool ReadPackageTemperature(double& celsius) override {
        ++calls;
        if (!succeeds_) return false;
        celsius = celsius_;
        return true;
    }

    int calls = 0;

private:
    bool succeeds_;
    double celsius_;
};

class FakePowerProvider final : public monitor::IPowerProvider {
public:
    bool ReadPackagePower(std::uint64_t nowMilliseconds, double& watts) override {
        lastNowMilliseconds = nowMilliseconds;
        watts = 42.5;
        return true;
    }

    void Reset() override { reset = true; }

    std::uint64_t lastNowMilliseconds = 0;
    bool reset = false;
};

void TestProviderFallbacks() {
    double celsius = 0.0;
    FakeTemperatureProvider primary(true, 51.5);
    FakeTemperatureProvider fallback(true, 47.0);
    monitor::TemperatureManager temperature(primary, fallback);
    Check(temperature.ReadPackageTemperature(celsius) && std::abs(celsius - 51.5) < 0.01,
          "temperature manager returns the primary provider result");
    Check(primary.calls == 1 && fallback.calls == 0,
          "temperature manager does not call fallback after a primary success");

    FakeTemperatureProvider unavailable(false, 0.0);
    monitor::TemperatureManager fallbackTemperature(unavailable, fallback);
    Check(fallbackTemperature.ReadPackageTemperature(celsius) && std::abs(celsius - 47.0) < 0.01,
          "temperature manager returns fallback after primary failure");

    FakePowerProvider powerProvider;
    monitor::PowerManager power(powerProvider);
    double watts = 0.0;
    Check(power.ReadPackagePower(1234, watts) && std::abs(watts - 42.5) < 0.01,
          "power manager returns the configured provider result");
    Check(powerProvider.lastNowMilliseconds == 1234,
          "power manager forwards the collection timestamp");
    power.Reset();
    Check(powerProvider.reset, "power manager resets its provider");
}

class FakeHardwareAccess final : public monitor::IHardwareAccess {
public:
    bool ReadPciConfig(std::uint32_t, void* buffer, std::size_t size) override {
        if (!pciReadable || size != sizeof(rawPciValue)) return false;
        std::memcpy(buffer, &rawPciValue, size);
        return true;
    }

    bool ReadMsr(std::uint32_t, std::uint32_t, std::uint64_t&) override { return false; }

    bool pciReadable = true;
    std::uint32_t rawPciValue = 0;
};

void TestAmdTemperatureProviderSafety() {
    double celsius = 0.0;
    Check(monitor::IsSupportedAmdZenIdentity({0x17, 0x31, 0}),
          "AMD provider recognizes its explicitly supported family");
    Check(!monitor::IsSupportedAmdZenIdentity({0x19, 0x61, 0}),
          "AMD provider rejects unverified CPU families");
    Check(monitor::DecodeAmdZenTemperature(0x40000000u, 0.0, celsius) &&
              std::abs(celsius - 64.0) < 0.01,
          "AMD provider decodes the documented fixed-point temperature field");

    FakeHardwareAccess access;
    const monitor::AmdCpuIdentity identity{0x17, 0x31, 0};
    monitor::AmdTemperatureProvider emptyMap(access, identity, {});
    Check(!emptyMap.ReadPackageTemperature(celsius),
          "AMD provider refuses direct reads without an explicit register map");

    monitor::AmdTemperatureRegisterMap map{};
    map.bus = 0;
    map.device = 0x18;
    map.function = 3;
    map.offset = 0xA4;
    map.width = 4;
    access.rawPciValue = 0x40000000u;
    monitor::AmdTemperatureProvider mapped(access, identity, map);
    Check(mapped.ReadPackageTemperature(celsius) && std::abs(celsius - 64.0) < 0.01,
          "AMD provider reads only the explicitly supplied PCI map");
}

void TestGenericSensorRegistry() {
    monitor::SensorRegistry registry;
    registry.Upsert({L"cpu.package.temperature", L"CPU", monitor::SensorType::Temperature,
                     50.0, true, 1});
    registry.Upsert({L"cpu.package.temperature", L"CPU", monitor::SensorType::Temperature,
                     55.0, true, 2});
    const auto* cpu = registry.Find(L"cpu.package.temperature");
    Check(cpu && std::abs(cpu->value - 55.0) < 0.01 && cpu->timestamp == 2,
          "sensor registry replaces an older value with the current value");
    registry.Clear();
    Check(registry.Find(L"cpu.package.temperature") == nullptr,
          "sensor registry clears readings that are no longer collected");

    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 60.0;
    snapshot.cpuTemperatureValid = true;
    snapshot.cpuClockMHz = 3600.0;
    snapshot.cpuClockValid = true;
    snapshot.memoryUsedBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    snapshot.memoryValid = true;
    snapshot.downloadBytesPerSecond = 1024;
    snapshot.networkValid = true;
    snapshot.diskTemperature = 40.0;
    snapshot.diskTemperatureValid = true;
    snapshot.diskReadBytesPerSecond = 2048;
    snapshot.diskWriteBytesPerSecond = 1024;
    snapshot.diskIoValid = true;
    snapshot.gpuTemperature = 50.0;
    snapshot.gpuTemperatureValid = true;
    snapshot.gpuUsage = 70.0;
    snapshot.gpuUsageValid = true;
    snapshot.gpuMemoryUsedBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    snapshot.gpuMemoryTotalBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    snapshot.gpuMemoryValid = true;
    snapshot.gpuPower = 125.0;
    snapshot.gpuPowerValid = true;
    snapshot.gpuFanPercent = 45.0;
    snapshot.gpuFanValid = true;
    snapshot.batteryPercent = 75.0;
    snapshot.batteryValid = true;
    snapshot.batteryState = monitor::BatteryCharging;
    snapshot.systemPower = 20.0;
    snapshot.systemPowerValid = true;
    monitor::SensorCollection sensors;
    monitor::AddSnapshotSensors(snapshot, monitor::DemandCpuTemperature | monitor::DemandMemory |
                                 monitor::DemandNetwork | monitor::DemandDiskTemperature |
                                 monitor::DemandDiskIo | monitor::DemandCpuClock |
                                 monitor::DemandGpuTemperature | monitor::DemandGpuUsage |
                                 monitor::DemandVram | monitor::DemandGpuPower | monitor::DemandFan |
                                 monitor::DemandBattery | monitor::DemandSystemPower,
                                 123, sensors);
    Check(sensors.size() == 18, "snapshot provider maps every demanded legacy reading into generic sensors");
    Check(sensors[0].identifier == L"cpu.package.temperature" && sensors[0].valid &&
              sensors[0].timestamp == 123,
          "snapshot provider preserves CPU reading validity and collection time");
    bool sawStorageRead = false;
    bool sawStorageWrite = false;
    bool sawCpuClock = false;
    bool sawGpuFan = false;
    bool sawBattery = false;
    bool sawSystemPower = false;
    for (const auto& sensor : sensors) {
        sawStorageRead = sawStorageRead || sensor.identifier == L"storage.read";
        sawStorageWrite = sawStorageWrite || sensor.identifier == L"storage.write";
        sawCpuClock = sawCpuClock || sensor.identifier == L"cpu.clock";
        sawGpuFan = sawGpuFan || sensor.identifier == L"gpu.fan";
        sawBattery = sawBattery || sensor.identifier == L"battery.percent";
        sawSystemPower = sawSystemPower || sensor.identifier == L"system.power";
    }
    Check(sawStorageRead && sawStorageWrite,
          "snapshot provider maps disk I/O into the generic sensor collection");
    Check(sawCpuClock && sawGpuFan && sawBattery && sawSystemPower,
          "snapshot provider includes clock, GPU, battery, and system-power readings");
}

void TestWmiRetryDeadline() {
    Check(monitor::WmiRetryDelayMilliseconds(1000, 0) == 0,
          "WMI retries immediately before any failure");
    Check(monitor::WmiRetryDelayMilliseconds(1000, 31000) == 30000,
          "WMI failed-probe retry waits for the configured backoff");
    Check(monitor::WmiRetryDelayMilliseconds(30999, 31000) == 1,
          "WMI retry delay counts down to the retry deadline");
    Check(monitor::WmiRetryDelayMilliseconds(31000, 31000) == 0,
          "WMI retry is allowed at the retry deadline");
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

void TestSharedSnapshotFreshness() {
    monitor::SharedSensorSnapshot snapshot{};
    snapshot.timestamp = 10000;
    Check(monitor::IsSharedSnapshotFresh(snapshot, 10000),
          "fresh shared snapshot is accepted");
    Check(monitor::IsSharedSnapshotFresh(
              snapshot, 10000 + monitor::SnapshotStaleTimeoutMs),
          "snapshot remains valid at the stale timeout boundary");
    Check(!monitor::IsSharedSnapshotFresh(
              snapshot, 10001 + monitor::SnapshotStaleTimeoutMs),
          "snapshot becomes stale after the timeout");
    Check(!monitor::IsSharedSnapshotFresh(snapshot, 9999),
          "future snapshot timestamp is rejected");
    Check(monitor::SnapshotStaleTimeoutMs > monitor::SnapshotHeartbeatIntervalMs * 2,
          "stale timeout allows multiple missed heartbeats");
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
    Check(!config.thresholdColorsEnabled, "threshold alert colors are disabled by default");
    Check(config.storageDriveIndex == -1, "storage temperature source uses auto selection by default");
    Check(config.showCpuTemperature && config.showGpuTemperature &&
              config.showDiskTemperature && config.showNetwork && config.showPower,
          "all taskbar metrics are enabled by default");
}

void TestConfigFileRoundTrip() {
    wchar_t directory[MAX_PATH]{};
    const DWORD length = GetTempPathW(_countof(directory), directory);
    Check(length > 0 && length < _countof(directory), "temporary directory is available");
    if (length == 0 || length >= _countof(directory)) return;

    const std::wstring path = std::wstring(directory) + L"TaskbarHardwareMonitor-config-test.json";
    DeleteFileW(path.c_str());
    DeleteFileW((path + L".tmp").c_str());

    monitor::Config written;
    written.path = path;
    written.refreshIntervalMs = 750;
    written.taskbarFormat = L"CPU:{cpu_temp}";
    Check(written.SaveFile(), "configuration file is written atomically");

    monitor::Config loaded;
    loaded.path = path;
    Check(loaded.Load(), "configuration file reloads after atomic write");
    Check(loaded.refreshIntervalMs == 750, "configuration round trip retains collection interval");
    Check(loaded.taskbarFormat == L"CPU:{cpu_temp}", "configuration round trip retains format");

    written.refreshIntervalMs = 1000;
    Check(written.SaveFile(), "configuration file atomically replaces an existing file");
    monitor::Config replaced;
    replaced.path = path;
    Check(replaced.Load() && replaced.refreshIntervalMs == 1000,
          "atomic replacement makes the new complete configuration visible");

    DeleteFileW(path.c_str());
    DeleteFileW((path + L".tmp").c_str());
}

void TestDeferredConfigSaveDeadline() {
    Check(monitor::MillisecondsUntilConfigSave(500, 1000, true) == 500,
          "pending configuration save waits until debounce deadline");
    Check(monitor::MillisecondsUntilConfigSave(1000, 1000, true) == 0,
          "configuration save is due at debounce deadline");
    Check(monitor::MillisecondsUntilConfigSave(1001, 1000, true) == 0,
          "configuration save remains due after debounce deadline");
    Check(monitor::MillisecondsUntilConfigSave(500, 1000, false) == INFINITE,
          "non-pending configuration save does not add a wake deadline");
}

void TestDiagnosticsSecondSampleDeadline() {
    Check(monitor::MillisecondsUntilConfigSave(500, 1000, true) == 500,
          "diagnostic second sample can use the worker deadline helper");
    Check(monitor::DiagnosticsSecondSampleDelayMs >= 250,
          "diagnostic disk I/O second sample allows measurable elapsed time");
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

void TestDisplayFormat2ModifiersAndConditions() {
    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperature = 54.26;
    snapshot.cpuTemperatureValid = true;
    snapshot.cpuClockMHz = 3400.0;
    snapshot.cpuClockValid = true;
    snapshot.memoryUsedBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
    snapshot.memoryTotalBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    snapshot.memoryUsage = 75.0;
    snapshot.memoryValid = true;
    snapshot.downloadBytesPerSecond = 12ULL * 1024ULL * 1024ULL;
    snapshot.networkValid = true;

    const auto modifiers = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"{cpu_temp:1}|{cpu_clock:ghz}|{ram_used:gb}|{down:mb}");
    Check(modifiers.runs.size() == 7, "format 2.0 modifiers preserve separators");
    Check(modifiers.runs[0].text == L"54.3\u00B0", "temperature modifier keeps one decimal");
    Check(modifiers.runs[2].text == L"3.4 GHz", "clock ghz modifier formats units");
    Check(modifiers.runs[4].text == L"12.0 GB", "memory gb modifier formats units");
    Check(modifiers.runs[6].text == L"12.0 MB/s", "network mb modifier formats units");

    snapshot.downloadBytesPerSecond = 182ULL * 1024ULL;
    snapshot.batteryPercent = 79.0;
    snapshot.batteryValid = true;
    snapshot.batteryState = monitor::BatteryOnAc;
    const auto compact = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"{down:short}|{battery:short}");
    Check(compact.runs.size() == 3, "short format modifiers preserve separators");
    Check(compact.runs[0].text == L"182K", "network short modifier removes verbose rate suffix");
    Check(compact.runs[0].stableText == L"1023M", "network short modifier keeps bounded stable width");
    Check(compact.runs[2].text == L"79%A", "battery short modifier uses compact AC state");

    const auto hidden = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"A{gpu_temp? GPU:{gpu_temp}}B");
    std::wstring hiddenText;
    for (const auto& run : hidden.runs) hiddenText += run.text;
    Check(hiddenText == L"AB", "conditional segment is hidden when sensor data is unavailable");

    snapshot.gpuTemperature = 47.0;
    snapshot.gpuTemperatureValid = true;
    const auto shown = monitor::BuildFormattedTaskbarLayout(
        snapshot, L"A{gpu_temp? GPU:{gpu_temp}}B");
    std::wstring shownText;
    for (const auto& run : shown.runs) shownText += run.text;
    Check(shownText == L"A GPU:47\u00B0B", "conditional segment renders nested variables when available");

    const auto demand = monitor::SensorDemandFromFormat(
        L"{gpu_temp?GPU:{gpu_temp}} {down:mb}");
    Check((demand & monitor::DemandGpuTemperature) != 0 &&
          (demand & monitor::DemandNetwork) != 0,
          "format 2.0 conditions and modifiers keep sensor demand accurate");
}

void TestTaskbarFormatValidation() {
    Check(monitor::ValidateTaskbarFormat(L"CPU:{cpu_temp}").empty(),
          "known display format is valid");
    Check(monitor::ValidateTaskbarFormat(L"{cpu_temp") == L"Missing closing }",
          "format reports unmatched opening brace");
    Check(monitor::ValidateTaskbarFormat(L"cpu_temp}") == L"Unexpected }",
          "format reports unmatched closing brace");
    Check(monitor::ValidateTaskbarFormat(L"{gpu_temp?}") == L"Conditional section is empty",
          "format reports empty conditional section");
    Check(monitor::ValidateTaskbarFormat(L"{not_a_metric}") == L"Unknown variable: not_a_metric",
          "format reports unknown variable");
    Check(monitor::ValidateTaskbarFormat(L"{cpu_temp:mb}") == L"Invalid modifier: mb",
          "format reports incompatible variable modifier");
    Check(monitor::ValidateTaskbarFormat(L"{gpu_temp?GPU:{gpu_temp}}").empty(),
          "format accepts nested conditional variable");
}

void TestDiagnosticsText() {
    monitor::SensorSnapshot unavailable;
    Check(monitor::BuildDiagnosticsText(unavailable).find(L"CPU temperature: Unavailable") != std::wstring::npos,
          "diagnostics reports unavailable CPU temperature");
    Check(monitor::BuildDiagnosticsText(unavailable).find(
              L"Disk I/O: Unavailable (needs a second sample, or disk performance counters are unavailable)") !=
              std::wstring::npos,
          "diagnostics explains disk I/O requires two samples or supported counters");

    monitor::SensorSnapshot available;
    available.cpuTemperatureValid = true;
    available.memoryValid = true;
    available.networkValid = true;
    const auto text = monitor::BuildDiagnosticsText(available);
    Check(text.find(L"CPU temperature: Available") != std::wstring::npos,
          "diagnostics reports available CPU temperature");
    Check(text.find(L"Memory: Available") != std::wstring::npos,
          "diagnostics reports available memory");
    Check(text.find(L"Network: Available") != std::wstring::npos,
          "diagnostics reports available network");
}

void TestThresholdAlertSeverity() {
    monitor::Config config;
    config.thresholdColorsEnabled = true;
    config.cpuTempWarning = 75;
    config.cpuTempCritical = 90;
    config.gpuTempWarning = 75;
    config.gpuTempCritical = 90;
    config.ramWarning = 85;
    config.ramCritical = 95;
    config.batteryWarning = 20;
    config.batteryCritical = 10;

    monitor::SensorSnapshot snapshot;
    snapshot.cpuTemperatureValid = true;
    snapshot.cpuTemperature = 80.0;
    Check(monitor::AlertSeverityForVariable(L"cpu_temp", snapshot, config) ==
              monitor::AlertSeverity::Warning,
          "CPU temperature enters warning severity");
    snapshot.cpuTemperature = 95.0;
    Check(monitor::AlertSeverityForVariable(L"cpu_temp:1", snapshot, config) ==
              monitor::AlertSeverity::Critical,
          "threshold severity ignores format modifier and reaches critical");

    snapshot.memoryValid = true;
    snapshot.memoryUsage = 90.0;
    Check(monitor::AlertSeverityForVariable(L"ram_used:gb", snapshot, config) ==
              monitor::AlertSeverity::Warning,
          "RAM byte variables use RAM usage threshold");

    snapshot.batteryValid = true;
    snapshot.batteryPercent = 8.0;
    Check(monitor::AlertSeverityForVariable(L"battery", snapshot, config) ==
              monitor::AlertSeverity::Critical,
          "low battery uses reversed critical threshold");

    const auto formatted = monitor::BuildFormattedTaskbarLayout(snapshot, L"{battery}", &config);
    Check(!formatted.runs.empty() && formatted.runs[0].severity == monitor::AlertSeverity::Critical,
          "custom format carries threshold severity into text runs");

    config.thresholdColorsEnabled = false;
    Check(monitor::AlertSeverityForVariable(L"battery", snapshot, config) ==
              monitor::AlertSeverity::Normal,
          "disabled threshold colors preserve normal severity");
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
    TestProviderFallbacks();
    TestAmdTemperatureProviderSafety();
    TestGenericSensorRegistry();
    TestWmiRetryDeadline();
    TestTemperatureConversion();
    TestIntelMsrTemperature();
    TestIntelRaplPower();
    TestTaskbarBandLayoutTracksValueWidth();
    TestTwoRowTaskbarLayout();
    TestCompactTaskbarBandLayout();
    TestNetworkSpeedUsesBoundedUnits();
    TestTemperatureDisplayFormatting();
    TestSharedSnapshotFreshness();
    TestSharedDisplayConfiguration();
    TestBandCommandCarriesDisplayState();
    TestTaskbarConfigurationDefaults();
    TestConfigFileRoundTrip();
    TestDeferredConfigSaveDeadline();
    TestDiagnosticsSecondSampleDeadline();
    TestCustomTaskbarFormat();
    TestCustomTaskbarColumns();
    TestDisplayFormat2ModifiersAndConditions();
    TestTaskbarFormatValidation();
    TestDiagnosticsText();
    TestThresholdAlertSeverity();
    TestBatteryFormatting();
    TestUnicodeUiText();
    TestSensorDemandSelection();
    TestTaskbarWidthUsesOneLayoutDefinition();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
    }
    return failures == 0 ? 0 : 1;
}
