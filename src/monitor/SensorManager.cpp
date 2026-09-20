#include "SensorManager.h"

namespace monitor {
SensorManager::SensorManager(IInternalThermoFanProvider& internalThermoFan)
    : internalThermoFan_(&internalThermoFan) {}

SensorSnapshot SensorManager::Update(std::uint32_t demand, bool forceRefresh) {
    SensorSnapshot s;
    if (demand == 0) {
        cpu_.ResetUsage();
        cpu_.ResetPowerSampling();
        network_.ResetSampling();
        storage_.ResetIoSampling();
        registry_.Clear();
        return s;
    }

    if (HasSensorDemand(demand, DemandCpuUsage))
        s.cpuUsage = cpu_.UpdateUsage(s.cpuUsageValid);
    else
        cpu_.ResetUsage();

    if (HasSensorDemand(demand, DemandCpuTemperature))
        s.cpuTemperature = cpu_.ReadTemperature(s.cpuTemperatureValid, forceRefresh);
    if (HasSensorDemand(demand, DemandCpuPower))
        s.cpuPower = cpu_.ReadPackagePower(s.cpuPowerValid);
    else
        cpu_.ResetPowerSampling();

    if (HasSensorDemand(demand, DemandMemory)) system_.UpdateMemory(s);
    if (HasSensorDemand(demand, DemandCpuClock)) system_.UpdateCpuClock(s);
    if (HasSensorDemand(demand, DemandBattery) || HasSensorDemand(demand, DemandSystemPower))
        system_.UpdateBattery(s, HasSensorDemand(demand, DemandSystemPower));

    if (HasSensorDemand(demand, DemandNetwork)) network_.Update(s);
    else network_.ResetSampling();

    const std::uint32_t gpuDemand = demand &
        (DemandGpuTemperature | DemandGpuUsage | DemandVram | DemandGpuPower | DemandFan);
    if (gpuDemand != 0) gpu_.Update(s, gpuDemand);

    const bool diskTemperature = HasSensorDemand(demand, DemandDiskTemperature);
    const bool diskIo = HasSensorDemand(demand, DemandDiskIo);
    if (forceRefresh && (diskTemperature || diskIo)) storage_.ForceRefresh();
    if (diskTemperature || diskIo) storage_.Update(s, diskTemperature, diskIo);
    else storage_.ResetIoSampling();

    registry_.Clear();
    SensorCollection generic;
    AddSnapshotSensors(s, demand, static_cast<std::uint64_t>(GetTickCount64()), generic);
    if (ShouldReadInternalThermoFans(demand) && internalThermoFan_)
        internalThermoFan_->Read(s, static_cast<std::uint64_t>(GetTickCount64()), generic);
    for (auto& value : generic) registry_.Upsert(std::move(value));
    return s;
}
}
