#include "TaskbarBand.h"

#include "../ui/TaskbarLayout.h"

#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <winreg.h>
#include <fstream>

namespace monitor {
void ModuleAddObject();
void ModuleReleaseObject();

namespace {
void BandLog(const wchar_t* message) {
    wchar_t directory[MAX_PATH]{};
    if (!GetTempPathW(ARRAYSIZE(directory), directory)) return;
    const std::wstring logPath = std::wstring(directory) + L"TaskbarBand-debug.log";
    std::wofstream log(logPath, std::ios::app);
    if (log) log << GetTickCount64() << L" " << message << L"\n";
}

constexpr UINT CommandDisplayFull = 1001;
constexpr UINT CommandDisplayCompact = 1002;
constexpr UINT CommandRowsOne = 1003;
constexpr UINT CommandRowsTwo = 1004;
constexpr UINT CommandCpu = 1010;
constexpr UINT CommandCpuUsage = 1011;
constexpr UINT CommandGpu = 1012;
constexpr UINT CommandSsd = 1013;
constexpr UINT CommandNetwork = 1014;
constexpr wchar_t BandSettingsPath[] = L"Software\\TaskbarHardwareMonitor\\TaskbarBand";

bool SameVisualSnapshot(const SharedSensorSnapshot& a, const SharedSensorSnapshot& b) {
    return a.version == b.version &&
           a.cpuTemperature == b.cpuTemperature &&
           a.cpuUsage == b.cpuUsage &&
           a.gpuTemperature == b.gpuTemperature &&
           a.diskTemperature == b.diskTemperature &&
           a.cpuPower == b.cpuPower &&
           a.memoryUsage == b.memoryUsage &&
           a.cpuClockMHz == b.cpuClockMHz &&
           a.gpuUsage == b.gpuUsage &&
           a.gpuPower == b.gpuPower &&
           a.gpuFanPercent == b.gpuFanPercent &&
           a.batteryPercent == b.batteryPercent &&
           a.systemPower == b.systemPower &&
           a.cpuInternalTemperature == b.cpuInternalTemperature &&
           a.gpuInternalTemperature == b.gpuInternalTemperature &&
           a.cpuFanRpm == b.cpuFanRpm &&
           a.gpuFanRpm == b.gpuFanRpm &&
           a.memoryUsedBytes == b.memoryUsedBytes &&
           a.memoryTotalBytes == b.memoryTotalBytes &&
           a.gpuMemoryUsedBytes == b.gpuMemoryUsedBytes &&
           a.gpuMemoryTotalBytes == b.gpuMemoryTotalBytes &&
           a.diskReadBytesPerSecond == b.diskReadBytesPerSecond &&
           a.diskWriteBytesPerSecond == b.diskWriteBytesPerSecond &&
           a.batteryState == b.batteryState &&
           a.downloadBytesPerSecond == b.downloadBytesPerSecond &&
           a.uploadBytesPerSecond == b.uploadBytesPerSecond &&
           a.validMask == b.validMask &&
           a.displayMode == b.displayMode &&
           a.displayFlags == b.displayFlags;
}

constexpr UINT CommandPower = 1015;
constexpr UINT CommandMemory = 1016;
constexpr UINT CommandGpuUsage = 1017;
constexpr UINT CommandVram = 1018;
constexpr UINT CommandDiskIo = 1019;
constexpr UINT CommandCpuClock = 1020;
constexpr UINT CommandGpuPower = 1021;
constexpr UINT CommandFan = 1022;
constexpr UINT CommandBattery = 1023;
constexpr UINT CommandSystemPower = 1024;
constexpr UINT CommandCpuInternalTemperature = 1025;
constexpr UINT CommandGpuInternalTemperature = 1026;
constexpr UINT CommandCpuFanRpm = 1027;
constexpr UINT CommandGpuFanRpm = 1028;
constexpr UINT CommandSettings = 1040;
constexpr UINT CommandExit = 1041;
constexpr UINT SnapshotEventMessage = WM_APP + 20;
constexpr UINT SettingsEventMessage = WM_APP + 21;

bool IsSafeMessage(UINT message) {
    return message == WM_PAINT || message == WM_ERASEBKGND ||
           message == WM_RBUTTONUP || message == WM_CONTEXTMENU ||
           message == SnapshotEventMessage || message == SettingsEventMessage;
}

void SetBandWidth(HWND window, int width) {
    if (!window || width < 1) return;
    const HWND rebar = GetParent(window);
    if (!rebar) return;

    const int bandCount = static_cast<int>(SendMessageW(rebar, RB_GETBANDCOUNT, 0, 0));
    for (int index = 0; index < bandCount; ++index) {
        REBARBANDINFOW current{};
        current.cbSize = sizeof(current);
        current.fMask = RBBIM_CHILD;
        if (!SendMessageW(rebar, RB_GETBANDINFOW, index,
                          reinterpret_cast<LPARAM>(&current)) || current.hwndChild != window) {
            continue;
        }

        RECT client{};
        GetClientRect(window, &client);
        const UINT height = static_cast<UINT>(std::max<LONG>(1, client.bottom - client.top));
        REBARBANDINFOW update{};
        update.cbSize = sizeof(update);
        update.fMask = RBBIM_CHILDSIZE | RBBIM_IDEALSIZE | RBBIM_SIZE;
        update.cxMinChild = static_cast<UINT>(width);
        update.cyMinChild = height;
        update.cxIdeal = static_cast<UINT>(width);
        update.cx = static_cast<UINT>(width);
        SendMessageW(rebar, RB_SETBANDINFOW, index, reinterpret_cast<LPARAM>(&update));
        SendMessageW(rebar, RB_SETBANDWIDTH, index, width);
        InvalidateRect(rebar, nullptr, TRUE);
        return;
    }
}

int MeasureTextWidth(HDC dc, const std::wstring& text) {
    if (!dc || text.empty()) return 0;
    SIZE size{};
    if (!GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size)) {
        return 0;
    }
    return size.cx;
}

bool IsDarkTaskbarTheme() {
    HKEY key = nullptr;
    const auto result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_QUERY_VALUE, &key);
    if (result != ERROR_SUCCESS) return false;

    DWORD value = 1;
    DWORD size = sizeof(value);
    const auto query = RegQueryValueExW(
        key, L"SystemUsesLightTheme", nullptr, nullptr,
        reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return query == ERROR_SUCCESS && value == 0;
}

int LoadLayoutRows() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return 1;
    }
    DWORD value = 1;
    DWORD type = 0;
    DWORD size = sizeof(value);
    const auto status = RegQueryValueExW(key, L"Rows", nullptr, &type,
                                         reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_DWORD && value == 2 ? 2 : 1;
}

void SaveLayoutRows(int rows) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, &disposition) != ERROR_SUCCESS) {
        return;
    }
    const DWORD value = rows == 2 ? 2u : 1u;
    RegSetValueExW(key, L"Rows", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

void LoadFontSettings(LOGFONTW& font, int dpi) {
    font = {};
    font.lfHeight = -MulDiv(11, dpi, 96);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    lstrcpyW(font.lfFaceName, L"Segoe UI");

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }

    wchar_t face[LF_FACESIZE]{};
    DWORD faceType = 0;
    DWORD faceBytes = sizeof(face);
    if (RegQueryValueExW(key, L"FontName", nullptr, &faceType,
                         reinterpret_cast<BYTE*>(face), &faceBytes) == ERROR_SUCCESS &&
        (faceType == REG_SZ || faceType == REG_EXPAND_SZ) && face[0] != L'\0') {
        lstrcpynW(font.lfFaceName, face, LF_FACESIZE);
    }

    DWORD sizeValue = 11;
    DWORD sizeType = 0;
    DWORD sizeBytes = sizeof(sizeValue);
    if (RegQueryValueExW(key, L"FontSize", nullptr, &sizeType,
                         reinterpret_cast<BYTE*>(&sizeValue), &sizeBytes) == ERROR_SUCCESS &&
        sizeType == REG_DWORD && sizeValue >= 6 && sizeValue <= 32) {
        font.lfHeight = -MulDiv(static_cast<int>(sizeValue), dpi, 96);
    }

    DWORD weightValue = FW_NORMAL;
    DWORD weightType = 0;
    DWORD weightBytes = sizeof(weightValue);
    if (RegQueryValueExW(key, L"FontWeight", nullptr, &weightType,
                         reinterpret_cast<BYTE*>(&weightValue), &weightBytes) == ERROR_SUCCESS &&
        weightType == REG_DWORD && weightValue >= 100 && weightValue <= 900) {
        font.lfWeight = static_cast<LONG>(weightValue);
    }
    RegCloseKey(key);
}

std::wstring LoadFormatTemplate() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return {};
    }
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, L"Format", nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        RegCloseKey(key);
        return {};
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, L"Format", nullptr, &type,
                         reinterpret_cast<BYTE*>(value.data()), &bytes) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return {};
    }
    RegCloseKey(key);
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

COLORREF LoadValueColor(bool darkTheme) {
    const COLORREF automatic = darkTheme ? RGB(245, 245, 245) : GetSysColor(COLOR_BTNTEXT);
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return automatic;
    }
    DWORD custom = 0;
    DWORD customType = 0;
    DWORD customSize = sizeof(custom);
    const auto customStatus = RegQueryValueExW(
        key, L"ValueColorCustom", nullptr, &customType,
        reinterpret_cast<BYTE*>(&custom), &customSize);
    if (customStatus != ERROR_SUCCESS || customType != REG_DWORD || custom == 0) {
        RegCloseKey(key);
        return automatic;
    }
    DWORD color = static_cast<DWORD>(automatic);
    DWORD colorType = 0;
    DWORD colorSize = sizeof(color);
    const auto colorStatus = RegQueryValueExW(
        key, L"ValueColor", nullptr, &colorType,
        reinterpret_cast<BYTE*>(&color), &colorSize);
    RegCloseKey(key);
    return colorStatus == ERROR_SUCCESS && colorType == REG_DWORD
        ? static_cast<COLORREF>(color & 0x00FFFFFFu)
        : automatic;
}

void LoadAlertSettings(Config& config) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }
    auto readDword = [&](const wchar_t* name, DWORD& value) {
        DWORD type = 0;
        DWORD size = sizeof(value);
        return RegQueryValueExW(key, name, nullptr, &type,
                                reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS &&
               type == REG_DWORD;
    };
    DWORD value = 0;
    if (readDword(L"ThresholdColorsEnabled", value)) config.thresholdColorsEnabled = value != 0;
    if (readDword(L"CpuTempWarning", value) && value <= 150) config.cpuTempWarning = static_cast<int>(value);
    if (readDword(L"CpuTempCritical", value) && value <= 150) config.cpuTempCritical = static_cast<int>(value);
    if (readDword(L"GpuTempWarning", value) && value <= 150) config.gpuTempWarning = static_cast<int>(value);
    if (readDword(L"GpuTempCritical", value) && value <= 150) config.gpuTempCritical = static_cast<int>(value);
    if (readDword(L"RamWarning", value) && value <= 100) config.ramWarning = static_cast<int>(value);
    if (readDword(L"RamCritical", value) && value <= 100) config.ramCritical = static_cast<int>(value);
    if (readDword(L"BatteryWarning", value) && value <= 100) config.batteryWarning = static_cast<int>(value);
    if (readDword(L"BatteryCritical", value) && value <= 100) config.batteryCritical = static_cast<int>(value);
    if (readDword(L"WarningColor", value)) config.warningColor = value & 0x00FFFFFFu;
    if (readDword(L"CriticalColor", value)) config.criticalColor = value & 0x00FFFFFFu;
    RegCloseKey(key);
}
}

TaskbarBand::TaskbarBand() { ModuleAddObject(); }
TaskbarBand::~TaskbarBand() {
    SafeClose();
    ModuleReleaseObject();
}

HRESULT STDMETHODCALLTYPE TaskbarBand::QueryInterface(REFIID iid, void** result) {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (iid == IID_IUnknown || iid == IID_IDeskBand || iid == IID_IDeskBand2) {
        *result = static_cast<IDeskBand2*>(this);
    } else if (iid == IID_IObjectWithSite) {
        *result = static_cast<IObjectWithSite*>(this);
    } else if (iid == IID_IPersist || iid == IID_IPersistStream) {
        *result = static_cast<IPersistStream*>(this);
    } else if (iid == IID_IInputObject) {
        *result = static_cast<IInputObject*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}
ULONG STDMETHODCALLTYPE TaskbarBand::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&refs_));
}

ULONG STDMETHODCALLTYPE TaskbarBand::Release() {
    const auto count = static_cast<ULONG>(InterlockedDecrement(&refs_));
    if (!count) delete this;
    return count;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::GetWindow(HWND* window) {
    if (!window) return E_POINTER;
    *window = hwnd_;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE TaskbarBand::ContextSensitiveHelp(BOOL) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::ShowDW(BOOL show) {
    shellShowRequested_ = show != FALSE;
    if (hwnd_) {
        const bool shouldShow = shellShowRequested_ && monitorReady_ && monitorEnabled_;
        ShowWindow(hwnd_, shouldShow ? SW_SHOW : SW_HIDE);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::CloseDW(DWORD) {
    SafeClose();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::ResizeBorderDW(const RECT*, IUnknown*, BOOL) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::CanRenderComposited(BOOL* result) {
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::SetCompositionState(BOOL) {
    return S_OK;
}
HRESULT STDMETHODCALLTYPE TaskbarBand::GetCompositionState(BOOL* result) {
    if (!result) return E_POINTER;
    *result = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::GetBandInfo(DWORD, DWORD, DESKBANDINFO* info) {
    if (!info) return E_POINTER;
    if (idealSize_.cx < 1) idealSize_.cx = 428;
    wchar_t diagnostics[128]{};
    wsprintfW(diagnostics, L"GetBandInfo mask=%lu width=%ld", info->dwMask, idealSize_.cx);
    BandLog(diagnostics);
    if (info->dwMask & DBIM_MODEFLAGS) {
        info->dwModeFlags = DBIMF_NORMAL | DBIMF_VARIABLEHEIGHT;
    }
    RECT client{};
    LONG height = 0;
    if (hwnd_) {
        const HWND parent = GetParent(hwnd_);
        if (parent && GetClientRect(parent, &client)) {
            height = client.bottom - client.top;
        }
        if (height <= 1 && GetClientRect(hwnd_, &client)) {
            height = client.bottom - client.top;
        }
    }
    if (height < 1) height = std::max<LONG>(1, idealSize_.cy);
    idealSize_.cy = height;
    if (info->dwMask & DBIM_MINSIZE) info->ptMinSize = POINTL{idealSize_.cx, height};
    if (info->dwMask & DBIM_MAXSIZE) info->ptMaxSize = POINTL{idealSize_.cx, height};
    if (info->dwMask & DBIM_INTEGRAL) info->ptIntegral = POINTL{1, 1};
    if (info->dwMask & DBIM_ACTUAL) info->ptActual = POINTL{idealSize_.cx, height};
    if (info->dwMask & DBIM_TITLE) info->wszTitle[0] = L'\0';
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::SetSite(IUnknown* site) {
    BandLog(site ? L"SetSite begin" : L"SetSite null");
    SafeClose();
    if (site) { site_ = site; site_->AddRef(); }
    if (!site) return S_OK;
    IOleWindow* ole = nullptr;
    if (FAILED(site->QueryInterface(IID_PPV_ARGS(&ole)))) {
        BandLog(L"SetSite: IOleWindow query failed");
        SafeClose();
        return E_NOINTERFACE;
    }
    HWND parent{};
    const HRESULT windowStatus = ole->GetWindow(&parent);
    ole->Release();
    if (FAILED(windowStatus) || !parent) {
        BandLog(L"SetSite: GetWindow failed");
        SafeClose();
        return E_FAIL;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(L"TaskbarBand.dll");
    wc.lpszClassName = L"TaskbarHardwareMonitorDeskBand";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    RECT hostClient{};
    LONG hostHeight = 1;
    if (GetClientRect(parent, &hostClient)) {
        hostHeight = std::max<LONG>(1, hostClient.bottom - hostClient.top);
    }
    idealSize_.cy = hostHeight;
    // Explorer can load the DeskBand before the monitor process has published data.
    // Keep the band hidden and effectively widthless until the first valid snapshot arrives.
    idealSize_.cx = 1;
    monitorReady_ = false;
    monitorEnabled_ = false;
    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"Hardware Monitor",
                            WS_CHILD, 0, 0, idealSize_.cx, hostHeight,
                            parent, nullptr, wc.hInstance, this);
    if (!hwnd_) {
        BandLog(L"SetSite: CreateWindowEx failed");
        const HRESULT error = HRESULT_FROM_WIN32(GetLastError());
        SafeClose();
        return error;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    tooltip_ = CreateWindowExW(
        0, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd_, nullptr, wc.hInstance, nullptr);
    if (tooltip_) {
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = hwnd_;
        tool.lpszText = const_cast<wchar_t*>(L"Hardware Monitor");
        tool.uId = reinterpret_cast<UINT_PTR>(hwnd_);
        SendMessageW(tooltip_, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&tool));
    }
    ConnectSharedMemory();
    ConnectSettingsNotifications();
    SetBandWidth(hwnd_, 1);
    RefreshSnapshotState(hwnd_, true);
    BandLog(L"SetSite complete");
    return S_OK;
}

std::wstring FormatTooltipSpeed(std::uint64_t bytesPerSecond) {
    const double value = static_cast<double>(bytesPerSecond);
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(2);
    if (bytesPerSecond >= 1024ULL * 1024ULL * 1024ULL) {
        stream << value / (1024.0 * 1024.0 * 1024.0) << L" GB/s";
    } else if (bytesPerSecond >= 1024ULL * 1024ULL) {
        stream << value / (1024.0 * 1024.0) << L" MB/s";
    } else {
        stream << value / 1024.0 << L" KB/s";
    }
    return stream.str();
}

std::wstring FormatTooltipBytes(std::uint64_t bytes) {
    std::wstringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << L" GB";
    } else {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << L" MB";
    }
    return stream.str();
}

void UpdateTooltip(HWND tooltip, HWND band, const SensorSnapshot& snapshot,
                   std::wstring& text) {
    if (!tooltip) return;
    text = L"CPU Temperature: " +
        monitor::FormatTemperature(snapshot.cpuTemperature, snapshot.cpuTemperatureValid) + L"C";
    text += L"\nGPU Temperature: " +
        monitor::FormatTemperature(snapshot.gpuTemperature, snapshot.gpuTemperatureValid) + L"C";
    text += L"\nDisk Temperature: " +
        monitor::FormatTemperature(snapshot.diskTemperature, snapshot.diskTemperatureValid) + L"C";
    text += L"\nCPU Power: " + (snapshot.cpuPowerValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.cpuPower))) + L" W" : L"-- W");
    text += L"\nCPU Usage: " + (snapshot.cpuUsageValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.cpuUsage))) + L"%" : L"--%");
    text += L"\nDownload: " + (snapshot.networkValid
        ? FormatTooltipSpeed(snapshot.downloadBytesPerSecond) : L"-- MB/s");
    text += L"\nUpload: " + (snapshot.networkValid
        ? FormatTooltipSpeed(snapshot.uploadBytesPerSecond) : L"-- MB/s");
    text += L"\nRAM: " + (snapshot.memoryValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.memoryUsage))) + L"% (" +
          FormatTooltipBytes(snapshot.memoryUsedBytes) + L" / " + FormatTooltipBytes(snapshot.memoryTotalBytes) + L")"
        : L"--");
    text += L"\nCPU Clock: " + (snapshot.cpuClockValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.cpuClockMHz))) + L" MHz" : L"--");
    text += L"\nGPU Usage: " + (snapshot.gpuUsageValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.gpuUsage))) + L"%" : L"--%");
    text += L"\nVRAM: " + (snapshot.gpuMemoryValid
        ? FormatTooltipBytes(snapshot.gpuMemoryUsedBytes) + L" / " + FormatTooltipBytes(snapshot.gpuMemoryTotalBytes)
        : L"--");
    text += L"\nGPU Power: " + (snapshot.gpuPowerValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.gpuPower))) + L" W" : L"-- W");
    text += L"\nGPU Fan: " + (snapshot.gpuFanValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.gpuFanPercent))) + L"%" : L"--%");
    text += L"\nDisk Read: " + (snapshot.diskIoValid
        ? FormatTooltipSpeed(snapshot.diskReadBytesPerSecond) : L"-- MB/s");
    text += L"\nDisk Write: " + (snapshot.diskIoValid
        ? FormatTooltipSpeed(snapshot.diskWriteBytesPerSecond) : L"-- MB/s");
    text += L"\nBattery: " + (snapshot.batteryValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.batteryPercent))) + L"% (" +
          FormatBatteryStatus(snapshot.batteryState) + L")"
        : L"--%");
    text += L"\nSystem Power: " + (snapshot.systemPowerValid
        ? std::to_wstring(static_cast<int>(std::lround(snapshot.systemPower))) + L" W (battery discharge)" : L"--");
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_IDISHWND;
    tool.hwnd = band;
    tool.lpszText = const_cast<wchar_t*>(text.c_str());
    tool.uId = reinterpret_cast<UINT_PTR>(band);
    SendMessageW(tooltip, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&tool));
}
HRESULT STDMETHODCALLTYPE TaskbarBand::GetSite(REFIID iid, void** result) {
    return site_ ? site_->QueryInterface(iid, result) : E_FAIL;
}
HRESULT STDMETHODCALLTYPE TaskbarBand::GetClassID(CLSID* result) {
    if (!result) return E_POINTER;
    *result = {0x65c3a923, 0x7a8e, 0x4a54, {0x9e, 0x3a, 0x22, 0xc8, 0xe2, 0x5a, 0x9d, 0x01}};
    return S_OK;
}
HRESULT STDMETHODCALLTYPE TaskbarBand::IsDirty() { return S_FALSE; }
HRESULT STDMETHODCALLTYPE TaskbarBand::Load(IStream*) { return S_OK; }
HRESULT STDMETHODCALLTYPE TaskbarBand::Save(IStream*, BOOL) { return S_OK; }

HRESULT STDMETHODCALLTYPE TaskbarBand::GetSizeMax(ULARGE_INTEGER* result) {
    if (!result) return E_POINTER;
    result->QuadPart = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TaskbarBand::UIActivateIO(BOOL, LPMSG) { return S_OK; }
HRESULT STDMETHODCALLTYPE TaskbarBand::HasFocusIO() { return S_FALSE; }
HRESULT STDMETHODCALLTYPE TaskbarBand::TranslateAcceleratorIO(LPMSG) { return S_FALSE; }

void TaskbarBand::SafeClose() {
    if (snapshotWait_) {
        UnregisterWaitEx(snapshotWait_, INVALID_HANDLE_VALUE);
        snapshotWait_ = nullptr;
    }
    if (settingsWait_) {
        UnregisterWaitEx(settingsWait_, INVALID_HANDLE_VALUE);
        settingsWait_ = nullptr;
    }
    if (settingsEvent_) CloseHandle(settingsEvent_);
    settingsEvent_ = nullptr;
    if (settingsKey_) RegCloseKey(settingsKey_);
    settingsKey_ = nullptr;
    if (tooltip_) DestroyWindow(tooltip_);
    tooltip_ = nullptr;
    if (shared_) UnmapViewOfFile(shared_);
    shared_ = nullptr;
    if (mapping_) CloseHandle(mapping_);
    mapping_ = nullptr;
    if (command_) UnmapViewOfFile(command_);
    command_ = nullptr;
    if (commandMapping_) CloseHandle(commandMapping_);
    commandMapping_ = nullptr;
    if (snapshotEvent_) CloseHandle(snapshotEvent_);
    snapshotEvent_ = nullptr;
    if (commandEvent_) CloseHandle(commandEvent_);
    commandEvent_ = nullptr;
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    if (site_) site_->Release();
    site_ = nullptr;
    tooltipText_.clear();
    monitorReady_ = false;
    monitorEnabled_ = false;
    hasLastVisualSnapshot_ = false;
}

void TaskbarBand::Paint(HDC dc) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;
    HDC buffer = CreateCompatibleDC(dc);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(dc, width, height) : nullptr;
    if (!buffer || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
        return;
    }
    const HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
    const COLORREF background = IsDarkTaskbarTheme()
        ? RGB(32, 32, 32) : GetSysColor(COLOR_3DFACE);
    const bool darkTheme = IsDarkTaskbarTheme();
    const COLORREF labelColor = darkTheme ? RGB(170, 170, 170) : RGB(96, 96, 96);
    const COLORREF valueColor = LoadValueColor(darkTheme);
    HBRUSH backgroundBrush = CreateSolidBrush(background);
    if (backgroundBrush) {
        FillRect(buffer, &client, backgroundBrush);
        DeleteObject(backgroundBrush);
    }
    SetBkMode(buffer, TRANSPARENT);
    SetTextColor(buffer, valueColor);
    LOGFONTW fontDescription{};
    HDC screen = GetDC(hwnd_);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) ReleaseDC(hwnd_, screen);
    LoadFontSettings(fontDescription, dpi);
    HFONT font = CreateFontIndirectW(&fontDescription);
    const HGDIOBJ oldFont = font ? SelectObject(buffer, font) : nullptr;
    SharedSensorSnapshot sharedSnapshot{};
    const bool hasSnapshot = ReadSnapshot(sharedSnapshot);
    const auto snapshot = hasSnapshot ? FromSharedSnapshot(sharedSnapshot) : SensorSnapshot{};
    UpdateTooltip(tooltip_, hwnd_, snapshot, tooltipText_);
    Config config;
    config.taskbarEnabled = true;
    if (hasSnapshot) {
        config.taskbarEnabled = (sharedSnapshot.displayFlags & TaskbarEnabled) != 0;
        config.displayMode = sharedSnapshot.displayMode ? DisplayMode::Compact : DisplayMode::Full;
        config.showCpuTemperature = (sharedSnapshot.displayFlags & ShowCpuTemperature) != 0;
        config.showCpuUsage = (sharedSnapshot.displayFlags & ShowCpuUsage) != 0;
        config.showGpuTemperature = (sharedSnapshot.displayFlags & ShowGpuTemperature) != 0;
        config.showDiskTemperature = (sharedSnapshot.displayFlags & ShowDiskTemperature) != 0;
        config.showNetwork = (sharedSnapshot.displayFlags & ShowNetwork) != 0;
        config.showPower = (sharedSnapshot.displayFlags & ShowPower) != 0;
        config.showMemory = (sharedSnapshot.displayFlags & ShowMemory) != 0;
        config.showGpuUsage = (sharedSnapshot.displayFlags & ShowGpuUsage) != 0;
        config.showVram = (sharedSnapshot.displayFlags & ShowVram) != 0;
        config.showDiskIo = (sharedSnapshot.displayFlags & ShowDiskIo) != 0;
        config.showCpuClock = (sharedSnapshot.displayFlags & ShowCpuClock) != 0;
        config.showGpuPower = (sharedSnapshot.displayFlags & ShowGpuPower) != 0;
        config.showFan = (sharedSnapshot.displayFlags & ShowFan) != 0;
        config.showBattery = (sharedSnapshot.displayFlags & ShowBattery) != 0;
        config.showSystemPower = (sharedSnapshot.displayFlags & ShowSystemPower) != 0;
        config.showCpuInternalTemperature = (sharedSnapshot.displayFlags & ShowCpuInternalTemperature) != 0;
        config.showGpuInternalTemperature = (sharedSnapshot.displayFlags & ShowGpuInternalTemperature) != 0;
        config.showCpuFanRpm = (sharedSnapshot.displayFlags & ShowCpuFanRpm) != 0;
        config.showGpuFanRpm = (sharedSnapshot.displayFlags & ShowGpuFanRpm) != 0;
    }
    if (!hasSnapshot) {
        config.showCpuTemperature = true;
        config.showCpuUsage = false;
        config.showGpuTemperature = true;
        config.showDiskTemperature = true;
        config.showNetwork = true;
        config.showPower = true;
    }
    config.taskbarRows = LoadLayoutRows();
    LoadAlertSettings(config);
    const std::wstring customFormat = LoadFormatTemplate();
    if (!config.taskbarEnabled) {
        ShowWindow(hwnd_, SW_HIDE);
        if (oldFont) SelectObject(buffer, oldFont);
        if (font) DeleteObject(font);
        if (oldBitmap) SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        return;
    }
    ShowWindow(hwnd_, SW_SHOW);
    if (!customFormat.empty()) {
        const auto formatted = BuildFormattedTaskbarLayout(snapshot, customFormat, &config);
        constexpr int outerPadding = 2;
        constexpr int columnGap = 6;
        int measuredRowWidths[2]{};
        std::vector<int> columnWidths(std::max(1, formatted.columns), 0);
        std::vector<int> rowColumnWidths[2] = {
            std::vector<int>(columnWidths.size(), 0),
            std::vector<int>(columnWidths.size(), 0)};

        for (const auto& run : formatted.runs) {
            const int row = run.row == 1 ? 1 : 0;
            const int column = std::max(0, std::min(run.column,
                static_cast<int>(columnWidths.size()) - 1));
            const int runWidth = MeasureTextWidth(buffer, run.stableText);
            measuredRowWidths[row] += runWidth;
            rowColumnWidths[row][column] += runWidth;
        }

        int measuredContentWidth = 0;
        if (formatted.columns > 1) {
            for (std::size_t column = 0; column < columnWidths.size(); ++column) {
                columnWidths[column] = std::max(rowColumnWidths[0][column], rowColumnWidths[1][column]);
                measuredContentWidth += columnWidths[column];
            }
            measuredContentWidth += (static_cast<int>(columnWidths.size()) - 1) * columnGap;
        } else {
            measuredContentWidth = std::max(measuredRowWidths[0], measuredRowWidths[1]);
        }

        const int neededWidth = std::max(1, measuredContentWidth + outerPadding * 2);
        UpdateBandSize(neededWidth);
        if (width < neededWidth) {
            if (oldFont) SelectObject(buffer, oldFont);
            if (font) DeleteObject(font);
            if (oldBitmap) SelectObject(buffer, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(buffer);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        const int rowHeight = std::max(1, height / formatted.rows);
        for (int row = 0; row < formatted.rows; ++row) {
            const int top = row * rowHeight;
            const int bottom = row == formatted.rows - 1 ? height : top + rowHeight;

            if (formatted.columns > 1) {
                int x = std::max(outerPadding, width - outerPadding - measuredContentWidth);
                for (int column = 0; column < formatted.columns; ++column) {
                    int runX = x;
                    for (const auto& run : formatted.runs) {
                        if (run.row != row || run.column != column) continue;
                        const int runWidth = MeasureTextWidth(buffer, run.stableText);
                        RECT rect{runX, top, runX + runWidth, bottom};
                        COLORREF runColor = run.value ? valueColor : labelColor;
                        if (run.value && run.severity == AlertSeverity::Warning)
                            runColor = static_cast<COLORREF>(config.warningColor);
                        else if (run.value && run.severity == AlertSeverity::Critical)
                            runColor = static_cast<COLORREF>(config.criticalColor);
                        SetTextColor(buffer, runColor);
                        DrawTextW(buffer, run.text.c_str(), -1, &rect,
                                  DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
                        runX += runWidth;
                    }
                    x += columnWidths[column];
                    if (column + 1 < formatted.columns) x += columnGap;
                }
            } else {
                int x = std::max(outerPadding, width - outerPadding - measuredRowWidths[row]);
                for (const auto& run : formatted.runs) {
                    if (run.row != row) continue;
                    const int runWidth = MeasureTextWidth(buffer, run.stableText);
                    RECT rect{x, top, x + runWidth, bottom};
                    COLORREF runColor = run.value ? valueColor : labelColor;
                    if (run.value && run.severity == AlertSeverity::Warning)
                        runColor = static_cast<COLORREF>(config.warningColor);
                    else if (run.value && run.severity == AlertSeverity::Critical)
                        runColor = static_cast<COLORREF>(config.criticalColor);
                    SetTextColor(buffer, runColor);
                    DrawTextW(buffer, run.text.c_str(), -1, &rect,
                              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
                    x += runWidth;
                }
            }
        }
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        if (oldFont) SelectObject(buffer, oldFont);
        if (font) DeleteObject(font);
        if (oldBitmap) SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        return;
    }

    const auto layout = BuildTaskbarLayout(snapshot, config);
    constexpr int outerPadding = 2;
    constexpr int labelValueGap = 4;
    constexpr int cellGap = 5;
    int measuredRowWidths[2]{};
    int rowCellCounts[2]{};
    for (const auto& cell : layout.cells) {
        const int row = cell.row == 1 ? 1 : 0;
        const int labelWidth = MeasureTextWidth(buffer, cell.label);
        const int valueWidth = MeasureTextWidth(buffer, cell.value);
        measuredRowWidths[row] += labelWidth + labelValueGap + valueWidth;
        ++rowCellCounts[row];
    }
    for (int row = 0; row != layout.rows; ++row) {
        if (rowCellCounts[row] > 1) {
            measuredRowWidths[row] += (rowCellCounts[row] - 1) * cellGap;
        }
    }
    const int measuredContentWidth = std::max(measuredRowWidths[0], measuredRowWidths[1]);
    const int neededWidth = std::max(1, measuredContentWidth + outerPadding * 2);
    UpdateBandSize(neededWidth);
    if (width < neededWidth) {
        if (oldFont) SelectObject(buffer, oldFont);
        if (font) DeleteObject(font);
        if (oldBitmap) SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    const int rowHeight = std::max(1, height / layout.rows);
    for (int row = 0; row != layout.rows; ++row) {
        int x = std::max(outerPadding, width - outerPadding - measuredRowWidths[row]);
        const int top = row * rowHeight;
        const int bottom = row == layout.rows - 1 ? height : top + rowHeight;
        int drawnCells = 0;
        for (const auto& cell : layout.cells) {
            if (cell.row != row) continue;
            const int labelWidth = MeasureTextWidth(buffer, cell.label);
            const int valueWidth = MeasureTextWidth(buffer, cell.value);
            RECT label{x, top, x + labelWidth, bottom};
            RECT value{x + labelWidth + labelValueGap, top,
                       x + labelWidth + labelValueGap + valueWidth, bottom};
            SetTextColor(buffer, labelColor);
            DrawTextW(buffer, cell.label.c_str(), -1, &label,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
            COLORREF cellValueColor = valueColor;
            if (cell.severity == AlertSeverity::Warning)
                cellValueColor = static_cast<COLORREF>(config.warningColor);
            else if (cell.severity == AlertSeverity::Critical)
                cellValueColor = static_cast<COLORREF>(config.criticalColor);
            SetTextColor(buffer, cellValueColor);
            DrawTextW(buffer, cell.value.c_str(), -1, &value,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
            x += labelWidth + labelValueGap + valueWidth;
            ++drawnCells;
            if (drawnCells < rowCellCounts[row]) x += cellGap;
        }
    }
    BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    if (oldFont) SelectObject(buffer, oldFont);
    if (font) DeleteObject(font);
    if (oldBitmap) SelectObject(buffer, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
}

void TaskbarBand::UpdateBandSize(int width) {
    const int nextWidth = std::max(1, width);
    if (nextWidth == idealSize_.cx) return;
    idealSize_.cx = nextWidth;
    if (hwnd_) {
        SetBandWidth(hwnd_, idealSize_.cx);
    }
}

void TaskbarBand::ConnectSharedMemory() {
    if (!mapping_) {
        mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, SharedSensorMappingName);
        if (mapping_) {
            shared_ = static_cast<const SharedSensorSnapshot*>(
                MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, sizeof(SharedSensorSnapshot)));
            if (!shared_) {
                CloseHandle(mapping_);
                mapping_ = nullptr;
            }
        }
    }
    if (!commandMapping_) {
        commandMapping_ = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE,
                                            SharedBandCommandMappingName);
        if (commandMapping_) {
            command_ = static_cast<SharedBandCommand*>(MapViewOfFile(
                commandMapping_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
                sizeof(SharedBandCommand)));
            if (!command_) {
                CloseHandle(commandMapping_);
                commandMapping_ = nullptr;
            }
        }
    }
    if (!snapshotEvent_) {
        snapshotEvent_ = OpenEventW(SYNCHRONIZE, FALSE, SharedSensorEventName);
    }
    if (snapshotEvent_ && hwnd_ && !snapshotWait_) {
        if (!RegisterWaitForSingleObject(
                &snapshotWait_, snapshotEvent_, SnapshotEventCallback,
                reinterpret_cast<PVOID>(hwnd_), INFINITE, WT_EXECUTEDEFAULT)) {
            snapshotWait_ = nullptr;
        }
    }
    if (!commandEvent_) {
        commandEvent_ = OpenEventW(EVENT_MODIFY_STATE, FALSE, SharedBandCommandEventName);
    }
}

void TaskbarBand::ConnectSettingsNotifications() {
    if (!settingsKey_) {
        DWORD disposition = 0;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_NOTIFY | KEY_QUERY_VALUE,
                            nullptr, &settingsKey_, &disposition) != ERROR_SUCCESS) {
            settingsKey_ = nullptr;
            return;
        }
    }
    if (!settingsEvent_) {
        settingsEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!settingsEvent_) return;
    }
    ArmSettingsNotification();
    if (settingsEvent_ && hwnd_ && !settingsWait_ &&
        !RegisterWaitForSingleObject(&settingsWait_, settingsEvent_, SettingsEventCallback,
                                     this, INFINITE, WT_EXECUTEDEFAULT)) {
        settingsWait_ = nullptr;
    }
}

void TaskbarBand::ArmSettingsNotification() {
    if (!settingsKey_ || !settingsEvent_) return;
    RegNotifyChangeKeyValue(settingsKey_, FALSE, REG_NOTIFY_CHANGE_LAST_SET,
                            settingsEvent_, TRUE);
}

bool TaskbarBand::ReadSnapshot(SharedSensorSnapshot& result) {
    ConnectSharedMemory();
    if (!shared_) return false;
    for (int attempt = 0; attempt != 3; ++attempt) {
        const auto before = shared_->sequence;
        if (before & 1u) continue;
        result = *shared_;
        const auto after = shared_->sequence;
        if (before == after && !(after & 1u) &&
            IsSharedSnapshotFresh(result, static_cast<std::uint64_t>(GetTickCount64()))) {
            return true;
        }
    }
    return false;
}

void TaskbarBand::RefreshSnapshotState(HWND window, bool settingsChanged) {
    SharedSensorSnapshot snapshot{};
    if (!ReadSnapshot(snapshot)) {
        monitorReady_ = false;
        monitorEnabled_ = false;
        UpdateBandSize(1);
        if (IsWindowVisible(window)) ShowWindow(window, SW_HIDE);
        return;
    }

    monitorReady_ = true;
    monitorEnabled_ = (snapshot.displayFlags & TaskbarEnabled) != 0;
    const bool shouldShow = shellShowRequested_ && monitorEnabled_;
    const bool visible = IsWindowVisible(window) != FALSE;
    const bool visibilityChanged = visible != shouldShow;
    if (!monitorEnabled_) UpdateBandSize(1);
    if (visibilityChanged) ShowWindow(window, shouldShow ? SW_SHOW : SW_HIDE);

    const bool snapshotChanged = !hasLastVisualSnapshot_ ||
        !SameVisualSnapshot(snapshot, lastVisualSnapshot_);
    if (snapshotChanged) {
        lastVisualSnapshot_ = snapshot;
        hasLastVisualSnapshot_ = true;
    }

    if (shouldShow && (snapshotChanged || visibilityChanged || settingsChanged)) {
        InvalidateRect(window, nullptr, FALSE);
    }
}

void PublishModeCommand(SharedBandCommand* command, bool compact) {
    if (!command) return;
    const auto sequence = command->sequence + 1;
    command->sequence = sequence | 1u;
    command->action = BandCommandSetDisplay;
    command->displayMode = compact ? 1u : 0u;
    command->version = 1;
    command->sequence = sequence + 1;
}

void PublishDisplayCommand(SharedBandCommand* command, std::uint32_t flags) {
    if (!command) return;
    const auto sequence = command->sequence + 1;
    command->sequence = sequence | 1u;
    command->action = BandCommandSetMetrics;
    command->displayFlags = flags;
    command->version = 1;
    command->sequence = sequence + 1;
}

void PublishSimpleCommand(SharedBandCommand* command, BandCommandAction action) {
    if (!command) return;
    const auto sequence = command->sequence + 1;
    command->sequence = sequence | 1u;
    command->action = action;
    command->version = 1;
    command->sequence = sequence + 1;
}

VOID CALLBACK TaskbarBand::SnapshotEventCallback(PVOID context, BOOLEAN) {
    const HWND window = reinterpret_cast<HWND>(context);
    if (window) PostMessageW(window, SnapshotEventMessage, 0, 0);
}

VOID CALLBACK TaskbarBand::SettingsEventCallback(PVOID context, BOOLEAN) {
    auto* self = static_cast<TaskbarBand*>(context);
    if (!self) return;
    self->ArmSettingsNotification();
    if (self->hwnd_) PostMessageW(self->hwnd_, SettingsEventMessage, 0, 0);
}

LRESULT CALLBACK TaskbarBand::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<TaskbarBand*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = reinterpret_cast<TaskbarBand*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    __try {
        return self ? self->HandleMessage(window, message, wParam, lParam) :
            DefWindowProcW(window, message, wParam, lParam);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

LRESULT TaskbarBand::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!IsSafeMessage(message)) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(window, &paint);
        Paint(dc);
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == SnapshotEventMessage || message == SettingsEventMessage) {
        ConnectSharedMemory();
        RefreshSnapshotState(window, message == SettingsEventMessage);
        return 0;
    }
    if (message != WM_RBUTTONUP && message != WM_CONTEXTMENU) {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HMENU menu = CreatePopupMenu();
    HMENU mode = CreatePopupMenu();
    HMENU rowsMenu = CreatePopupMenu();
    HMENU metrics = CreatePopupMenu();
    if (!menu || !mode || !rowsMenu || !metrics) {
        if (menu) DestroyMenu(menu);
        if (mode) DestroyMenu(mode);
        if (rowsMenu) DestroyMenu(rowsMenu);
        if (metrics) DestroyMenu(metrics);
        return 0;
    }
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Hardware Monitor");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    SharedSensorSnapshot snapshot{};
    const bool hasSnapshot = ReadSnapshot(snapshot);
    const bool compact = hasSnapshot && snapshot.displayMode != 0;
    AppendMenuW(mode, MF_STRING | (!compact ? MF_CHECKED : 0), CommandDisplayFull, L"Full");
    AppendMenuW(mode, MF_STRING | (compact ? MF_CHECKED : 0), CommandDisplayCompact, L"Compact");
    const int rows = LoadLayoutRows();
    AppendMenuW(rowsMenu, MF_STRING | (rows == 1 ? MF_CHECKED : 0), CommandRowsOne, L"One line");
    AppendMenuW(rowsMenu, MF_STRING | (rows == 2 ? MF_CHECKED : 0), CommandRowsTwo, L"Two lines");
    const UINT flags = hasSnapshot ? snapshot.displayFlags : DisplayFlagsFromConfig(Config{});
    const bool customFormatControlsMetrics = !LoadFormatTemplate().empty();
    const UINT metricIds[] = {
        CommandCpu, CommandCpuUsage, CommandGpu, CommandSsd, CommandNetwork, CommandPower,
        CommandMemory, CommandGpuUsage, CommandVram, CommandDiskIo, CommandCpuClock,
        CommandGpuPower, CommandFan, CommandBattery, CommandSystemPower,
        CommandCpuInternalTemperature, CommandGpuInternalTemperature, CommandCpuFanRpm, CommandGpuFanRpm};
    const UINT metricFlags[] = {
        ShowCpuTemperature, ShowCpuUsage, ShowGpuTemperature, ShowDiskTemperature, ShowNetwork,
        ShowPower, ShowMemory, ShowGpuUsage, ShowVram, ShowDiskIo, ShowCpuClock,
        ShowGpuPower, ShowFan, ShowBattery, ShowSystemPower, ShowCpuInternalTemperature,
        ShowGpuInternalTemperature, ShowCpuFanRpm, ShowGpuFanRpm};
    const wchar_t* const metricLabels[] = {
        L"CPU Temperature", L"CPU Usage", L"GPU Temperature", L"Disk Temperature", L"Network",
        L"CPU Power", L"RAM Usage", L"GPU Usage", L"VRAM", L"Disk Read/Write",
        L"CPU Clock", L"GPU Power", L"GPU Fan", L"Battery", L"System Power",
        L"CPU Internal Temperature", L"GPU Internal Temperature", L"CPU Fan RPM",
        L"GPU Fan RPM"};
    for (int index = 0; index != 19; ++index) {
        AppendMenuW(metrics, MF_STRING | ((flags & metricFlags[index]) ? MF_CHECKED : 0) |
                    (customFormatControlsMetrics ? MF_GRAYED : 0),
                    metricIds[index], metricLabels[index]);
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(mode), L"Display Mode");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(rowsMenu), L"Layout");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(metrics),
                customFormatControlsMetrics ? L"Metrics (controlled by format)" : L"Metrics");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CommandSettings, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CommandExit, L"Exit Hardware Monitor");
    POINT point{};
    GetCursorPos(&point);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                        point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command == CommandDisplayFull || command == CommandDisplayCompact) {
        PublishModeCommand(command_, command == CommandDisplayCompact);
        if (commandEvent_) SetEvent(commandEvent_);
    } else if (command == CommandRowsOne || command == CommandRowsTwo) {
        SaveLayoutRows(command == CommandRowsTwo ? 2 : 1);
        InvalidateRect(window, nullptr, FALSE);
    } else if (command >= CommandCpu && command <= CommandGpuFanRpm && hasSnapshot &&
               !customFormatControlsMetrics) {
        std::uint32_t nextFlags = snapshot.displayFlags;
        nextFlags ^= metricFlags[command - CommandCpu];
        PublishDisplayCommand(command_, nextFlags);
        if (commandEvent_) SetEvent(commandEvent_);
    } else if (command == CommandSettings) {
        PublishSimpleCommand(command_, BandCommandOpenSettings);
        if (commandEvent_) SetEvent(commandEvent_);
    } else if (command == CommandExit) {
        PublishSimpleCommand(command_, BandCommandExit);
        if (commandEvent_) SetEvent(commandEvent_);
    }
    return 0;
}
} // namespace monitor
