#pragma once
#include <windows.h>
#include "SensorTypes.h"
#include "StorageProvider.h"
#include <string>
#include <vector>

namespace monitor {

struct StorageDeviceInfo {
    int index = -1;
    std::wstring name;
    std::wstring bus;
};

std::vector<StorageDeviceInfo> EnumerateStorageDevices();

class StorageMonitor : public IStorageProvider {
public:
    void Update(SensorSnapshot& snapshot, bool collectTemperature, bool collectIo) override;
    void ForceRefresh() override { lastScan_ = 0; }
    void ResetIoSampling() override { ioInitialized_ = false; ioDriveIndex_ = -1; }
    void SetDriveIndex(int index) {
        const int next = index >= 0 && index < 32 ? index : -1;
        if (selectedDriveIndex_ == next) return;
        selectedDriveIndex_ = next;
        preferredDriveIndex_ = -1;
        temperatureValid_ = false;
        lastScan_ = 0;
        ioInitialized_ = false;
        ioDriveIndex_ = -1;
    }
private:
    ULONGLONG lastScan_ = 0;
    double temperature_ = 0.0;
    bool temperatureValid_ = false;
    int selectedDriveIndex_ = -1;
    int preferredDriveIndex_ = -1;
    bool preferredDriveNvme_ = true;
    int ioDriveIndex_ = -1;
    ULONGLONG lastIoTick_ = 0;
    std::uint64_t lastReadBytes_ = 0;
    std::uint64_t lastWriteBytes_ = 0;
    bool ioInitialized_ = false;
    void UpdateDiskIo(SensorSnapshot& snapshot, int driveIndex);
    bool ReadDriveTemperature(DWORD index, bool nvme, double& temperature) const;
    bool ReadAtaTemperature(HANDLE drive, double& temperature) const;
    bool ReadNvmeTemperature(HANDLE drive, double& temperature) const;
};

}
