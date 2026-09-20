#include "DellThermoFanWmiProvider.h"

#include <algorithm>
#include <cwctype>
#include <vector>

#include <comdef.h>
#include <wbemidl.h>

namespace monitor {
namespace {

constexpr std::uint8_t MinimumTemperatureCelsius = 1;
constexpr std::uint8_t MaximumTemperatureCelsius = 125;
constexpr std::uint16_t MinimumFanRpm = 1;
constexpr std::uint16_t MaximumFanRpm = 10000;

std::uint16_t DecodeLittleEndian16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

bool IsTemperatureValid(std::uint8_t value) {
    return value >= MinimumTemperatureCelsius && value <= MaximumTemperatureCelsius;
}

bool IsFanRpmValid(std::uint16_t value) {
    return value >= MinimumFanRpm && value <= MaximumFanRpm;
}

bool ConnectWmi(const wchar_t* nameSpace, IWbemLocator*& locator, IWbemServices*& services) {
    HRESULT result = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&locator));
    if (SUCCEEDED(result)) result = locator->ConnectServer(
        _bstr_t(nameSpace), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    if (SUCCEEDED(result)) result = CoSetProxyBlanket(
        services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return SUCCEEDED(result);
}

bool QueryFirst(IWbemServices* services, const wchar_t* query,
                std::uint32_t timeoutMilliseconds, IWbemClassObject*& object) {
    IEnumWbemClassObject* enumerator = nullptr;
    const HRESULT result = services->ExecQuery(
        _bstr_t(L"WQL"), _bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (FAILED(result) || !enumerator) return false;
    ULONG count = 0;
    const bool found = SUCCEEDED(enumerator->Next(timeoutMilliseconds, 1, &object, &count)) && count == 1;
    enumerator->Release();
    return found;
}

bool ReadSmbiosManufacturer(std::wstring& manufacturer, std::uint32_t timeoutMilliseconds) {
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    bool valid = false;
    if (ConnectWmi(L"ROOT\\CIMV2", locator, services)) {
        IWbemClassObject* object = nullptr;
        if (QueryFirst(services, L"SELECT Manufacturer FROM Win32_ComputerSystem",
                       timeoutMilliseconds, object)) {
            VARIANT value{};
            if (SUCCEEDED(object->Get(L"Manufacturer", 0, &value, nullptr, nullptr)) &&
                value.vt == VT_BSTR && value.bstrVal) {
                manufacturer.assign(value.bstrVal, SysStringLen(value.bstrVal));
                valid = true;
            }
            VariantClear(&value);
            object->Release();
        }
    }
    if (services) services->Release();
    if (locator) locator->Release();
    return valid;
}

bool ReadThermoFanData(std::vector<std::uint8_t>& data, std::uint32_t timeoutMilliseconds) {
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    bool valid = false;
    if (ConnectWmi(L"ROOT\\WMI", locator, services)) {
        IWbemClassObject* object = nullptr;
        if (QueryFirst(services, L"SELECT Active, data FROM ThermoFanData",
                       timeoutMilliseconds, object)) {
            VARIANT active{};
            VARIANT value{};
            const bool isActive = SUCCEEDED(object->Get(L"Active", 0, &active, nullptr, nullptr)) &&
                                  active.vt == VT_BOOL && active.boolVal == VARIANT_TRUE;
            if (isActive && SUCCEEDED(object->Get(L"data", 0, &value, nullptr, nullptr)) &&
                (value.vt & VT_ARRAY) != 0 && (value.vt & VT_TYPEMASK) == VT_UI1 &&
                value.parray && SafeArrayGetDim(value.parray) == 1) {
                LONG lower = 0;
                LONG upper = -1;
                if (SUCCEEDED(SafeArrayGetLBound(value.parray, 1, &lower)) &&
                    SUCCEEDED(SafeArrayGetUBound(value.parray, 1, &upper)) &&
                    upper >= lower) {
                    const auto size = static_cast<std::size_t>(upper - lower + 1);
                    std::uint8_t* bytes = nullptr;
                    if (SUCCEEDED(SafeArrayAccessData(value.parray,
                                                       reinterpret_cast<void**>(&bytes)))) {
                        data.assign(bytes, bytes + size);
                        SafeArrayUnaccessData(value.parray);
                        valid = true;
                    }
                }
            }
            VariantClear(&value);
            VariantClear(&active);
            object->Release();
        }
    }
    if (services) services->Release();
    if (locator) locator->Release();
    return valid;
}

class WmiDellThermoFanDataSource final : public IDellThermoFanDataSource {
public:
    bool ReadManufacturer(std::wstring& manufacturer,
                          std::uint32_t timeoutMilliseconds) override {
        return ReadSmbiosManufacturer(manufacturer, timeoutMilliseconds);
    }

    bool ReadThermoFanData(std::vector<std::uint8_t>& data,
                           std::uint32_t timeoutMilliseconds) override {
        return monitor::ReadThermoFanData(data, timeoutMilliseconds);
    }
};

WmiDellThermoFanDataSource& DefaultDataSource() {
    static WmiDellThermoFanDataSource source;
    return source;
}

} // namespace

bool IsDellSmbiosManufacturer(const std::wstring& manufacturer) {
    auto begin = manufacturer.begin();
    while (begin != manufacturer.end() && std::iswspace(*begin)) ++begin;
    auto end = manufacturer.end();
    while (end != begin && std::iswspace(*(end - 1))) --end;
    std::wstring normalized(begin, end);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
    return normalized == L"dell" || normalized.rfind(L"dell ", 0) == 0;
}

bool DecodeDellThermoFanSnapshot(const std::uint8_t* data, std::size_t size,
                                 DellThermoFanSnapshot& snapshot) {
    snapshot = {};
    if (!data || size != DellThermoFanDataSize) return false;

    snapshot.cpuTemperature = data[0];
    snapshot.cpuTemperatureValid = IsTemperatureValid(data[0]);
    snapshot.gpuTemperature = data[3];
    snapshot.gpuTemperatureValid = IsTemperatureValid(data[3]);
    snapshot.cpuFanRpm = DecodeLittleEndian16(data + 14);
    snapshot.cpuFanRpmValid = IsFanRpmValid(snapshot.cpuFanRpm);
    snapshot.gpuFanRpm = DecodeLittleEndian16(data + 22);
    snapshot.gpuFanRpmValid = IsFanRpmValid(snapshot.gpuFanRpm);
    return true;
}

void AddDellThermoFanSensors(const DellThermoFanSnapshot& snapshot,
                             std::uint64_t timestamp, SensorCollection& sensors) {
    sensors.push_back({L"cpu.internal.temperature", L"CPU Internal Temperature", SensorType::Temperature,
                       snapshot.cpuTemperature, snapshot.cpuTemperatureValid, timestamp});
    sensors.push_back({L"gpu.internal.temperature", L"GPU Internal Temperature", SensorType::Temperature,
                       snapshot.gpuTemperature, snapshot.gpuTemperatureValid, timestamp});
    sensors.push_back({L"cpu.fan.rpm", L"CPU Fan", SensorType::Fan,
                       static_cast<double>(snapshot.cpuFanRpm), snapshot.cpuFanRpmValid, timestamp});
    sensors.push_back({L"gpu.fan.rpm", L"GPU Fan", SensorType::Fan,
                       static_cast<double>(snapshot.gpuFanRpm), snapshot.gpuFanRpmValid, timestamp});
}

DellThermoFanWmiProvider::DellThermoFanWmiProvider()
    : source_(&DefaultDataSource()) {}

DellThermoFanWmiProvider::DellThermoFanWmiProvider(IDellThermoFanDataSource& source)
    : source_(&source), ownsComInitialization_(false) {}

bool DellThermoFanWmiProvider::Read(DellThermoFanSnapshot& snapshot) {
    snapshot = {};
    HRESULT initialized = S_FALSE;
    bool uninitialize = false;
    if (ownsComInitialization_) {
        initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        uninitialize = initialized == S_OK || initialized == S_FALSE;
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    }

    std::vector<std::uint8_t> data;
    if (!manufacturerChecked_) {
        std::wstring manufacturer;
        if (source_->ReadManufacturer(manufacturer, DellThermoFanWmiQueryTimeoutMilliseconds)) {
            manufacturerIsDell_ = IsDellSmbiosManufacturer(manufacturer);
            manufacturerChecked_ = true;
        }
    }
    const bool valid = manufacturerIsDell_ && source_->ReadThermoFanData(
        data, DellThermoFanWmiQueryTimeoutMilliseconds) &&
                       DecodeDellThermoFanSnapshot(data.data(), data.size(), snapshot);
    available_ = valid;
    if (uninitialize) CoUninitialize();
    return valid;
}

bool DellThermoFanWmiProvider::Read(SensorSnapshot& snapshot, std::uint64_t timestamp,
                                    SensorCollection& sensors) {
    DellThermoFanSnapshot dellSnapshot;
    if (!Read(dellSnapshot)) return false;

    snapshot.cpuInternalTemperature = dellSnapshot.cpuTemperature;
    snapshot.cpuInternalTemperatureValid = dellSnapshot.cpuTemperatureValid;
    snapshot.gpuInternalTemperature = dellSnapshot.gpuTemperature;
    snapshot.gpuInternalTemperatureValid = dellSnapshot.gpuTemperatureValid;
    snapshot.cpuFanRpm = dellSnapshot.cpuFanRpm;
    snapshot.cpuFanRpmValid = dellSnapshot.cpuFanRpmValid;
    snapshot.gpuFanRpm = dellSnapshot.gpuFanRpm;
    snapshot.gpuFanRpmValid = dellSnapshot.gpuFanRpmValid;
    AddDellThermoFanSensors(dellSnapshot, timestamp, sensors);
    return true;
}

} // namespace monitor
