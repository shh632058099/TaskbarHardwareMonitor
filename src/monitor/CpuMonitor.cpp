#include "CpuMonitor.h"

namespace monitor {
namespace {

class PawnIoTemperatureProvider final : public ITemperatureProvider {
public:
    explicit PawnIoTemperatureProvider(PawnIoTemperature& pawnIo) : pawnIo_(pawnIo) {}
    bool ReadPackageTemperature(double& celsius) override {
        return pawnIo_.ReadPackageTemperature(celsius);
    }
private:
    PawnIoTemperature& pawnIo_;
};

} // namespace

class CpuMonitor::PawnIoPowerProvider final : public IPowerProvider {
public:
    explicit PawnIoPowerProvider(PawnIoTemperature& pawnIo) : pawnIo_(pawnIo) {}
    bool ReadPackagePower(std::uint64_t, double& watts) override {
        return pawnIo_.ReadPackagePower(watts);
    }
    void Reset() override { pawnIo_.ResetPowerSampling(); }
private:
    PawnIoTemperature& pawnIo_;
};

CpuMonitor::CpuMonitor()
    : amdTemperature_(unavailableHardwareAccess_, DetectAmdCpuIdentity(), {}),
      pawnIoPower_(std::make_unique<PawnIoPowerProvider>(pawnIo_)),
      powerManager_(std::make_unique<PowerManager>(*pawnIoPower_)) {}

CpuMonitor::~CpuMonitor() = default;

double CpuMonitor::UpdateUsage(bool& valid) {
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        valid = false;
        return 0.0;
    }
    const auto toValue = [](const FILETIME& value) {
        return (static_cast<ULONGLONG>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    const auto currentIdle = toValue(idle);
    const auto currentKernel = toValue(kernel);
    const auto currentUser = toValue(user);
    if (!initialized_) {
        idle_ = currentIdle;
        kernel_ = currentKernel;
        user_ = currentUser;
        initialized_ = true;
        valid = false;
        return 0.0;
    }
    const auto idleDelta = currentIdle - idle_;
    const auto kernelDelta = currentKernel - kernel_;
    const auto userDelta = currentUser - user_;
    idle_ = currentIdle;
    kernel_ = currentKernel;
    user_ = currentUser;
    const auto total = kernelDelta + userDelta;
    valid = total != 0;
    return valid ? 100.0 * static_cast<double>(total - idleDelta) / total : 0.0;
}

double CpuMonitor::ReadTemperature(bool& valid, bool forceWmiRetry) {
    const ULONGLONG now = GetTickCount64();
    double reading = 0.0;
    if (vendor_ == CpuVendor::Amd) {
        TemperatureManager temperatures(amdTemperature_, wmi_);
        const bool read = forceWmiRetry
            ? wmi_.ForceReadPackageTemperature(reading)
            : temperatures.ReadPackageTemperature(reading);
        if (read) {
            temperature_ = reading;
            temperatureValid_ = true;
            temperatureSuccessTick_ = now;
            valid = true;
            return temperature_;
        }
        valid = temperatureValid_ && temperatureSuccessTick_ != 0 &&
                now - temperatureSuccessTick_ <= 15000;
        if (!valid) temperatureValid_ = false;
        return temperature_;
    }
    PawnIoTemperatureProvider pawnIo(pawnIo_);
    const bool read = pawnIo.ReadPackageTemperature(reading) ||
        (forceWmiRetry ? wmi_.ForceReadPackageTemperature(reading)
                        : wmi_.ReadPackageTemperature(reading));
    if (read) {
        temperature_ = reading;
        temperatureValid_ = true;
        temperatureSuccessTick_ = now;
        valid = true;
        return temperature_;
    }
    valid = temperatureValid_ && temperatureSuccessTick_ != 0 &&
            now - temperatureSuccessTick_ <= 15000;
    if (!valid) temperatureValid_ = false;
    return temperature_;
}

double CpuMonitor::ReadPackagePower(bool& valid) {
    const ULONGLONG now = GetTickCount64();
    double reading = 0.0;
    if (vendor_ == CpuVendor::Amd) {
        valid = false;
        return 0.0;
    }
    if (powerManager_->ReadPackagePower(now, reading)) {
        power_ = reading;
        powerValid_ = true;
        powerSuccessTick_ = now;
    }
    valid = powerValid_ && powerSuccessTick_ != 0 && now - powerSuccessTick_ <= 5000;
    if (!valid) powerValid_ = false;
    return power_;
}

void CpuMonitor::ResetPowerSampling() {
    powerManager_->Reset();
    powerValid_ = false;
    powerSuccessTick_ = 0;
}

} // namespace monitor
