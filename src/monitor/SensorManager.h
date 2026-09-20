#pragma once
#include "CpuMonitor.h"
#include "NetworkMonitor.h"
#include "GpuMonitor.h"
#include "StorageMonitor.h"
#include "SystemMonitor.h"
#include "SensorDemand.h"
#include "SnapshotSensorProvider.h"
#include "InternalThermoFanProvider.h"
#include "../hardware/SensorRegistry.h"

namespace monitor {

inline bool ShouldReadInternalThermoFans(std::uint32_t demand) {
    return (demand & DemandInternalThermoFans) != 0;
}

class SensorManager {
public:
    explicit SensorManager(IInternalThermoFanProvider& internalThermoFan);
    void SetNetworkAdapter(const std::wstring& adapter) { network_.SetAdapter(adapter); }
    void SetStorageDrive(int index) { storage_.SetDriveIndex(index); }
    SensorSnapshot Update(std::uint32_t demand, bool forceRefresh = false);
    SensorSnapshot UpdateAll() { return Update(AllSensorDemand, true); }
    SensorCollection GenericSensors() const { return registry_.Values(); }
private:
    CpuMonitor cpu_;
    NetworkMonitor network_;
    GpuMonitor gpu_;
    SystemMonitor system_;
    StorageMonitor storage_;
    SensorRegistry registry_;
    IInternalThermoFanProvider* internalThermoFan_ = nullptr;
};
}
