#pragma once
#include "CpuMonitor.h"
#include "NetworkMonitor.h"
#include "GpuMonitor.h"
#include "StorageMonitor.h"
#include "SystemMonitor.h"
#include "SensorDemand.h"

namespace monitor {
class SensorManager {
public:
    void SetNetworkAdapter(const std::wstring& adapter) { network_.SetAdapter(adapter); }
    void SetStorageDrive(int index) { storage_.SetDriveIndex(index); }
    SensorSnapshot Update(std::uint32_t demand, bool forceRefresh = false);
    SensorSnapshot UpdateAll() { return Update(AllSensorDemand, true); }
private:
    CpuMonitor cpu_;
    NetworkMonitor network_;
    GpuMonitor gpu_;
    SystemMonitor system_;
    StorageMonitor storage_;
};
}
