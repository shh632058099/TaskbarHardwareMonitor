#include "StorageMonitor.h"

#include <windows.h>
#include <winioctl.h>
#include <ntddstor.h>
#include <nvme.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <utility>

#include <cstring>
#include <vector>

#ifndef SMART_READ_DATA
#define SMART_READ_DATA 0xD0
#endif

namespace monitor {

namespace {

std::wstring TrimStorageText(std::wstring value) {
    while (!value.empty() && std::iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && std::iswspace(value.back())) value.pop_back();
    return value;
}

std::wstring DescriptorText(const BYTE* buffer, DWORD bytes, DWORD offset) {
    if (offset == 0 || offset >= bytes) return {};
    const char* text = reinterpret_cast<const char*>(buffer + offset);
    const std::size_t maxLength = bytes - offset;
    std::size_t length = 0;
    while (length < maxLength && text[length] != '\0') ++length;
    if (length == 0 || length == maxLength) return {};
    const int wideLength = MultiByteToWideChar(
        CP_ACP, 0, text, static_cast<int>(length), nullptr, 0);
    if (wideLength <= 0) return {};
    std::wstring result(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(length),
                        result.data(), wideLength);
    return TrimStorageText(std::move(result));
}

const wchar_t* BusName(STORAGE_BUS_TYPE bus) {
    switch (bus) {
    case BusTypeNvme: return L"NVMe";
    case BusTypeSata: return L"SATA";
    case BusTypeAta: return L"ATA";
    case BusTypeUsb: return L"USB";
    case BusTypeScsi: return L"SCSI";
    case BusTypeSas: return L"SAS";
    case BusTypeRAID: return L"RAID";
    default: return L"Other";
    }
}

} // namespace

std::vector<StorageDeviceInfo> EnumerateStorageDevices() {
    std::vector<StorageDeviceInfo> devices;
    for (DWORD index = 0; index < 32; ++index) {
        wchar_t path[64]{};
        wsprintfW(path, L"\\\\.\\PhysicalDrive%lu", index);
        HANDLE drive = CreateFileW(
            path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (drive == INVALID_HANDLE_VALUE) continue;

        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        std::vector<BYTE> buffer(4096, 0);
        DWORD returned = 0;
        const bool queried = DeviceIoControl(
            drive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
            buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) != FALSE;
        CloseHandle(drive);
        if (!queried || returned < sizeof(STORAGE_DEVICE_DESCRIPTOR)) continue;

        const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
        std::wstring vendor = DescriptorText(buffer.data(), returned, descriptor->VendorIdOffset);
        std::wstring product = DescriptorText(buffer.data(), returned, descriptor->ProductIdOffset);
        std::wstring name;
        if (!vendor.empty() && !product.empty()) name = vendor + L" " + product;
        else name = !product.empty() ? product : vendor;
        name = TrimStorageText(std::move(name));
        if (name.empty()) name = L"PhysicalDrive" + std::to_wstring(index);

        StorageDeviceInfo info;
        info.index = static_cast<int>(index);
        info.name = std::move(name);
        info.bus = BusName(descriptor->BusType);
        devices.push_back(std::move(info));
    }
    return devices;
}

bool StorageMonitor::ReadNvmeTemperature(HANDLE drive, double& temperature) const {
    const DWORD queryHeaderSize = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters);
    const DWORD protocolSize = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    const DWORD healthSize = sizeof(NVME_HEALTH_INFO_LOG);
    const DWORD inputSize = queryHeaderSize + protocolSize;
    const DWORD outputSize = std::max<DWORD>(
        sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR) + healthSize,
        inputSize + healthSize);

    std::vector<BYTE> buffer(outputSize, 0);
    auto* query = reinterpret_cast<STORAGE_PROPERTY_QUERY*>(buffer.data());
    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    auto* specific = reinterpret_cast<STORAGE_PROTOCOL_SPECIFIC_DATA*>(query->AdditionalParameters);
    specific->ProtocolType = ProtocolTypeNvme;
    specific->DataType = NVMeDataTypeLogPage;
    specific->ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
    specific->ProtocolDataRequestSubValue = 0;
    specific->ProtocolDataOffset = protocolSize;
    specific->ProtocolDataLength = healthSize;

    DWORD returned = 0;
    if (!DeviceIoControl(
            drive, IOCTL_STORAGE_QUERY_PROPERTY, buffer.data(), inputSize,
            buffer.data(), outputSize, &returned, nullptr) ||
        returned < sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) {
        return false;
    }

    auto* descriptor = reinterpret_cast<STORAGE_PROTOCOL_DATA_DESCRIPTOR*>(buffer.data());
    auto* data = &descriptor->ProtocolSpecificData;
    const std::size_t dataOffset = static_cast<std::size_t>(
        reinterpret_cast<BYTE*>(data) - buffer.data());
    const std::size_t logOffset = dataOffset + data->ProtocolDataOffset;
    if (data->ProtocolType != ProtocolTypeNvme ||
        data->ProtocolDataLength < healthSize ||
        logOffset > returned || healthSize > returned - logOffset) {
        return false;
    }

    auto* log = reinterpret_cast<const NVME_HEALTH_INFO_LOG*>(buffer.data() + logOffset);
    const USHORT kelvin = static_cast<USHORT>(log->Temperature[0]) |
                          (static_cast<USHORT>(log->Temperature[1]) << 8);
    const double celsius = static_cast<double>(kelvin) - 273.15;
    if (kelvin < 250 || kelvin > 450 || celsius < -20.0 || celsius > 150.0) {
        return false;
    }

    temperature = celsius;
    return true;
}

bool StorageMonitor::ReadAtaTemperature(HANDLE drive, double& temperature) const {
    GETVERSIONINPARAMS version{};
    DWORD returned = 0;
    if (!DeviceIoControl(drive, SMART_GET_VERSION, nullptr, 0, &version,
                         sizeof(version), &returned, nullptr) ||
        !(version.fCapabilities & CAP_SMART_CMD) || version.bIDEDeviceMap == 0) {
        return false;
    }

    std::vector<BYTE> input(sizeof(SENDCMDINPARAMS));
    auto* parameters = reinterpret_cast<SENDCMDINPARAMS*>(input.data());
    parameters->cBufferSize = 512;
    parameters->irDriveRegs.bCommandReg = SMART_READ_DATA;
    parameters->irDriveRegs.bFeaturesReg = READ_ATTRIBUTES;
    parameters->irDriveRegs.bSectorCountReg = 1;
    parameters->irDriveRegs.bSectorNumberReg = 1;
    parameters->irDriveRegs.bCylLowReg = SMART_CYL_LOW;
    parameters->irDriveRegs.bCylHighReg = SMART_CYL_HI;
    parameters->irDriveRegs.bDriveHeadReg = 0xA0;

    std::vector<BYTE> output(sizeof(SENDCMDOUTPARAMS) + 512);
    auto* result = reinterpret_cast<SENDCMDOUTPARAMS*>(output.data());
    if (!DeviceIoControl(
            drive, SMART_RCV_DRIVE_DATA, parameters, static_cast<DWORD>(input.size()),
            result, static_cast<DWORD>(output.size()), &returned, nullptr) ||
        returned < sizeof(SENDCMDOUTPARAMS) || result->cBufferSize < 512) {
        return false;
    }

    const BYTE* data = result->bBuffer;
    for (int index = 0; index < 30; ++index) {
        const BYTE* attribute = data + 2 + index * 12;
        if (attribute[0] == 0xC2 || attribute[0] == 0xBE) {
            const double celsius = attribute[5];
            if (celsius > 0 && celsius < 150) {
                temperature = celsius;
                return true;
            }
        }
    }
    return false;
}

bool StorageMonitor::ReadDriveTemperature(DWORD index, bool nvme, double& temperature) const {
    wchar_t path[64]{};
    wsprintfW(path, L"\\\\.\\PhysicalDrive%lu", index);
    HANDLE drive = CreateFileW(
        path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (drive == INVALID_HANDLE_VALUE) {
        return false;
    }
    const bool valid = nvme
        ? ReadNvmeTemperature(drive, temperature)
        : ReadAtaTemperature(drive, temperature);
    CloseHandle(drive);
    return valid;
}

void StorageMonitor::UpdateDiskIo(SensorSnapshot& snapshot, int driveIndex) {
    if (driveIndex < 0 || driveIndex >= 32) return;

    wchar_t path[64]{};
    wsprintfW(path, L"\\\\.\\PhysicalDrive%d", driveIndex);
    HANDLE drive = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (drive == INVALID_HANDLE_VALUE) {
        drive = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, 0, nullptr);
    }
    if (drive == INVALID_HANDLE_VALUE) return;

    DISK_PERFORMANCE performance{};
    DWORD returned = 0;
    const bool ok = DeviceIoControl(drive, IOCTL_DISK_PERFORMANCE, nullptr, 0,
                                    &performance, sizeof(performance), &returned, nullptr) != FALSE;
    CloseHandle(drive);
    if (!ok || returned < sizeof(performance) || performance.BytesRead.QuadPart < 0 ||
        performance.BytesWritten.QuadPart < 0) {
        ioInitialized_ = false;
        return;
    }

    const auto now = GetTickCount64();
    const auto readBytes = static_cast<std::uint64_t>(performance.BytesRead.QuadPart);
    const auto writeBytes = static_cast<std::uint64_t>(performance.BytesWritten.QuadPart);
    if (ioDriveIndex_ != driveIndex || !ioInitialized_ || now <= lastIoTick_ ||
        readBytes < lastReadBytes_ || writeBytes < lastWriteBytes_) {
        ioDriveIndex_ = driveIndex;
        lastIoTick_ = now;
        lastReadBytes_ = readBytes;
        lastWriteBytes_ = writeBytes;
        ioInitialized_ = true;
        return;
    }

    const auto elapsed = now - lastIoTick_;
    if (elapsed == 0) return;
    snapshot.diskReadBytesPerSecond = (readBytes - lastReadBytes_) * 1000ULL / elapsed;
    snapshot.diskWriteBytesPerSecond = (writeBytes - lastWriteBytes_) * 1000ULL / elapsed;
    snapshot.diskIoValid = true;
    lastIoTick_ = now;
    lastReadBytes_ = readBytes;
    lastWriteBytes_ = writeBytes;
}

void StorageMonitor::Update(SensorSnapshot& snapshot, bool collectTemperature, bool collectIo) {
    if (collectTemperature) {
        temperatureValid_ = false;

        if (selectedDriveIndex_ >= 0) {
            double temperature = 0.0;
            const DWORD index = static_cast<DWORD>(selectedDriveIndex_);
            if (ReadDriveTemperature(index, true, temperature) ||
                ReadDriveTemperature(index, false, temperature)) {
                temperature_ = temperature;
                temperatureValid_ = true;
            }
        } else {
            if (preferredDriveIndex_ >= 0) {
                double temperature = 0.0;
                if (ReadDriveTemperature(static_cast<DWORD>(preferredDriveIndex_),
                                         preferredDriveNvme_, temperature)) {
                    temperature_ = temperature;
                    temperatureValid_ = true;
                } else {
                    preferredDriveIndex_ = -1;
                }
            }
            for (int pass = 0; pass < 2 && !temperatureValid_; ++pass) {
                for (DWORD index = 0; index < 32; ++index) {
                    double temperature = 0.0;
                    const bool nvme = pass == 0;
                    if (ReadDriveTemperature(index, nvme, temperature)) {
                        temperature_ = temperature;
                        temperatureValid_ = true;
                        preferredDriveIndex_ = static_cast<int>(index);
                        preferredDriveNvme_ = nvme;
                        break;
                    }
                }
            }
        }
    }

    if (collectTemperature) {
        snapshot.diskTemperature = temperature_;
        snapshot.diskTemperatureValid = temperatureValid_;
    }

    if (collectIo) {
        int ioDrive = selectedDriveIndex_ >= 0 ? selectedDriveIndex_ : preferredDriveIndex_;
        if (ioDrive < 0) {
            const auto devices = EnumerateStorageDevices();
            for (const auto& device : devices) {
                if (device.bus == L"NVMe") { ioDrive = device.index; break; }
            }
            if (ioDrive < 0 && !devices.empty()) ioDrive = devices.front().index;
            if (ioDrive >= 0) preferredDriveIndex_ = ioDrive;
        }
        UpdateDiskIo(snapshot, ioDrive);
    } else {
        ResetIoSampling();
    }
}

} // namespace monitor
