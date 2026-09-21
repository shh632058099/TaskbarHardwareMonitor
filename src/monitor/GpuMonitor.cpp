#include "GpuMonitor.h"

#include <windows.h>

#include <string>

namespace monitor {
namespace {

using Return = int;
using Device = void*;
using Init = Return (*)();
using Shutdown = Return (*)();
using DeviceCount = Return (*)(unsigned int*);
using DeviceByIndex = Return (*)(unsigned int, Device*);
using Temperature = Return (*)(Device, int, unsigned int*);
using UtilizationRates = Return (*)(Device, NvmlUtilization*);
using MemoryInfo = Return (*)(Device, NvmlMemory*);
using PowerUsage = Return (*)(Device, unsigned int*);
using FanSpeed = Return (*)(Device, unsigned int*);

constexpr Return Success = 0;
constexpr int TemperatureSensor = 0;

template <typename Function>
Function GetFunction(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

HMODULE LoadNvmlModule() {
    if (HMODULE module = LoadLibraryW(L"nvml.dll")) {
        return module;
    }

    wchar_t programFiles[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"ProgramW6432", programFiles, _countof(programFiles));
    if (length > 0 && length < _countof(programFiles)) {
        const std::wstring path = std::wstring(programFiles) + L"\\NVIDIA Corporation\\NVSMI\\nvml.dll";
        if (HMODULE module = LoadLibraryW(path.c_str())) {
            return module;
        }
    }
    return nullptr;
}

} // namespace

GpuMonitor::~GpuMonitor() {
    if (!module_) {
        return;
    }
    if (const auto shutdown = GetFunction<Shutdown>(static_cast<HMODULE>(module_), "nvmlShutdown")) {
        shutdown();
    }
    FreeLibrary(static_cast<HMODULE>(module_));
}

void GpuMonitor::Initialize() {
    if (module_) {
        return;
    }
    const auto now = GetTickCount64();
    if (nextRetryTick_ != 0 && now < nextRetryTick_) {
        return;
    }
    nextRetryTick_ = now + 30000;

    HMODULE module = LoadNvmlModule();
    if (!module) {
        return;
    }

    const auto init = GetFunction<Init>(module, "nvmlInit_v2");
    const auto countV2 = GetFunction<DeviceCount>(module, "nvmlDeviceGetCount_v2");
    const auto count = countV2 ? countV2 : GetFunction<DeviceCount>(module, "nvmlDeviceGetCount");
    const auto byIndexV2 = GetFunction<DeviceByIndex>(module, "nvmlDeviceGetHandleByIndex_v2");
    const auto byIndex = byIndexV2
        ? byIndexV2 : GetFunction<DeviceByIndex>(module, "nvmlDeviceGetHandleByIndex");
    if (!init || !count || !byIndex || init() != Success) {
        FreeLibrary(module);
        return;
    }

    unsigned int devices = 0;
    if (count(&devices) != Success || devices == 0 || byIndex(0, &device_) != Success) {
        if (const auto shutdown = GetFunction<Shutdown>(module, "nvmlShutdown")) {
            shutdown();
        }
        FreeLibrary(module);
        return;
    }

    module_ = module;
    temperatureFunction_ = GetFunction<Temperature>(module, "nvmlDeviceGetTemperature");
    utilizationFunction_ = GetFunction<UtilizationRates>(module, "nvmlDeviceGetUtilizationRates");
    memoryFunction_ = GetFunction<MemoryInfo>(module, "nvmlDeviceGetMemoryInfo");
    powerFunction_ = GetFunction<PowerUsage>(module, "nvmlDeviceGetPowerUsage");
    fanFunction_ = GetFunction<FanSpeed>(module, "nvmlDeviceGetFanSpeed");
    nextRetryTick_ = 0;
}

void GpuMonitor::Update(SensorSnapshot& snapshot, std::uint32_t demand) {
    Initialize();
    const auto now = GetTickCount64();
    if (module_ && device_) {
        if (HasSensorDemand(demand, DemandGpuTemperature)) {
            unsigned int value = 0;
            if (temperatureFunction_ && temperatureFunction_(device_, TemperatureSensor, &value) == Success && value < 150) {
                temperature_ = static_cast<double>(value);
                temperatureValid_ = true;
                temperatureSuccessTick_ = now;
            }
        }

        if (HasSensorDemand(demand, DemandGpuUsage)) {
            if (utilizationFunction_) {
                NvmlUtilization rates{};
                if (utilizationFunction_(device_, &rates) == Success && rates.gpu <= 100) {
                    snapshot.gpuUsage = static_cast<double>(rates.gpu);
                    snapshot.gpuUsageValid = true;
                }
            }
        }

        if (HasSensorDemand(demand, DemandVram)) {
            if (memoryFunction_) {
                NvmlMemory memory{};
                if (memoryFunction_(device_, &memory) == Success && memory.total > 0 && memory.used <= memory.total) {
                    snapshot.gpuMemoryUsedBytes = memory.used;
                    snapshot.gpuMemoryTotalBytes = memory.total;
                    snapshot.gpuMemoryValid = true;
                }
            }
        }

        if (HasSensorDemand(demand, DemandGpuPower)) {
            if (powerFunction_) {
                unsigned int milliwatts = 0;
                if (powerFunction_(device_, &milliwatts) == Success && milliwatts < 1000000u) {
                    snapshot.gpuPower = static_cast<double>(milliwatts) / 1000.0;
                    snapshot.gpuPowerValid = true;
                }
            }
        }

        if (HasSensorDemand(demand, DemandFan)) {
            if (fanFunction_) {
                unsigned int percent = 0;
                if (fanFunction_(device_, &percent) == Success && percent <= 100) {
                    snapshot.gpuFanPercent = static_cast<double>(percent);
                    snapshot.gpuFanValid = true;
                }
            }
        }
    }

    if (HasSensorDemand(demand, DemandGpuTemperature)) {
        if (temperatureSuccessTick_ == 0 || now - temperatureSuccessTick_ > 10000) {
            temperatureValid_ = false;
        }
        snapshot.gpuTemperature = temperature_;
        snapshot.gpuTemperatureValid = temperatureValid_;
    }
}

} // namespace monitor
