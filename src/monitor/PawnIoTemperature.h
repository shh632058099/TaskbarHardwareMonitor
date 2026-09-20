#pragma once

#include <windows.h>
#include <string>

namespace monitor {

class PawnIoTemperature {
public:
    ~PawnIoTemperature();

    bool ReadPackageTemperature(double& celsius);
    bool ReadPackagePower(double& watts);
    void ResetPowerSampling() { powerInitialized_ = false; lastPowerTick_ = 0; }

private:
    bool InstallPawnIo();
    bool LoadIntelMsrModule();
    bool Initialize();
    bool IsPawnIoInstalled() const;
    bool OpenDevice();
    bool ExecuteReadMsr(unsigned int index, unsigned long long& value);
    bool ReadTemperatureRegister(unsigned int registerIndex, double& celsius);
    bool ReadResource(int resourceId, const wchar_t* fileName, std::wstring& path);
    void Log(const wchar_t* message) const;

    HANDLE device_ = INVALID_HANDLE_VALUE;
    bool moduleLoaded_ = false;
    bool installationAttempted_ = false;
    bool unavailable_ = false;
    double raplEnergyUnit_ = 0.0;
    unsigned int lastPackageEnergy_ = 0;
    ULONGLONG lastPowerTick_ = 0;
    bool powerInitialized_ = false;
    ULONGLONG nextRetryTick_ = 0;
};

bool DecodeIntelThermalStatus(unsigned int status, unsigned int tjMax, double& celsius);
double DecodeIntelRaplEnergyUnit(unsigned long long raplPowerUnitMsr);
bool ComputeIntelPackagePower(unsigned int previousEnergy, unsigned int currentEnergy,
                              double energyUnitJoules, double elapsedSeconds, double& watts);

} // namespace monitor
