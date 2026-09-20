#pragma once
#include "SensorTypes.h"
#include <windows.h>
#include "PawnIoTemperature.h"
#include "PowerProvider.h"
#include "WmiTemperatureProvider.h"
#include "../hardware/CpuVendor.h"
#include "../hardware/HardwareAccess.h"
#include "../hardware/amd/AmdTemperature.h"

#include <memory>

namespace monitor {
class CpuMonitor {
public:
    CpuMonitor();
    ~CpuMonitor();
    double UpdateUsage(bool& valid);
    double ReadTemperature(bool& valid, bool forceWmiRetry = false);
    double ReadPackagePower(bool& valid);
    void ResetUsage() { initialized_ = false; }
    void ResetPowerSampling();
private:
    ULONGLONG idle_ = 0, kernel_ = 0, user_ = 0;
    bool initialized_ = false;
    ULONGLONG temperatureSuccessTick_ = 0;
    double temperature_ = 0.0;
    bool temperatureValid_ = false;
    double power_ = 0.0;
    bool powerValid_ = false;
    ULONGLONG powerSuccessTick_ = 0;
    PawnIoTemperature pawnIo_{};
    WmiTemperatureProvider wmi_{};
    class PawnIoPowerProvider;
    std::unique_ptr<PawnIoPowerProvider> pawnIoPower_;
    std::unique_ptr<PowerManager> powerManager_;
    CpuVendor vendor_ = DetectCpuVendor();
    UnavailableHardwareAccess unavailableHardwareAccess_{};
    AmdTemperatureProvider amdTemperature_;
};
}
