#include "Config.h"
#include "../monitor/SensorDemand.h"

#include <windows.h>
#include <taskschd.h>
#include <sddl.h>
#include <oleauto.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace monitor {
namespace {

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring FromUtf8(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

std::string JsonString(const std::wstring& value) {
    const auto utf8 = ToUtf8(value);
    std::string escaped;
    escaped.reserve(utf8.size());
    for (const char character : utf8) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

bool HasFalse(const std::string& text, const char* key) {
    return text.find(std::string("\"") + key + "\": false") != std::string::npos;
}

bool HasTrue(const std::string& text, const char* key) {
    return text.find(std::string("\"") + key + "\": true") != std::string::npos;
}

bool LoadIntValue(const std::string& text, const char* key, long& result) {
    const auto position = text.find(std::string("\"") + key + "\"");
    if (position == std::string::npos) return false;
    const auto colon = text.find(':', position);
    if (colon == std::string::npos) return false;
    char* end = nullptr;
    const long value = std::strtol(text.c_str() + colon + 1, &end, 10);
    if (end == text.c_str() + colon + 1) return false;
    result = value;
    return true;
}

void LoadStringValue(const std::string& text, const char* key, std::wstring& result) {
    const auto keyPosition = text.find(std::string("\"") + key + "\"");
    if (keyPosition == std::string::npos) return;
    const auto colon = text.find(':', keyPosition);
    const auto quote = colon == std::string::npos ? std::string::npos : text.find('"', colon + 1);
    if (quote == std::string::npos) return;

    std::string raw;
    for (std::size_t index = quote + 1; index < text.size(); ++index) {
        const char character = text[index];
        if (character == '"') {
            result = FromUtf8(raw);
            return;
        }
        if (character == '\\' && index + 1 < text.size()) {
            const char escaped = text[++index];
            if (escaped == '\\' || escaped == '"') raw.push_back(escaped);
            else if (escaped == 'n') raw.push_back('\n');
            else if (escaped == 'r') raw.push_back('\r');
            else if (escaped == 't') raw.push_back('\t');
            else {
                raw.push_back('\\');
                raw.push_back(escaped);
            }
            continue;
        }
        raw.push_back(character);
    }
}

template <typename T>
void SafeRelease(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

std::wstring CurrentUserSid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    if (!bytes) {
        CloseHandle(token);
        return {};
    }
    std::vector<BYTE> buffer(bytes);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), bytes, &bytes)) {
        CloseHandle(token);
        return {};
    }
    CloseHandle(token);

    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
    LPWSTR sidText = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sidText) || !sidText) return {};
    std::wstring result(sidText);
    LocalFree(sidText);
    return result;
}

void DeleteLegacyRunEntry() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, L"TaskbarHardwareMonitor");
        RegCloseKey(key);
    }
}

bool ConfigureStartupTask(bool enabled) {
    constexpr wchar_t TaskName[] = L"TaskbarHardwareMonitor";
    const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitialize = SUCCEEDED(initializeResult);
    if (FAILED(initializeResult) && initializeResult != RPC_E_CHANGED_MODE) return false;

    ITaskService* service = nullptr;
    ITaskFolder* root = nullptr;
    ITaskDefinition* definition = nullptr;
    IPrincipal* principal = nullptr;
    ITaskSettings* settings = nullptr;
    ITriggerCollection* triggers = nullptr;
    ITrigger* trigger = nullptr;
    ILogonTrigger* logonTrigger = nullptr;
    IActionCollection* actions = nullptr;
    IAction* action = nullptr;
    IExecAction* execAction = nullptr;
    IRegisteredTask* registeredTask = nullptr;
    BSTR taskName = nullptr;
    BSTR userId = nullptr;
    BSTR executable = nullptr;
    BSTR workingDirectoryValue = nullptr;
    VARIANT empty{};
    VARIANT userVariant{};
    VariantInit(&empty);
    VariantInit(&userVariant);

    auto cleanup = [&](bool success) {
        VariantClear(&userVariant);
        if (workingDirectoryValue) SysFreeString(workingDirectoryValue);
        if (executable) SysFreeString(executable);
        if (userId) SysFreeString(userId);
        if (taskName) SysFreeString(taskName);
        SafeRelease(registeredTask);
        SafeRelease(execAction);
        SafeRelease(action);
        SafeRelease(actions);
        SafeRelease(logonTrigger);
        SafeRelease(trigger);
        SafeRelease(triggers);
        SafeRelease(settings);
        SafeRelease(principal);
        SafeRelease(definition);
        SafeRelease(root);
        SafeRelease(service);
        if (uninitialize) CoUninitialize();
        return success;
    };

    HRESULT result = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&service));
    if (SUCCEEDED(result)) result = service->Connect(empty, empty, empty, empty);

    BSTR rootPath = SysAllocString(L"\\");
    if (SUCCEEDED(result) && !rootPath) result = E_OUTOFMEMORY;
    if (SUCCEEDED(result)) result = service->GetFolder(rootPath, &root);
    if (rootPath) SysFreeString(rootPath);
    if (FAILED(result)) return cleanup(false);

    taskName = SysAllocString(TaskName);
    if (!taskName) return cleanup(false);

    if (!enabled) {
        result = root->DeleteTask(taskName, 0);
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
            result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
            result = S_OK;
        }
        const bool success = SUCCEEDED(result);
        if (success) DeleteLegacyRunEntry();
        return cleanup(success);
    }

    result = service->NewTask(0, &definition);
    if (SUCCEEDED(result)) result = definition->get_Principal(&principal);

    const std::wstring userSid = CurrentUserSid();
    if (SUCCEEDED(result) && userSid.empty()) result = E_FAIL;
    if (SUCCEEDED(result)) userId = SysAllocString(userSid.c_str());
    if (SUCCEEDED(result) && !userId) result = E_OUTOFMEMORY;
    if (SUCCEEDED(result)) result = principal->put_UserId(userId);
    if (SUCCEEDED(result)) result = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if (SUCCEEDED(result)) result = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);

    if (SUCCEEDED(result)) result = definition->get_Settings(&settings);
    if (SUCCEEDED(result)) result = settings->put_StartWhenAvailable(VARIANT_TRUE);
    if (SUCCEEDED(result)) result = settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    if (SUCCEEDED(result)) result = settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    if (SUCCEEDED(result)) result = settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);

    if (SUCCEEDED(result)) result = definition->get_Triggers(&triggers);
    if (SUCCEEDED(result)) result = triggers->Create(TASK_TRIGGER_LOGON, &trigger);
    if (SUCCEEDED(result)) result = trigger->QueryInterface(IID_PPV_ARGS(&logonTrigger));
    if (SUCCEEDED(result)) result = logonTrigger->put_UserId(userId);

    if (SUCCEEDED(result)) result = definition->get_Actions(&actions);
    if (SUCCEEDED(result)) result = actions->Create(TASK_ACTION_EXEC, &action);
    if (SUCCEEDED(result)) result = action->QueryInterface(IID_PPV_ARGS(&execAction));

    wchar_t executablePath[MAX_PATH]{};
    if (SUCCEEDED(result) && !GetModuleFileNameW(nullptr, executablePath, MAX_PATH)) {
        result = HRESULT_FROM_WIN32(GetLastError());
    }
    if (SUCCEEDED(result)) executable = SysAllocString(executablePath);
    if (SUCCEEDED(result) && !executable) result = E_OUTOFMEMORY;
    if (SUCCEEDED(result)) result = execAction->put_Path(executable);

    std::wstring workingDirectory(executablePath);
    const auto separator = workingDirectory.find_last_of(L"\\/");
    if (separator != std::wstring::npos) workingDirectory.resize(separator);
    if (SUCCEEDED(result)) workingDirectoryValue = SysAllocString(workingDirectory.c_str());
    if (SUCCEEDED(result) && !workingDirectoryValue) result = E_OUTOFMEMORY;
    if (SUCCEEDED(result)) result = execAction->put_WorkingDirectory(workingDirectoryValue);

    if (SUCCEEDED(result)) {
        V_VT(&userVariant) = VT_BSTR;
        V_BSTR(&userVariant) = SysAllocString(userId);
        if (!V_BSTR(&userVariant)) result = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(result)) {
        result = root->RegisterTaskDefinition(
            taskName, definition, TASK_CREATE_OR_UPDATE, userVariant, empty,
            TASK_LOGON_INTERACTIVE_TOKEN, empty, &registeredTask);
    }

    const bool success = SUCCEEDED(result);
    if (success) DeleteLegacyRunEntry();
    return cleanup(success);
}

} // namespace

bool Config::Load() {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    const auto position = text.find("refresh_interval");
    if (position != std::string::npos) {
        const auto colon = text.find(':', position);
        if (colon != std::string::npos) {
            const long value = std::strtol(text.c_str() + colon + 1, nullptr, 10);
            if (value >= 250 && value <= 10000) refreshIntervalMs = static_cast<int>(value);
        }
    }
    startWithWindows = HasTrue(text, "start_with_windows");
    const auto taskbarPosition = text.find("\"taskbar\"");
    const auto enabledPosition = text.find("\"enabled\": false", taskbarPosition);
    taskbarEnabled = enabledPosition == std::string::npos;
    displayMode = text.find("\"display_mode\": \"compact\"") != std::string::npos
        ? DisplayMode::Compact : DisplayMode::Full;
    const auto rowsPosition = text.find("\"rows\"");
    if (rowsPosition != std::string::npos) {
        const auto colon = text.find(':', rowsPosition);
        if (colon != std::string::npos) {
            const long value = std::strtol(text.c_str() + colon + 1, nullptr, 10);
            taskbarRows = value == 2 ? 2 : 1;
        }
    }
    showCpuTemperature = !HasFalse(text, "show_cpu_temperature");
    showCpuUsage = HasTrue(text, "show_cpu_usage");
    showGpuTemperature = !HasFalse(text, "show_gpu_temperature");
    showDiskTemperature = !HasFalse(text, "show_disk_temperature");
    showNetwork = !HasFalse(text, "show_network");
    showPower = !HasFalse(text, "show_power");
    showMemory = HasTrue(text, "show_memory");
    showGpuUsage = HasTrue(text, "show_gpu_usage");
    showVram = HasTrue(text, "show_vram");
    showDiskIo = HasTrue(text, "show_disk_io");
    showCpuClock = HasTrue(text, "show_cpu_clock");
    showGpuPower = HasTrue(text, "show_gpu_power");
    showFan = HasTrue(text, "show_fan");
    showBattery = HasTrue(text, "show_battery");
    showSystemPower = HasTrue(text, "show_system_power");
    showCpuInternalTemperature = HasTrue(text, "show_cpu_internal_temperature");
    showGpuInternalTemperature = HasTrue(text, "show_gpu_internal_temperature");
    showCpuFanRpm = HasTrue(text, "show_cpu_fan_rpm");
    showGpuFanRpm = HasTrue(text, "show_gpu_fan_rpm");
    taskbarValueColorCustom = HasTrue(text, "value_color_custom");
    thresholdColorsEnabled = HasTrue(text, "threshold_colors_enabled");
    const auto colorPosition = text.find("\"value_color\"");
    if (colorPosition != std::string::npos) {
        const auto colon = text.find(':', colorPosition);
        if (colon != std::string::npos) {
            const unsigned long value = std::strtoul(text.c_str() + colon + 1, nullptr, 10);
            if (value <= 0x00FFFFFFul) taskbarValueColor = static_cast<unsigned int>(value);
        }
    }
    long thresholdValue = 0;
    if (LoadIntValue(text, "cpu_temp_warning", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 150)
        cpuTempWarning = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "cpu_temp_critical", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 150)
        cpuTempCritical = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "gpu_temp_warning", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 150)
        gpuTempWarning = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "gpu_temp_critical", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 150)
        gpuTempCritical = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "ram_warning", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 100)
        ramWarning = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "ram_critical", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 100)
        ramCritical = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "battery_warning", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 100)
        batteryWarning = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "battery_critical", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 100)
        batteryCritical = static_cast<int>(thresholdValue);
    if (LoadIntValue(text, "warning_color", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 0x00FFFFFFL)
        warningColor = static_cast<unsigned int>(thresholdValue);
    if (LoadIntValue(text, "critical_color", thresholdValue) && thresholdValue >= 0 && thresholdValue <= 0x00FFFFFFL)
        criticalColor = static_cast<unsigned int>(thresholdValue);
    LoadStringValue(text, "network_adapter", networkAdapter);
    const auto storageDrivePosition = text.find("\"storage_drive\"");
    if (storageDrivePosition != std::string::npos) {
        const auto colon = text.find(':', storageDrivePosition);
        if (colon != std::string::npos) {
            const long value = std::strtol(text.c_str() + colon + 1, nullptr, 10);
            if (value >= -1 && value < 32) storageDriveIndex = static_cast<int>(value);
        }
    }
    LoadStringValue(text, "font_name", taskbarFontName);
    LoadStringValue(text, "format", taskbarFormat);
    if (!taskbarFormat.empty()) {
        ApplySensorDemandToMetrics(*this, SensorDemandFromFormat(taskbarFormat));
    }
    const auto fontSizePosition = text.find("\"font_size\"");
    if (fontSizePosition != std::string::npos) {
        const auto colon = text.find(':', fontSizePosition);
        if (colon != std::string::npos) {
            const long value = std::strtol(text.c_str() + colon + 1, nullptr, 10);
            if (value >= 6 && value <= 32) taskbarFontSize = static_cast<int>(value);
        }
    }
    const auto fontWeightPosition = text.find("\"font_weight\"");
    if (fontWeightPosition != std::string::npos) {
        const auto colon = text.find(':', fontWeightPosition);
        if (colon != std::string::npos) {
            const long value = std::strtol(text.c_str() + colon + 1, nullptr, 10);
            if (value >= 100 && value <= 900) taskbarFontWeight = static_cast<int>(value);
        }
    }
    return true;
}

bool Config::ApplyStartupSetting() const {
    return ConfigureStartupTask(startWithWindows);
}

bool Config::SaveFile() const {
    if (path.empty()) return false;
    const std::string adapter = JsonString(networkAdapter);
    const std::string fontName = JsonString(taskbarFontName);
    const std::string taskbarFormatValue = JsonString(taskbarFormat);
    std::ostringstream text;
    text << "{\n  \"refresh_interval\": " << refreshIntervalMs
         << ",\n  \"taskbar\": {\n"
         << "    \"enabled\": " << (taskbarEnabled ? "true" : "false")
         << ",\n    \"display_mode\": \""
         << (displayMode == DisplayMode::Compact ? "compact" : "full")
         << "\",\n    \"rows\": " << (taskbarRows == 2 ? 2 : 1)
         << ",\n    \"show_cpu_temperature\": " << (showCpuTemperature ? "true" : "false")
         << ",\n    \"show_cpu_usage\": " << (showCpuUsage ? "true" : "false")
         << ",\n    \"show_gpu_temperature\": " << (showGpuTemperature ? "true" : "false")
         << ",\n    \"show_disk_temperature\": " << (showDiskTemperature ? "true" : "false")
         << ",\n    \"show_network\": " << (showNetwork ? "true" : "false")
         << ",\n    \"show_power\": " << (showPower ? "true" : "false")
         << ",\n    \"show_memory\": " << (showMemory ? "true" : "false")
         << ",\n    \"show_gpu_usage\": " << (showGpuUsage ? "true" : "false")
         << ",\n    \"show_vram\": " << (showVram ? "true" : "false")
         << ",\n    \"show_disk_io\": " << (showDiskIo ? "true" : "false")
         << ",\n    \"show_cpu_clock\": " << (showCpuClock ? "true" : "false")
         << ",\n    \"show_gpu_power\": " << (showGpuPower ? "true" : "false")
         << ",\n    \"show_fan\": " << (showFan ? "true" : "false")
         << ",\n    \"show_battery\": " << (showBattery ? "true" : "false")
    << ",\n    \"show_system_power\": " << (showSystemPower ? "true" : "false")
         << ",\n    \"show_cpu_internal_temperature\": " << (showCpuInternalTemperature ? "true" : "false")
         << ",\n    \"show_gpu_internal_temperature\": " << (showGpuInternalTemperature ? "true" : "false")
         << ",\n    \"show_cpu_fan_rpm\": " << (showCpuFanRpm ? "true" : "false")
         << ",\n    \"show_gpu_fan_rpm\": " << (showGpuFanRpm ? "true" : "false")
         << ",\n    \"value_color_custom\": " << (taskbarValueColorCustom ? "true" : "false")
         << ",\n    \"value_color\": " << taskbarValueColor
         << ",\n    \"threshold_colors_enabled\": " << (thresholdColorsEnabled ? "true" : "false")
         << ",\n    \"cpu_temp_warning\": " << cpuTempWarning
         << ",\n    \"cpu_temp_critical\": " << cpuTempCritical
         << ",\n    \"gpu_temp_warning\": " << gpuTempWarning
         << ",\n    \"gpu_temp_critical\": " << gpuTempCritical
         << ",\n    \"ram_warning\": " << ramWarning
         << ",\n    \"ram_critical\": " << ramCritical
         << ",\n    \"battery_warning\": " << batteryWarning
         << ",\n    \"battery_critical\": " << batteryCritical
         << ",\n    \"warning_color\": " << warningColor
         << ",\n    \"critical_color\": " << criticalColor
         << ",\n    \"font_name\": \"" << fontName << "\""
         << ",\n    \"font_size\": " << taskbarFontSize
         << ",\n    \"font_weight\": " << taskbarFontWeight
         << ",\n    \"format\": \"" << taskbarFormatValue << "\""
         << ",\n    \"fixed_width\": true,\n    \"tabular_numbers\": true\n  },\n"
         << "  \"network_adapter\": \"" << adapter
         << "\",\n  \"storage_drive\": " << storageDriveIndex
         << ",\n  \"start_with_windows\": "
         << (startWithWindows ? "true" : "false") << "\n}\n";
    if (!text.good()) return false;

    const std::wstring temporary = path + L".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file << text.str();
        file.flush();
        if (!file.good()) {
            file.close();
            DeleteFileW(temporary.c_str());
            return false;
        }
    }

    if (ReplaceFileW(path.c_str(), temporary.c_str(), nullptr,
                     REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
        return true;
    }
    const DWORD replaceError = GetLastError();
    if (replaceError == ERROR_FILE_NOT_FOUND &&
        MoveFileExW(temporary.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    DeleteFileW(temporary.c_str());
    return false;
}

bool Config::Save() const {
    const bool fileSaved = SaveFile();
    return fileSaved && ApplyStartupSetting();
}

} // namespace monitor
