#include "SensorManager.h"

namespace monitor {
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
    if (ShouldReadInternalThermoFans(demand)) {
        DellThermoFanSnapshot dellSnapshot;
        if (dellThermoFan_.Read(dellSnapshot)) {
            s.cpuInternalTemperature = dellSnapshot.cpuTemperature;
            s.cpuInternalTemperatureValid = dellSnapshot.cpuTemperatureValid;
            s.gpuInternalTemperature = dellSnapshot.gpuTemperature;
            s.gpuInternalTemperatureValid = dellSnapshot.gpuTemperatureValid;
            s.cpuFanRpm = dellSnapshot.cpuFanRpm;
            s.cpuFanRpmValid = dellSnapshot.cpuFanRpmValid;
            s.gpuFanRpm = dellSnapshot.gpuFanRpm;
            s.gpuFanRpmValid = dellSnapshot.gpuFanRpmValid;
            AddDellThermoFanSensors(dellSnapshot,
                                    static_cast<std::uint64_t>(GetTickCount64()), generic);
        }
    }
    for (auto& value : generic) registry_.Upsert(std::move(value));
    return s;
}
}
