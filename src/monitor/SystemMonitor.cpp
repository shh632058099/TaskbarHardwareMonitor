#include "SystemMonitor.h"

#include <windows.h>
#include <powrprof.h>

#include <algorithm>
#include <vector>

namespace monitor {
namespace {
struct ProcessorPowerInformation {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
};
} // namespace


void SystemMonitor::UpdateMemory(SensorSnapshot& snapshot) const {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory) || memory.ullTotalPhys == 0) {
        return;
    }

    snapshot.memoryTotalBytes = memory.ullTotalPhys;
    snapshot.memoryUsedBytes = memory.ullTotalPhys - memory.ullAvailPhys;
    snapshot.memoryUsage = 100.0 * static_cast<double>(snapshot.memoryUsedBytes) /
                           static_cast<double>(snapshot.memoryTotalBytes);
    snapshot.memoryValid = true;
}

void SystemMonitor::UpdateCpuClock(SensorSnapshot& snapshot) const {
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (system.dwNumberOfProcessors == 0) {
        return;
    }

    std::vector<ProcessorPowerInformation> processors(system.dwNumberOfProcessors);
    const ULONG bytes = static_cast<ULONG>(processors.size() * sizeof(ProcessorPowerInformation));
    if (CallNtPowerInformation(ProcessorInformation, nullptr, 0,
                               processors.data(), bytes) != 0) {
        return;
    }

    unsigned long long sum = 0;
    unsigned int count = 0;
    for (const auto& processor : processors) {
        if (processor.CurrentMhz == 0) continue;
        sum += processor.CurrentMhz;
        ++count;
    }
    if (count == 0) return;

    snapshot.cpuClockMHz = static_cast<double>(sum) / static_cast<double>(count);
    snapshot.cpuClockValid = true;
}

void SystemMonitor::UpdateBattery(SensorSnapshot& snapshot, bool collectSystemPower) const {
    SYSTEM_POWER_STATUS status{};
    if (GetSystemPowerStatus(&status) && status.BatteryFlag != 128 &&
        status.BatteryLifePercent != 255) {
        snapshot.batteryPercent = static_cast<double>(status.BatteryLifePercent);
        snapshot.batteryValid = true;
        if (status.BatteryFlag & 8) snapshot.batteryState = BatteryCharging;
        else if (status.ACLineStatus == 1 && status.BatteryLifePercent >= 99)
            snapshot.batteryState = BatteryFull;
        else if (status.ACLineStatus == 1) snapshot.batteryState = BatteryOnAc;
        else snapshot.batteryState = BatteryDischarging;
    }

    if (!collectSystemPower) return;

    SYSTEM_BATTERY_STATE battery{};
    if (CallNtPowerInformation(SystemBatteryState, nullptr, 0,
                               &battery, sizeof(battery)) != 0 || !battery.BatteryPresent) {
        return;
    }

    if (battery.Charging) {
        snapshot.batteryState = BatteryCharging;
    } else if (battery.Discharging) {
        snapshot.batteryState = BatteryDischarging;
    } else if (battery.AcOnLine) {
        const bool capacityFull = battery.MaxCapacity > 0 &&
            battery.RemainingCapacity >= (battery.MaxCapacity * 99ULL) / 100ULL;
        const bool percentFull = snapshot.batteryValid && snapshot.batteryPercent >= 99.0;
        snapshot.batteryState = (capacityFull || percentFull) ? BatteryFull : BatteryOnAc;
    }

    // SYSTEM_BATTERY_STATE::Rate is meaningful as whole-system discharge power
    // while running from the battery. Do not invent a value while on AC power.
    if (battery.Discharging && battery.Rate > 0 && battery.Rate < 1000000u) {
        snapshot.systemPower = static_cast<double>(battery.Rate) / 1000.0;
        snapshot.systemPowerValid = true;
    }
}

} // namespace monitor
