#include "PawnIoTemperature.h"

#include "CpuTemperature.h"

#include <shellapi.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <winreg.h>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace monitor {
namespace {

constexpr unsigned int IoctlDeviceType = 41394u << 16;
constexpr unsigned int IoctlLoadBinary = IoctlDeviceType | (0x821u << 2);
constexpr unsigned int IoctlExecute = IoctlDeviceType | (0x841u << 2);
constexpr unsigned int IntelThermStatus = 0x019C;
constexpr unsigned int IntelPackageThermStatus = 0x01B1;
constexpr unsigned int IntelTemperatureTarget = 0x01A2;
constexpr unsigned int IntelRaplPowerUnit = 0x0606;
constexpr unsigned int IntelPackageEnergyStatus = 0x0611;
constexpr unsigned int ThermalValidBit = 0x80000000u;

bool IsAdministrator() {
    BOOL member = FALSE;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &administrators)) {
        return false;
    }
    CheckTokenMembership(nullptr, administrators, &member);
    FreeSid(administrators);
    return member == TRUE;
}

} // namespace

bool DecodeIntelThermalStatus(unsigned int status, unsigned int tjMax, double& celsius) {
    if ((status & ThermalValidBit) == 0) {
        return false;
    }
    const unsigned int distance = (status >> 16) & 0x7F;
    celsius = static_cast<double>(tjMax) - static_cast<double>(distance);
    return celsius > -20.0 && celsius < 150.0;
}

double DecodeIntelRaplEnergyUnit(unsigned long long raplPowerUnitMsr) {
    const unsigned int exponent = static_cast<unsigned int>((raplPowerUnitMsr >> 8) & 0x1F);
    return std::ldexp(1.0, -static_cast<int>(exponent));
}

bool ComputeIntelPackagePower(unsigned int previousEnergy, unsigned int currentEnergy,
                              double energyUnitJoules, double elapsedSeconds, double& watts) {
    if (!(energyUnitJoules > 0.0) || elapsedSeconds < 0.1 || elapsedSeconds > 30.0) {
        return false;
    }
    const std::uint32_t delta = static_cast<std::uint32_t>(currentEnergy - previousEnergy);
    const double computed = static_cast<double>(delta) * energyUnitJoules / elapsedSeconds;
    if (!std::isfinite(computed) || computed < 0.0 || computed > 500.0) {
        return false;
    }
    watts = computed;
    return true;
}

PawnIoTemperature::~PawnIoTemperature() {
    if (device_ != INVALID_HANDLE_VALUE) {
        CloseHandle(device_);
    }
}

void PawnIoTemperature::Log(const wchar_t* message) const {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
    std::wstring logPath(modulePath);
    const auto slash = logPath.find_last_of(L"\\/");
    logPath = logPath.substr(0, slash + 1) + L"temperature-debug.log";
    std::wofstream log(logPath, std::ios::app);
    if (log) {
        log << GetTickCount64() << L" " << message << L"\n";
    }
}

bool PawnIoTemperature::ReadResource(int resourceId, const wchar_t* fileName, std::wstring& path) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) {
        return false;
    }
    HGLOBAL loaded = LoadResource(module, resource);
    void* data = LockResource(loaded);
    const DWORD size = SizeofResource(module, resource);
    if (!data || size == 0) {
        return false;
    }

    wchar_t tempDirectory[MAX_PATH]{};
    if (!GetTempPathW(_countof(tempDirectory), tempDirectory)) {
        return false;
    }
    path = std::wstring(tempDirectory) + fileName;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool success = WriteFile(file, data, size, &written, nullptr) && written == size;
    CloseHandle(file);
    if (!success) {
        DeleteFileW(path.c_str());
    }
    return success;
}

bool PawnIoTemperature::InstallPawnIo() {
    if (!IsAdministrator()) {
        std::wstring installer;
        if (!ReadResource(101, L"TaskbarMonitor_PawnIO_setup.exe", installer)) {
            return false;
        }
        SHELLEXECUTEINFOW execute{};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS;
        execute.lpVerb = L"runas";
        execute.lpFile = installer.c_str();
        execute.lpParameters = L"-install";
        execute.nShow = SW_HIDE;
        const bool launched = ShellExecuteExW(&execute) != FALSE;
        if (launched && execute.hProcess) {
            WaitForSingleObject(execute.hProcess, 120000);
            CloseHandle(execute.hProcess);
        }
        DeleteFileW(installer.c_str());
        return launched;
    }

    std::wstring installer;
    if (!ReadResource(101, L"TaskbarMonitor_PawnIO_setup.exe", installer)) {
        return false;
    }
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpFile = installer.c_str();
    execute.lpParameters = L"-install";
    execute.nShow = SW_HIDE;
    const bool launched = ShellExecuteExW(&execute) != FALSE;
    if (launched && execute.hProcess) {
        WaitForSingleObject(execute.hProcess, 120000);
        CloseHandle(execute.hProcess);
    }
    DeleteFileW(installer.c_str());
    return launched;
}

bool PawnIoTemperature::IsPawnIoInstalled() const {
    HKEY key = nullptr;
    const auto result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO",
        0, KEY_QUERY_VALUE, &key);
    if (result == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }

    HKEY wowKey = nullptr;
    const auto wowResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO",
        0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &wowKey);
    if (wowResult == ERROR_SUCCESS) {
        RegCloseKey(wowKey);
        return true;
    }
    return false;
}

bool PawnIoTemperature::OpenDevice() {
    device_ = CreateFileW(
        L"\\\\?\\GLOBALROOT\\Device\\PawnIO",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    return device_ != INVALID_HANDLE_VALUE;
}

bool PawnIoTemperature::LoadIntelMsrModule() {
    std::wstring path;
    if (!ReadResource(102, L"TaskbarMonitor_IntelMSR.bin", path)) {
        return false;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(path.c_str());
        return false;
    }
    LARGE_INTEGER length{};
    const bool sized = GetFileSizeEx(file, &length) && length.QuadPart > 0 && length.QuadPart < 1024 * 1024;
    std::vector<BYTE> binary(sized ? static_cast<size_t>(length.QuadPart) : 0);
    DWORD read = 0;
    const bool readOk = sized && ReadFile(file, binary.data(), static_cast<DWORD>(binary.size()), &read, nullptr) && read == binary.size();
    CloseHandle(file);
    DeleteFileW(path.c_str());
    if (!readOk) {
        return false;
    }

    DWORD returned = 0;
    moduleLoaded_ = DeviceIoControl(device_, IoctlLoadBinary, binary.data(), read,
                                     nullptr, 0, &returned, nullptr) != FALSE;
    if (!moduleLoaded_) {
        Log(L"Load IntelMSR module failed");
    }
    return moduleLoaded_;
}

bool PawnIoTemperature::Initialize() {
    if (moduleLoaded_) {
        return true;
    }
    if (unavailable_) {
        return false;
    }

    const ULONGLONG now = GetTickCount64();
    if (nextRetryTick_ != 0 && now < nextRetryTick_) {
        return false;
    }
    nextRetryTick_ = now + 30000;

    if (!OpenDevice() && !installationAttempted_ && !IsPawnIoInstalled()) {
        installationAttempted_ = true;
        if (!InstallPawnIo()) {
            unavailable_ = true;
            return false;
        }
        Sleep(1000);
        OpenDevice();
    }
    if (device_ == INVALID_HANDLE_VALUE) {
        Log(L"Open PawnIO device failed");
        unavailable_ = true;
        return false;
    }
    if (!LoadIntelMsrModule()) {
        unavailable_ = true;
        CloseHandle(device_);
        device_ = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

bool PawnIoTemperature::ExecuteReadMsr(unsigned int index, unsigned long long& value) {
    if (device_ == INVALID_HANDLE_VALUE || !moduleLoaded_) {
        return false;
    }

    std::array<BYTE, 40> input{};
    const char functionName[] = "ioctl_read_msr";
    memcpy(input.data(), functionName, sizeof(functionName) - 1);
    memcpy(input.data() + 32, &index, sizeof(index));
    DWORD returned = 0;
    unsigned long long output = 0;
    if (!DeviceIoControl(device_, IoctlExecute, input.data(), static_cast<DWORD>(input.size()),
                         &output, sizeof(output), &returned, nullptr) ||
        returned < sizeof(output)) {
        wchar_t message[128]{};
        wsprintfW(message, L"Read MSR 0x%04X failed, error=%lu", index, GetLastError());
        Log(message);
        return false;
    }
    value = output;
    return true;
}

bool PawnIoTemperature::ReadPackageTemperature(double& celsius) {
    if (!Initialize()) {
        return false;
    }

    if (ReadTemperatureRegister(IntelPackageThermStatus, celsius) ||
        ReadTemperatureRegister(IntelThermStatus, celsius)) {
#ifndef NDEBUG
        std::wstringstream message;
        message << L"Temperature read succeeded: " << std::fixed
                << std::setprecision(2) << celsius << L" C";
        Log(message.str().c_str());
#endif
        return true;
    }
    Log(L"Both package and core temperature MSRs returned invalid data");
    return false;
}

bool PawnIoTemperature::ReadPackagePower(double& watts) {
    if (!Initialize()) {
        return false;
    }

    if (!(raplEnergyUnit_ > 0.0)) {
        unsigned long long units = 0;
        if (!ExecuteReadMsr(IntelRaplPowerUnit, units)) {
            return false;
        }
        raplEnergyUnit_ = DecodeIntelRaplEnergyUnit(units);
        if (!(raplEnergyUnit_ > 0.0)) {
            return false;
        }
    }

    unsigned long long energy = 0;
    if (!ExecuteReadMsr(IntelPackageEnergyStatus, energy)) {
        return false;
    }
    const unsigned int currentEnergy = static_cast<unsigned int>(energy & 0xFFFFFFFFull);
    const ULONGLONG now = GetTickCount64();
    if (!powerInitialized_) {
        lastPackageEnergy_ = currentEnergy;
        lastPowerTick_ = now;
        powerInitialized_ = true;
        return false;
    }

    const double elapsedSeconds = static_cast<double>(now - lastPowerTick_) / 1000.0;
    const unsigned int previousEnergy = lastPackageEnergy_;
    lastPackageEnergy_ = currentEnergy;
    lastPowerTick_ = now;
    return ComputeIntelPackagePower(previousEnergy, currentEnergy,
                                    raplEnergyUnit_, elapsedSeconds, watts);
}

bool PawnIoTemperature::ReadTemperatureRegister(unsigned int registerIndex, double& celsius) {
    unsigned long long status = 0;
    if (!ExecuteReadMsr(registerIndex, status)) {
        return false;
    }
    unsigned long long target = 0;
    const unsigned int tjMax = ExecuteReadMsr(IntelTemperatureTarget, target)
        ? static_cast<unsigned int>((target >> 16) & 0xFF)
        : 100;
    return DecodeIntelThermalStatus(static_cast<unsigned int>(status), tjMax, celsius);
}

} // namespace monitor
