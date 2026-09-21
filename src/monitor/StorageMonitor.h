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
    ~StorageMonitor();
    void Update(SensorSnapshot& snapshot, bool collectTemperature, bool collectIo) override;
    void ForceRefresh() override {
        lastScan_ = 0;
        deviceCacheTick_ = 0;
        devices_.clear();
    }
    void ResetIoSampling() override { ioInitialized_ = false; ioDriveIndex_ = -1; }
    void SetDriveIndex(int index) {
        const int next = index >= 0 && index < 32 ? index : -1;
        if (selectedDriveIndex_ == next) return;
        selectedDriveIndex_ = next;
        preferredDriveIndex_ = -1;
        temperatureValid_ = false;
        lastScan_ = 0;
        deviceCacheTick_ = 0;
        devices_.clear();
        ioInitialized_ = false;
        ioDriveIndex_ = -1;
        CloseIoHandle();
    }
private:
    ULONGLONG lastScan_ = 0;
    ULONGLONG deviceCacheTick_ = 0;
    std::vector<StorageDeviceInfo> devices_;
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
    HANDLE ioDriveHandle_ = INVALID_HANDLE_VALUE;
    int ioHandleDriveIndex_ = -1;
    void CloseIoHandle();
    void UpdateDiskIo(SensorSnapshot& snapshot, int driveIndex);
    bool ReadDriveTemperature(DWORD index, bool nvme, double& temperature) const;
    const std::vector<StorageDeviceInfo>& CachedDevices(ULONGLONG now);
    bool ReadAtaTemperature(HANDLE drive, double& temperature) const;
    bool ReadNvmeTemperature(HANDLE drive, double& temperature) const;
};

}
