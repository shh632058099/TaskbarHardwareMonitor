#include "WmiTemperatureProvider.h"

#include "CpuTemperature.h"

#include <comdef.h>
#include <wbemidl.h>

#include <string>

namespace monitor {
namespace {

bool QueryTemperature(IWbemServices* services, const wchar_t* className,
                      const wchar_t* propertyName, bool tenthsKelvin,
                      double& temperature) {
    const std::wstring query = L"SELECT " + std::wstring(propertyName) + L" FROM " + className;
    IEnumWbemClassObject* enumerator = nullptr;
    if (FAILED(services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query.c_str()),
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                   nullptr, &enumerator)) || !enumerator) return false;
    bool found = false;
    IWbemClassObject* object = nullptr;
    ULONG count = 0;
    if (SUCCEEDED(enumerator->Next(500, 1, &object, &count)) && count) {
        VARIANT value{};
        if (SUCCEEDED(object->Get(propertyName, 0, &value, nullptr, nullptr)) &&
            (value.vt == VT_I4 || value.vt == VT_UI4)) {
            const long reading = value.vt == VT_I4 ? value.lVal : static_cast<long>(value.ulVal);
            found = tenthsKelvin ? DecodeTemperatureTenthsKelvin(reading, temperature)
                                 : DecodeTemperatureCelsius(reading, temperature);
        }
        VariantClear(&value);
        object->Release();
    }
    enumerator->Release();
    return found;
}

} // namespace

bool WmiTemperatureProvider::ReadPackageTemperature(double& celsius) {
    return ReadPackageTemperature(celsius, false);
}

bool WmiTemperatureProvider::ForceReadPackageTemperature(double& celsius) {
    return ReadPackageTemperature(celsius, true);
}

bool WmiTemperatureProvider::ReadPackageTemperature(double& celsius, bool force) {
    const std::uint64_t now = GetTickCount64();
    if (!force && WmiRetryDelayMilliseconds(now, retryAfter_) != 0) return false;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = initialized == S_OK;
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    HRESULT result = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&locator));
    if (SUCCEEDED(result)) result = locator->ConnectServer(
        _bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    if (SUCCEEDED(result)) result = CoSetProxyBlanket(
        services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    bool valid = false;
    if (SUCCEEDED(result)) {
        valid = QueryTemperature(services, L"MSAcpi_ThermalZoneTemperature",
                                 L"CurrentTemperature", true, celsius);
        if (!valid) valid = QueryTemperature(services,
            L"Win32_PerfFormattedData_Counters_ThermalZoneInformation", L"Temperature", true, celsius);
        if (!valid) valid = QueryTemperature(services, L"Win32_TemperatureProbe",
                                              L"CurrentReading", true, celsius);
    }
    if (services) services->Release();
    if (locator) locator->Release();
    if (uninitialize) CoUninitialize();
    retryAfter_ = valid ? 0 : now + WmiRetryBackoffMilliseconds;
    return valid;
}

} // namespace monitor
