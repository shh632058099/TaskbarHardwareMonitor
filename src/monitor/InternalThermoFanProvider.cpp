#include "InternalThermoFanProvider.h"

#include <utility>

namespace monitor {
namespace {

void AddNewSensors(const SensorCollection& before, SensorCollection& sensors,
                   const SensorSnapshot& beforeSnapshot, const SensorSnapshot& snapshot) {
    const bool keepCpuFan = !beforeSnapshot.cpuFanRpmValid && snapshot.cpuFanRpmValid;
    const bool keepGpuFan = !beforeSnapshot.gpuFanRpmValid && snapshot.gpuFanRpmValid;
    const bool keepCpuTemperature = !beforeSnapshot.cpuInternalTemperatureValid &&
                                    snapshot.cpuInternalTemperatureValid;
    const bool keepGpuTemperature = !beforeSnapshot.gpuInternalTemperatureValid &&
                                    snapshot.gpuInternalTemperatureValid;
    SensorCollection filtered;
    for (std::size_t index = before.size(); index < sensors.size(); ++index) {
        const auto& sensor = sensors[index];
        if ((sensor.identifier == L"cpu.fan.rpm" && keepCpuFan) ||
            (sensor.identifier == L"gpu.fan.rpm" && keepGpuFan) ||
            (sensor.identifier == L"cpu.internal.temperature" && keepCpuTemperature) ||
            (sensor.identifier == L"gpu.internal.temperature" && keepGpuTemperature)) {
            filtered.push_back(sensor);
        }
    }
    sensors.resize(before.size());
    sensors.insert(sensors.end(), std::make_move_iterator(filtered.begin()),
                   std::make_move_iterator(filtered.end()));
}

void RestoreExistingReadings(const SensorSnapshot& before, SensorSnapshot& snapshot) {
    if (before.cpuInternalTemperatureValid) {
        snapshot.cpuInternalTemperature = before.cpuInternalTemperature;
        snapshot.cpuInternalTemperatureValid = true;
    }
    if (before.gpuInternalTemperatureValid) {
        snapshot.gpuInternalTemperature = before.gpuInternalTemperature;
        snapshot.gpuInternalTemperatureValid = true;
    }
    if (before.cpuFanRpmValid) {
        snapshot.cpuFanRpm = before.cpuFanRpm;
        snapshot.cpuFanRpmValid = true;
    }
    if (before.gpuFanRpmValid) {
        snapshot.gpuFanRpm = before.gpuFanRpm;
        snapshot.gpuFanRpmValid = true;
    }
}

} // namespace

InternalThermoFanProviderManager::InternalThermoFanProviderManager(
    std::vector<IInternalThermoFanProvider*> providers)
    : providers_(std::move(providers)) {}

bool InternalThermoFanProviderManager::Read(SensorSnapshot& snapshot, std::uint64_t timestamp,
                                            SensorCollection& sensors) {
    bool available = false;
    for (auto* provider : providers_) {
        if (!provider) continue;
        const SensorSnapshot beforeSnapshot = snapshot;
        const SensorCollection beforeSensors = sensors;
        const bool read = provider->Read(snapshot, timestamp, sensors);
        RestoreExistingReadings(beforeSnapshot, snapshot);
        AddNewSensors(beforeSensors, sensors, beforeSnapshot, snapshot);
        available = available || read;
    }
    return available;
}

} // namespace monitor
