#pragma once
#include "SensorTypes.h"
#include <windows.h>
#include "PawnIoTemperature.h"

namespace monitor {
class CpuMonitor {
public:
    double UpdateUsage(bool& valid);
    double ReadTemperature(bool& valid);
    double ReadPackagePower(bool& valid);
    void ResetUsage() { initialized_ = false; }
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
};
}
