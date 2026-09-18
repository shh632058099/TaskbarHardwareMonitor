#include "CpuMonitor.h"
#include "CpuTemperature.h"

#include <wbemidl.h>
#include <comdef.h>

#include <string>

namespace monitor {

namespace {

bool QueryTemperature(
    IWbemServices* services,
    const wchar_t* className,
    const wchar_t* propertyName,
    bool tenthsKelvin,
    double& temperature) {
    const std::wstring query = L"SELECT " + std::wstring(propertyName) + L" FROM " + className;
    IEnumWbemClassObject* enumerator = nullptr;
    const HRESULT queryResult = services->ExecQuery(
        _bstr_t(L"WQL"), _bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator);
    if (FAILED(queryResult) || !enumerator) {
        return false;
    }

    bool found = false;
    IWbemClassObject* object = nullptr;
    ULONG count = 0;
    if (SUCCEEDED(enumerator->Next(500, 1, &object, &count)) && count) {
        VARIANT value{};
        if (SUCCEEDED(object->Get(propertyName, 0, &value, nullptr, nullptr)) &&
            (value.vt == VT_I4 || value.vt == VT_UI4)) {
            const long reading = value.vt == VT_I4
                ? value.lVal
                : static_cast<long>(value.ulVal);
            found = tenthsKelvin
                ? DecodeTemperatureTenthsKelvin(reading, temperature)
                : DecodeTemperatureCelsius(reading, temperature);
        }
        VariantClear(&value);
        object->Release();
    }
    enumerator->Release();
    return found;
}

} // namespace

double CpuMonitor::UpdateUsage(bool& valid) {
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        valid = false;
        return 0.0;
    }

    const auto toValue = [](const FILETIME& value) {
        return (static_cast<ULONGLONG>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    const auto currentIdle = toValue(idle);
    const auto currentKernel = toValue(kernel);
    const auto currentUser = toValue(user);
    if (!initialized_) {
        idle_ = currentIdle;
        kernel_ = currentKernel;
        user_ = currentUser;
        initialized_ = true;
        valid = false;
        return 0.0;
    }

    const auto idleDelta = currentIdle - idle_;
    const auto kernelDelta = currentKernel - kernel_;
    const auto userDelta = currentUser - user_;
    idle_ = currentIdle;
    kernel_ = currentKernel;
    user_ = currentUser;

    const auto total = kernelDelta + userDelta;
    valid = total != 0;
    return valid ? 100.0 * static_cast<double>(total - idleDelta) / total : 0.0;
}

double CpuMonitor::ReadTemperature(bool& valid) {
    const ULONGLONG now = GetTickCount64();
    double reading = 0.0;
    bool readingValid = false;

    if (pawnIo_.ReadPackageTemperature(reading)) {
        temperature_ = reading;
        temperatureValid_ = true;
        temperatureSuccessTick_ = now;
        valid = true;
        return temperature_;
    }

    const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = initializeResult == S_OK;
    if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE) {
        valid = temperatureValid_ && temperatureSuccessTick_ != 0 &&
                now - temperatureSuccessTick_ <= 15000;
        return temperature_;
    }

    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
    if (SUCCEEDED(result)) {
        result = locator->ConnectServer(
            _bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    }
    if (SUCCEEDED(result)) {
        result = CoSetProxyBlanket(
            services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    }

    if (SUCCEEDED(result)) {
        readingValid = QueryTemperature(
            services, L"MSAcpi_ThermalZoneTemperature", L"CurrentTemperature",
            true, reading);
    }
    if (SUCCEEDED(result) && !readingValid) {
        readingValid = QueryTemperature(
            services, L"Win32_PerfFormattedData_Counters_ThermalZoneInformation",
            L"Temperature", true, reading);
    }
    if (SUCCEEDED(result) && !readingValid) {
        readingValid = QueryTemperature(
            services, L"Win32_TemperatureProbe", L"CurrentReading",
            true, reading);
    }

    if (services) services->Release();
    if (locator) locator->Release();
    if (uninitialize) CoUninitialize();

    if (readingValid) {
        temperature_ = reading;
        temperatureValid_ = true;
        temperatureSuccessTick_ = now;
    }
    valid = temperatureValid_ && temperatureSuccessTick_ != 0 &&
            now - temperatureSuccessTick_ <= 15000;
    if (!valid) temperatureValid_ = false;
    return temperature_;
}

double CpuMonitor::ReadPackagePower(bool& valid) {
    const ULONGLONG now = GetTickCount64();
    double reading = 0.0;
    if (pawnIo_.ReadPackagePower(reading)) {
        power_ = reading;
        powerValid_ = true;
        powerSuccessTick_ = now;
    }
    valid = powerValid_ && powerSuccessTick_ != 0 && now - powerSuccessTick_ <= 5000;
    if (!valid) powerValid_ = false;
    return power_;
}

} // namespace monitor
