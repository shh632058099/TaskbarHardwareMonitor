#include "SensorManager.h"

namespace monitor {
SensorSnapshot SensorManager::Update(std::uint32_t demand, bool forceRefresh) {
    SensorSnapshot s;
    if (demand == 0) {
        cpu_.ResetUsage();
        network_.ResetSampling();
        storage_.ResetIoSampling();
        return s;
    }

    if (HasSensorDemand(demand, DemandCpuUsage))
        s.cpuUsage = cpu_.UpdateUsage(s.cpuUsageValid);
    else
        cpu_.ResetUsage();

    if (HasSensorDemand(demand, DemandCpuTemperature))
        s.cpuTemperature = cpu_.ReadTemperature(s.cpuTemperatureValid);
    if (HasSensorDemand(demand, DemandCpuPower))
        s.cpuPower = cpu_.ReadPackagePower(s.cpuPowerValid);

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

    return s;
}
}
