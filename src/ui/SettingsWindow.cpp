#include "SettingsWindow.h"
#include "TaskbarLayout.h"
#include "../monitor/StorageMonitor.h"
#include "../monitor/SensorDemand.h"

#include <windows.h>
#include <commdlg.h>
#include <winreg.h>
#include <algorithm>

namespace monitor {
namespace {
constexpr int CloseId = 100;
constexpr int SaveId = 101;
constexpr int TaskbarCheckBase = 200;
constexpr int ValueColorChooseId = 300;
constexpr int ValueColorAutoId = 301;
constexpr int ValueColorPreviewId = 302;
constexpr int FontChooseId = 310;
constexpr int FormatEditId = 320;
constexpr int FormatVariablesId = 321;
constexpr int FormatResetId = 322;
constexpr int HelpId = 323;
constexpr int HelpCloseId = 324;
constexpr int AlertSettingsId = 325;
constexpr int AlertSaveId = 400;
constexpr int AlertCancelId = 401;
constexpr int AlertEnableId = 402;
constexpr int WarningColorChooseId = 410;
constexpr int CriticalColorChooseId = 411;
constexpr int WarningColorPreviewId = 412;
constexpr int CriticalColorPreviewId = 413;
constexpr wchar_t FormatHintText[] =
    L"Display format 2.0 - 快捷说明\r\n"
    L"Enter / \\n = 第二行    \\t = 下一对齐列\r\n"
    L"\r\n"
    L"普通变量：{cpu_temp}  {cpu_usage}  {ram_usage}  {down}  {up}\r\n"
    L"精度：{cpu_temp:1} -> 54.3°    {cpu_usage:1} -> 23.0%\r\n"
    L"单位：{cpu_clock:ghz}  {ram_used:gb}  {down:mb}\r\n"
    L"条件段：{gpu_temp?GPU:{gpu_temp}}\r\n"
    L"        只有 GPU 温度有效时才显示整个 GPU 段\r\n"
    L"\r\n"
    L"示例：\r\n"
    L"CPU:{cpu_temp:1}\\tRAM:{ram_usage}\\t{gpu_temp?GPU:{gpu_temp}}\r\n"
    L"↑:{up:mb}\\t↓:{down:mb}\\tBAT:{battery}";
constexpr wchar_t BandSettingsPath[] = L"Software\\TaskbarHardwareMonitor\\TaskbarBand";
constexpr wchar_t HelpText[] =
    L"Taskbar Hardware Monitor 帮助\r\n"
    L"\r\n"
    L"【电池状态】\r\n"
    L"AC    已连接交流电源，但当前没有充电。常见于电池保护模式，例如限制充到 80%。\r\n"
    L"CHG   正在充电。\r\n"
    L"FULL  电池已充满。\r\n"
    L"无后缀  当前正在使用电池放电。\r\n"
    L"\r\n"
    L"【任务栏常见缩写】\r\n"
    L"CPU / C     CPU 温度\r\n"
    L"LOAD / U    CPU 占用率\r\n"
    L"GPU / G     GPU 温度\r\n"
    L"GLOAD / GU  GPU 占用率\r\n"
    L"RAM / R     内存占用率\r\n"
    L"PWR / P     CPU Package 功耗\r\n"
    L"GPWR / GP   GPU 功耗\r\n"
    L"CLK / F     CPU 当前频率\r\n"
    L"VRAM / V    显存占用\r\n"
    L"DISK / D    磁盘温度\r\n"
    L"READ / DR   磁盘读取速度\r\n"
    L"WRITE / DW  磁盘写入速度\r\n"
    L"BAT / B     电池电量和状态\r\n"
    L"SYS         电池放电时的整机功耗\r\n"
    L"↓           网络下载速度\r\n"
    L"↑           网络上传速度\r\n"
    L"\r\n"
    L"【Display format】\r\n"
    L"直接按 Enter 或使用 \\n：换到任务栏第二行。\r\n"
    L"使用 \\t：开始下一列；上下两行相同列会自动像素对齐。\r\n"
    L"\r\n"
    L"推荐示例：\r\n"
    L"温度:{cpu_temp}\\t占用:{cpu_usage}\\t内存:{ram_usage}\\n"
    L"上行:{up}\\t下行:{down}\\t电池:{battery}\r\n"
    L"\r\n"
    L"【常用格式变量】\r\n"
    L"{cpu_temp}       CPU 温度\r\n"
    L"{cpu_usage}      CPU 占用率\r\n"
    L"{cpu_clock}      CPU 频率\r\n"
    L"{power}          CPU 功耗\r\n"
    L"{gpu_temp}       GPU 温度\r\n"
    L"{gpu_usage}      GPU 占用率\r\n"
    L"{gpu_power}      GPU 功耗\r\n"
    L"{fan}            GPU 风扇百分比\r\n"
    L"{ram_usage}      内存占用率\r\n"
    L"{ram_used}       已用内存\r\n"
    L"{vram}           显存 已用/总量\r\n"
    L"{disk_temp}      磁盘温度\r\n"
    L"{disk_read}      磁盘读取速度\r\n"
    L"{disk_write}     磁盘写入速度\r\n"
    L"{down} / {up}    网络下载 / 上传速度\r\n"
    L"{battery}        电池百分比 + 简短状态\r\n"
    L"{battery_percent} 仅电池百分比\r\n"
    L"{battery_status}  完整电池状态文字\r\n"
    L"{system_power}   电池放电时的整机功耗\r\n"
    L"\r\n"
    L"【Display format 2.0】\r\n"
    L"变量修饰符示例：\r\n"
    L"{cpu_temp:1}      保留 1 位小数，例如 54.3°\r\n"
    L"{cpu_usage:1}     保留 1 位小数\r\n"
    L"{cpu_clock:ghz}   GHz 格式\r\n"
    L"{cpu_clock:mhz}   MHz 格式\r\n"
    L"{ram_used:gb}     强制 GB\r\n"
    L"{down:kb|mb|gb}   强制网络速率单位\r\n"
    L"条件段：{gpu_temp?GPU:{gpu_temp}}\r\n"
    L"条件变量无有效数据时，整个条件段不显示。\r\n"
    L"\r\n"
    L"【Threshold alert colors】\r\n"
    L"CPU/GPU 温度和 RAM 在达到 Warning/Critical 上限时变色。\r\n"
    L"Battery 在低于 Warning/Critical 下限时变色。\r\n"
    L"该功能默认关闭，可在 Settings -> Thresholds... 中开启。\r\n";

constexpr COLORREF WindowBackground = RGB(32, 32, 32);
constexpr COLORREF PanelBackground = RGB(45, 45, 45);
constexpr COLORREF TextColor = RGB(235, 235, 235);

void ApplyTheme(HWND window, HFONT uiFont) {
    const HFONT font = uiFont ? uiFont :
        reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    EnumChildWindows(window, [](HWND child, LPARAM parameter) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(child, className, _countof(className));
        if (lstrcmpW(className, L"Button") == 0 || lstrcmpW(className, L"Edit") == 0 ||
            lstrcmpW(className, L"ComboBox") == 0 || lstrcmpW(className, L"Static") == 0) {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(parameter), TRUE);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(font));
}

HWND AddCheck(HWND parent, HINSTANCE instance, const wchar_t* text, int x, int y, int id) {
    return CreateWindowW(
        L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, x, y, 190, 22,
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}

void ActivateSettingsWindow(HWND window) {
    if (!window || !IsWindow(window)) return;

    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    else ShowWindow(window, SW_SHOW);

    // Explorer owns the user gesture that opens Settings, while this window belongs
    // to the monitor process. Attach to the current foreground input queue so Windows
    // does not leave the Settings window behind another application.
    const HWND foreground = GetForegroundWindow();
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD foregroundThread = foreground
        ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
        AttachThreadInput(currentThread, foregroundThread, TRUE) != FALSE;

    // Raise once using TOPMOST, then immediately restore normal z-order. This avoids
    // making Settings permanently always-on-top while still guaranteeing visibility.
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetActiveWindow(window);
    SetFocus(window);
    SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

    if (attached) AttachThreadInput(currentThread, foregroundThread, FALSE);

    // If foreground activation is still denied by an unusual shell/security policy,
    // request visible taskbar attention rather than silently hiding behind windows.
    if (GetForegroundWindow() != window) {
        FLASHWINFO flash{sizeof(flash), window, FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0};
        FlashWindowEx(&flash);
    }
}

void SaveBandValueColor(bool custom, COLORREF color) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, &disposition) != ERROR_SUCCESS) {
        return;
    }
    const DWORD enabled = custom ? 1u : 0u;
    const DWORD storedColor = static_cast<DWORD>(color & 0x00FFFFFFu);
    RegSetValueExW(key, L"ValueColorCustom", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
    RegSetValueExW(key, L"ValueColor", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&storedColor), sizeof(storedColor));
    RegCloseKey(key);
}

void SaveBandFont(const std::wstring& name, int size, int weight) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, &disposition) != ERROR_SUCCESS) {
        return;
    }
    RegSetValueExW(key, L"FontName", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(name.c_str()),
                   static_cast<DWORD>((name.size() + 1) * sizeof(wchar_t)));
    const DWORD storedSize = static_cast<DWORD>(size);
    const DWORD storedWeight = static_cast<DWORD>(weight);
    RegSetValueExW(key, L"FontSize", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&storedSize), sizeof(storedSize));
    RegSetValueExW(key, L"FontWeight", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&storedWeight), sizeof(storedWeight));
    RegCloseKey(key);
}

void SaveBandFormat(const std::wstring& format) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, &disposition) != ERROR_SUCCESS) {
        return;
    }
    RegSetValueExW(key, L"Format", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(format.c_str()),
                   static_cast<DWORD>((format.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

void SaveBandAlertSettings(const Config& config) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, BandSettingsPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, &disposition) != ERROR_SUCCESS) {
        return;
    }
    auto writeDword = [&](const wchar_t* name, DWORD value) {
        RegSetValueExW(key, name, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
    };
    writeDword(L"ThresholdColorsEnabled", config.thresholdColorsEnabled ? 1u : 0u);
    writeDword(L"CpuTempWarning", static_cast<DWORD>(config.cpuTempWarning));
    writeDword(L"CpuTempCritical", static_cast<DWORD>(config.cpuTempCritical));
    writeDword(L"GpuTempWarning", static_cast<DWORD>(config.gpuTempWarning));
    writeDword(L"GpuTempCritical", static_cast<DWORD>(config.gpuTempCritical));
    writeDword(L"RamWarning", static_cast<DWORD>(config.ramWarning));
    writeDword(L"RamCritical", static_cast<DWORD>(config.ramCritical));
    writeDword(L"BatteryWarning", static_cast<DWORD>(config.batteryWarning));
    writeDword(L"BatteryCritical", static_cast<DWORD>(config.batteryCritical));
    writeDword(L"WarningColor", config.warningColor & 0x00FFFFFFu);
    writeDword(L"CriticalColor", config.criticalColor & 0x00FFFFFFu);
    RegCloseKey(key);
}

} // namespace

bool SettingsWindow::Show(HINSTANCE instance, HWND owner, Config* config) {
    if (hwnd_) {
        ActivateSettingsWindow(hwnd_);
        return true;
    }
    config_ = config;
    owner_ = owner;
    thresholdColorsEnabled_ = config_->thresholdColorsEnabled;
    cpuTempWarning_ = config_->cpuTempWarning;
    cpuTempCritical_ = config_->cpuTempCritical;
    gpuTempWarning_ = config_->gpuTempWarning;
    gpuTempCritical_ = config_->gpuTempCritical;
    ramWarning_ = config_->ramWarning;
    ramCritical_ = config_->ramCritical;
    batteryWarning_ = config_->batteryWarning;
    batteryCritical_ = config_->batteryCritical;
    warningColor_ = static_cast<COLORREF>(config_->warningColor & 0x00FFFFFFu);
    criticalColor_ = static_cast<COLORREF>(config_->criticalColor & 0x00FFFFFFu);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = Proc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = L"MonitorSettingsWindow";
    RegisterClassW(&windowClass);
    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME, windowClass.lpszClassName, L"Hardware Monitor Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
        820, 740, owner, nullptr, instance, this);
    if (!hwnd_) return false;

    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
        uiFont_ = CreateFontIndirectW(&metrics.lfMessageFont);
    }

    taskbarEnabled_ = AddCheck(hwnd_, instance, L"Enable taskbar band", 24, 20, 0);
    CreateWindowW(L"STATIC", L"Display mode:", WS_CHILD | WS_VISIBLE,
                  24, 52, 145, 20, hwnd_, nullptr, instance, nullptr);
    displayMode_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                                 CBS_DROPDOWNLIST, 180, 48, 150, 100, hwnd_, nullptr,
                                 instance, nullptr);
    SendMessageW(displayMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Full"));
    SendMessageW(displayMode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Compact"));
    SendMessageW(displayMode_, CB_SETCURSEL,
                 config_->displayMode == DisplayMode::Compact ? 1 : 0, 0);

    CreateWindowW(L"STATIC", L"Metrics:", WS_CHILD | WS_VISIBLE,
                  24, 84, 145, 20, hwnd_, nullptr, instance, nullptr);
    const wchar_t* const labels[] = {
        L"CPU temperature", L"CPU usage", L"GPU temperature", L"Disk temperature", L"Network",
        L"CPU power", L"RAM usage", L"GPU usage", L"VRAM", L"Disk read/write",
        L"CPU clock", L"GPU power", L"GPU fan", L"Battery", L"System power"};
    for (int index = 0; index != 15; ++index) {
        const int column = index / 5;
        const int row = index % 5;
        taskbarChecks_[index] = AddCheck(
            hwnd_, instance, labels[index], 180 + column * 200, 80 + row * 25,
            TaskbarCheckBase + index);
    }

    valueColorCustom_ = config_->taskbarValueColorCustom;
    valueColor_ = static_cast<COLORREF>(config_->taskbarValueColor & 0x00FFFFFFu);
    CreateWindowW(L"STATIC", L"Value color:", WS_CHILD | WS_VISIBLE,
                  24, 240, 145, 20, hwnd_, nullptr, instance, nullptr);
    valueColorPreview_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                       180, 236, 34, 24, hwnd_,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(ValueColorPreviewId)),
                                       instance, nullptr);
    CreateWindowW(L"BUTTON", L"Choose...", WS_CHILD | WS_VISIBLE,
                  222, 235, 82, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(ValueColorChooseId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Auto", WS_CHILD | WS_VISIBLE,
                  312, 235, 64, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(ValueColorAutoId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Thresholds...", WS_CHILD | WS_VISIBLE,
                  390, 235, 110, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(AlertSettingsId)), instance, nullptr);

    ZeroMemory(&taskbarFont_, sizeof(taskbarFont_));
    taskbarFontSize_ = config_->taskbarFontSize;
    taskbarFont_.lfWeight = config_->taskbarFontWeight;
    taskbarFont_.lfCharSet = DEFAULT_CHARSET;
    taskbarFont_.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    lstrcpynW(taskbarFont_.lfFaceName, config_->taskbarFontName.c_str(), LF_FACESIZE);
    CreateWindowW(L"STATIC", L"Font:", WS_CHILD | WS_VISIBLE,
                  24, 275, 145, 20, hwnd_, nullptr, instance, nullptr);
    fontDisplay_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                 180, 275, 330, 20, hwnd_, nullptr, instance, nullptr);
    CreateWindowW(L"BUTTON", L"Choose...", WS_CHILD | WS_VISIBLE,
                  525, 270, 95, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(FontChooseId)), instance, nullptr);
    UpdateFontDisplay();

    CreateWindowW(L"STATIC", L"Display format:", WS_CHILD | WS_VISIBLE,
                  24, 310, 145, 20, hwnd_, nullptr, instance, nullptr);
    formatEdit_ = CreateWindowW(L"EDIT", config_->taskbarFormat.c_str(),
                                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                180, 305, 600, 90, hwnd_,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(FormatEditId)),
                                instance, nullptr);
    SendMessageW(formatEdit_, EM_SETLIMITTEXT, 4095, 0);
    CreateWindowW(L"BUTTON", L"Variables...", WS_CHILD | WS_VISIBLE,
                  590, 402, 95, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(FormatVariablesId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Default", WS_CHILD | WS_VISIBLE,
                  695, 402, 85, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(FormatResetId)), instance, nullptr);
    CreateWindowW(L"STATIC", L"Preview:", WS_CHILD | WS_VISIBLE,
                  24, 442, 145, 20, hwnd_, nullptr, instance, nullptr);
    formatPreview_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   180, 440, 600, 48, hwnd_, nullptr, instance, nullptr);
    formatHint_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", FormatHintText,
        WS_POPUP | WS_BORDER | SS_LEFT | SS_NOPREFIX,
        0, 0, 610, 160, hwnd_, nullptr, instance, nullptr);
    if (formatHint_) {
        const HFONT hintFont = uiFont_ ? uiFont_ :
            reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(formatHint_, WM_SETFONT,
                     reinterpret_cast<WPARAM>(hintFont), FALSE);
    }
    UpdateFormatPreview();

    CreateWindowW(L"STATIC", L"Disk temperature:", WS_CHILD | WS_VISIBLE,
                  24, 510, 145, 20, hwnd_, nullptr, instance, nullptr);
    diskSource_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                                CBS_DROPDOWNLIST | WS_VSCROLL,
                                 180, 505, 600, 220, hwnd_, nullptr, instance, nullptr);
    const int autoItem = static_cast<int>(SendMessageW(
        diskSource_, CB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(L"Auto (prefer NVMe/SSD)")));
    SendMessageW(diskSource_, CB_SETITEMDATA, autoItem, static_cast<LPARAM>(-1));
    int selectedDiskItem = config_->storageDriveIndex < 0 ? autoItem : -1;
    for (const auto& device : EnumerateStorageDevices()) {
        std::wstring label = device.name + L" (" + device.bus + L", PhysicalDrive" +
                             std::to_wstring(device.index) + L")";
        const int item = static_cast<int>(SendMessageW(
            diskSource_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(diskSource_, CB_SETITEMDATA, item, static_cast<LPARAM>(device.index));
        if (device.index == config_->storageDriveIndex) selectedDiskItem = item;
    }
    if (selectedDiskItem < 0 && config_->storageDriveIndex >= 0) {
        const std::wstring missing = L"PhysicalDrive" + std::to_wstring(config_->storageDriveIndex) +
                                     L" (currently unavailable)";
        selectedDiskItem = static_cast<int>(SendMessageW(
            diskSource_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(missing.c_str())));
        SendMessageW(diskSource_, CB_SETITEMDATA, selectedDiskItem,
                     static_cast<LPARAM>(config_->storageDriveIndex));
    }
    SendMessageW(diskSource_, CB_SETCURSEL,
                 selectedDiskItem >= 0 ? selectedDiskItem : autoItem, 0);

    interval_ = CreateWindowW(L"EDIT", std::to_wstring(config_->refreshIntervalMs).c_str(),
                              WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                              180, 550, 90, 24, hwnd_, nullptr, instance, nullptr);
    CreateWindowW(L"STATIC", L"Collection interval (ms):", WS_CHILD | WS_VISIBLE,
                  24, 554, 145, 20, hwnd_, nullptr, instance, nullptr);
    startup_ = AddCheck(hwnd_, instance, L"Start with Windows", 24, 585, 0);
    SendMessageW(taskbarEnabled_, BM_SETCHECK,
                 config_->taskbarEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    const bool enabled[] = {
        config_->showCpuTemperature, config_->showCpuUsage, config_->showGpuTemperature,
        config_->showDiskTemperature, config_->showNetwork, config_->showPower,
        config_->showMemory, config_->showGpuUsage, config_->showVram, config_->showDiskIo,
        config_->showCpuClock, config_->showGpuPower, config_->showFan,
        config_->showBattery, config_->showSystemPower};
    for (int index = 0; index != 15; ++index) {
        SendMessageW(taskbarChecks_[index], BM_SETCHECK,
                     enabled[index] ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    SyncMetricsFromFormat();
    SendMessageW(startup_, BM_SETCHECK,
                 config_->startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
    CreateWindowW(L"BUTTON", L"Help...", WS_CHILD | WS_VISIBLE,
                  24, 650, 80, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(HelpId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  650, 650, 65, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(SaveId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                  725, 650, 65, 26, hwnd_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(CloseId)), instance, nullptr);
    ApplyTheme(hwnd_, uiFont_);
    ActivateSettingsWindow(hwnd_);
    UpdateWindow(hwnd_);
    return true;
}

void SettingsWindow::ChooseValueColor() {
    static COLORREF customColors[16]{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = hwnd_;
    chooser.rgbResult = valueColor_;
    chooser.lpCustColors = customColors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&chooser)) {
        valueColor_ = chooser.rgbResult;
        valueColorCustom_ = true;
        if (valueColorPreview_) InvalidateRect(valueColorPreview_, nullptr, TRUE);
    }
}

void SettingsWindow::SetAutomaticValueColor() {
    valueColorCustom_ = false;
    if (valueColorPreview_) InvalidateRect(valueColorPreview_, nullptr, TRUE);
}

void SettingsWindow::ChooseWarningColor() {
    static COLORREF customColors[16]{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = alertWindow_ ? alertWindow_ : hwnd_;
    chooser.rgbResult = alertWarningColor_;
    chooser.lpCustColors = customColors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&chooser)) {
        alertWarningColor_ = chooser.rgbResult;
        if (warningColorPreview_) InvalidateRect(warningColorPreview_, nullptr, TRUE);
    }
}

void SettingsWindow::ChooseCriticalColor() {
    static COLORREF customColors[16]{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = alertWindow_ ? alertWindow_ : hwnd_;
    chooser.rgbResult = alertCriticalColor_;
    chooser.lpCustColors = customColors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&chooser)) {
        alertCriticalColor_ = chooser.rgbResult;
        if (criticalColorPreview_) InvalidateRect(criticalColorPreview_, nullptr, TRUE);
    }
}

void SettingsWindow::ShowThresholdSettings() {
    if (alertWindow_ && IsWindow(alertWindow_)) {
        ActivateSettingsWindow(alertWindow_);
        return;
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
    WNDCLASSW alertClass{};
    alertClass.lpfnWndProc = AlertProc;
    alertClass.hInstance = instance;
    alertClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    alertClass.hbrBackground = nullptr;
    alertClass.lpszClassName = L"TaskbarHardwareMonitorThresholdWindow";
    RegisterClassW(&alertClass);

    alertWarningColor_ = warningColor_;
    alertCriticalColor_ = criticalColor_;
    alertWindow_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME, alertClass.lpszClassName, L"Threshold Alert Colors",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 410, hwnd_, nullptr, instance, this);
    if (!alertWindow_) return;

    alertEnabled_ = CreateWindowW(
        L"BUTTON", L"Enable threshold warning / critical colors",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 18, 360, 22,
        alertWindow_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(AlertEnableId)), instance, nullptr);
    // Opening Thresholds... is an explicit intent to configure alert colors.
    // Default the dialog to enabled so users do not save valid thresholds that never apply.
    SendMessageW(alertEnabled_, BM_SETCHECK, BST_CHECKED, 0);

    CreateWindowW(L"STATIC", L"Warning", WS_CHILD | WS_VISIBLE,
                  245, 54, 80, 20, alertWindow_, nullptr, instance, nullptr);
    CreateWindowW(L"STATIC", L"Critical", WS_CHILD | WS_VISIBLE,
                  345, 54, 80, 20, alertWindow_, nullptr, instance, nullptr);

    auto addThresholdRow = [&](const wchar_t* label, int y, int warning, int critical,
                               HWND& warningEdit, HWND& criticalEdit) {
        CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE,
                      20, y + 4, 190, 20, alertWindow_, nullptr, instance, nullptr);
        warningEdit = CreateWindowW(L"EDIT", std::to_wstring(warning).c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                    240, y, 70, 24, alertWindow_, nullptr, instance, nullptr);
        criticalEdit = CreateWindowW(L"EDIT", std::to_wstring(critical).c_str(),
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                     340, y, 70, 24, alertWindow_, nullptr, instance, nullptr);
    };
    addThresholdRow(L"CPU temperature (°C, high)", 82, cpuTempWarning_, cpuTempCritical_, cpuWarning_, cpuCritical_);
    addThresholdRow(L"GPU temperature (°C, high)", 116, gpuTempWarning_, gpuTempCritical_, gpuWarning_, gpuCritical_);
    addThresholdRow(L"RAM usage (%, high)", 150, ramWarning_, ramCritical_, ramWarningEdit_, ramCriticalEdit_);
    addThresholdRow(L"Battery (%, low)", 184, batteryWarning_, batteryCritical_, batteryWarningEdit_, batteryCriticalEdit_);

    CreateWindowW(L"STATIC", L"For CPU/GPU/RAM, Critical must be above Warning. For Battery, Critical must be below Warning.",
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 220, 460, 38, alertWindow_, nullptr, instance, nullptr);

    CreateWindowW(L"STATIC", L"Warning color:", WS_CHILD | WS_VISIBLE,
                  20, 268, 110, 20, alertWindow_, nullptr, instance, nullptr);
    warningColorPreview_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                         132, 264, 34, 24, alertWindow_,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(WarningColorPreviewId)),
                                         instance, nullptr);
    CreateWindowW(L"BUTTON", L"Choose...", WS_CHILD | WS_VISIBLE,
                  176, 263, 82, 26, alertWindow_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(WarningColorChooseId)), instance, nullptr);

    CreateWindowW(L"STATIC", L"Critical color:", WS_CHILD | WS_VISIBLE,
                  280, 268, 100, 20, alertWindow_, nullptr, instance, nullptr);
    criticalColorPreview_ = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                          380, 264, 34, 24, alertWindow_,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(CriticalColorPreviewId)),
                                          instance, nullptr);
    CreateWindowW(L"BUTTON", L"Choose...", WS_CHILD | WS_VISIBLE,
                  424, 263, 72, 26, alertWindow_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(CriticalColorChooseId)), instance, nullptr);

    CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  330, 325, 75, 28, alertWindow_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(AlertSaveId)), instance, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                  415, 325, 75, 28, alertWindow_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(AlertCancelId)), instance, nullptr);

    ApplyTheme(alertWindow_, uiFont_);
    EnableWindow(hwnd_, FALSE);
    ActivateSettingsWindow(alertWindow_);
}

void SettingsWindow::SaveThresholdSettings() {
    auto readValue = [](HWND edit) {
        wchar_t text[16]{};
        if (edit) GetWindowTextW(edit, text, _countof(text));
        return _wtoi(text);
    };

    const int cpuWarning = readValue(cpuWarning_);
    const int cpuCritical = readValue(cpuCritical_);
    const int gpuWarning = readValue(gpuWarning_);
    const int gpuCritical = readValue(gpuCritical_);
    const int ramWarning = readValue(ramWarningEdit_);
    const int ramCritical = readValue(ramCriticalEdit_);
    const int batteryWarning = readValue(batteryWarningEdit_);
    const int batteryCritical = readValue(batteryCriticalEdit_);

    const bool valid = cpuWarning >= 0 && cpuCritical <= 150 && cpuWarning < cpuCritical &&
                       gpuWarning >= 0 && gpuCritical <= 150 && gpuWarning < gpuCritical &&
                       ramWarning >= 0 && ramCritical <= 100 && ramWarning < ramCritical &&
                       batteryCritical >= 0 && batteryWarning <= 100 && batteryCritical < batteryWarning;
    if (!valid) {
        MessageBoxW(alertWindow_,
                    L"Invalid thresholds.\n\n"
                    L"CPU/GPU: 0..150 and Warning < Critical\n"
                    L"RAM: 0..100 and Warning < Critical\n"
                    L"Battery: 0..100 and Critical < Warning",
                    L"Threshold settings", MB_OK | MB_ICONWARNING);
        return;
    }

    thresholdColorsEnabled_ = SendMessageW(alertEnabled_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cpuTempWarning_ = cpuWarning;
    cpuTempCritical_ = cpuCritical;
    gpuTempWarning_ = gpuWarning;
    gpuTempCritical_ = gpuCritical;
    ramWarning_ = ramWarning;
    ramCritical_ = ramCritical;
    batteryWarning_ = batteryWarning;
    batteryCritical_ = batteryCritical;
    warningColor_ = alertWarningColor_;
    criticalColor_ = alertCriticalColor_;
    DestroyWindow(alertWindow_);
}

void SettingsWindow::UpdateFontDisplay() {
    if (!fontDisplay_) return;
    const int tenthsOfPoint = MulDiv(taskbarFontSize_, 720, 96);
    const int whole = tenthsOfPoint / 10;
    const int fraction = tenthsOfPoint % 10;
    std::wstring text = taskbarFont_.lfFaceName;
    text += L", " + std::to_wstring(whole);
    if (fraction != 0) text += L"." + std::to_wstring(fraction);
    text += L" pt";
    if (taskbarFont_.lfWeight >= FW_BOLD) text += L", Bold";
    SetWindowTextW(fontDisplay_, text.c_str());
}

void SettingsWindow::ChooseTaskbarFont() {
    HDC screen = GetDC(hwnd_);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen) ReleaseDC(hwnd_, screen);
    taskbarFont_.lfHeight = -MulDiv(taskbarFontSize_, dpi, 96);

    CHOOSEFONTW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = hwnd_;
    chooser.lpLogFont = &taskbarFont_;
    chooser.iPointSize = MulDiv(taskbarFontSize_, 720, 96);
    chooser.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS;
    if (ChooseFontW(&chooser)) {
        taskbarFontSize_ = std::max(6, std::min(32, MulDiv(chooser.iPointSize, 96, 720)));
        UpdateFontDisplay();
    }
}

void SettingsWindow::ShowHelp() {
    if (helpWindow_ && IsWindow(helpWindow_)) {
        ActivateSettingsWindow(helpWindow_);
        return;
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE));
    WNDCLASSW helpClass{};
    helpClass.lpfnWndProc = HelpProc;
    helpClass.hInstance = instance;
    helpClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    helpClass.hbrBackground = nullptr;
    helpClass.lpszClassName = L"TaskbarHardwareMonitorHelpWindow";
    RegisterClassW(&helpClass);

    helpWindow_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME, helpClass.lpszClassName, L"Taskbar Hardware Monitor - Help",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 680, 650, hwnd_, nullptr, instance, this);
    if (!helpWindow_) return;

    HWND text = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", HelpText,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_READONLY | ES_LEFT,
        18, 18, 626, 545, helpWindow_, nullptr, instance, nullptr);
    if (text) {
        SendMessageW(text, EM_SETSEL, 0, 0);
        SendMessageW(text, EM_SCROLLCARET, 0, 0);
    }
    CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  559, 575, 85, 28, helpWindow_,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(HelpCloseId)), instance, nullptr);

    ApplyTheme(helpWindow_, uiFont_);
    ActivateSettingsWindow(helpWindow_);
}

void SettingsWindow::ShowFormatVariables() {
    MessageBoxW(hwnd_,
        L"Available variables:\n\n"
        L"{cpu_temp}   CPU temperature, e.g. 54\u00B0\n"
        L"{cpu_usage}  CPU usage, e.g. 23%\n"
        L"{gpu_temp}   GPU temperature\n"
        L"{disk_temp}  Disk temperature\n"
        L"{ssd_temp}   Disk temperature (legacy alias)\n"
        L"{down}       Download speed\n"
        L"{up}         Upload speed\n"
        L"{power}      CPU power\n"
        L"{ram_usage}  RAM usage\n"
        L"{ram_used}   RAM used\n"
        L"{ram_total}  RAM total\n"
        L"{gpu_usage}  GPU usage\n"
        L"{vram}       VRAM used/total\n"
        L"{vram_used}  VRAM used\n"
        L"{vram_total} VRAM total\n"
        L"{disk_read}  Disk read speed\n"
        L"{disk_write} Disk write speed\n"
        L"{cpu_clock}  CPU clock\n"
        L"{gpu_power}  GPU power\n"
        L"{fan}        GPU fan speed (%)\n"
        L"{battery}    Battery level + short status\n"
        L"{battery_percent} Battery percentage only\n"
        L"{battery_status}  Battery status text\n"
        L"{system_power} Whole-system battery discharge power\n\n"
        L"Display format controls:\n"
        L"Enter         Start the second taskbar row directly\n"
        L"\\n            Start the second row; same effect as Enter\n"
        L"\\t            Start the next aligned column; matching columns align across rows\n\n"
        L"Format 2.0 modifiers:\n"
        L"{cpu_temp:1} one decimal   {cpu_clock:ghz} GHz   {ram_used:gb} GB\n"
        L"{down:kb|mb|gb} forced rate unit\n"
        L"Conditional: {gpu_temp?GPU:{gpu_temp}}\n"
        L"The body is hidden when the condition variable has no valid data.\n\n"
        L"Example:\n"
        L"温度:{cpu_temp}\\t占用:{cpu_usage}\\t内存:{ram_usage}\n"
        L"上行:{up}\\t下行:{down}\\t电池:{battery}\n\n"
        L"When Display format is not empty, it overrides the Full/Compact metric layout.",
        L"Taskbar format variables", MB_OK | MB_ICONINFORMATION);
}

void SettingsWindow::ShowFormatHint() {
    if (!formatHint_ || !formatEdit_) return;
    RECT edit{};
    GetWindowRect(formatEdit_, &edit);
    constexpr int hintWidth = 610;
    constexpr int hintHeight = 245;
    int left = edit.left;
    // Prefer the area above the format editor so the hint never covers Preview.
    int top = edit.top - hintHeight - 4;

    const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    if (GetMonitorInfoW(monitor, &info)) {
        const LONG clampedLeft = std::max<LONG>(
            info.rcWork.left + 4,
            std::min<LONG>(static_cast<LONG>(left), info.rcWork.right - hintWidth - 4));
        left = static_cast<int>(clampedLeft);
        // Very small / top-edge work areas are the only case where we fall back below.
        if (top < info.rcWork.top + 4) {
            top = edit.bottom + 4;
        }
    }
    SetWindowPos(formatHint_, HWND_TOP, left, top, hintWidth, hintHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void SettingsWindow::HideFormatHint() {
    if (formatHint_) ShowWindow(formatHint_, SW_HIDE);
}

void SettingsWindow::ResetTaskbarFormat() {
    if (!formatEdit_) return;
    SetWindowTextW(formatEdit_, L"");
    UpdateFormatPreview();
}

void SettingsWindow::SyncMetricsFromFormat() {
    if (!formatEdit_) return;
    wchar_t format[4096]{};
    GetWindowTextW(formatEdit_, format, _countof(format));
    const bool custom = format[0] != L'\0';
    if (custom) {
        const std::uint32_t demand = SensorDemandFromFormat(format);
        for (int index = 0; index != 15; ++index) {
            const bool selected = (demand & SensorDemandForMetricIndex(index)) != 0;
            SendMessageW(taskbarChecks_[index], BM_SETCHECK,
                         selected ? BST_CHECKED : BST_UNCHECKED, 0);
            EnableWindow(taskbarChecks_[index], FALSE);
        }
    } else {
        for (HWND check : taskbarChecks_) EnableWindow(check, TRUE);
    }
}

void SettingsWindow::UpdateFormatPreview() {
    if (!formatEdit_ || !formatPreview_) return;
    wchar_t format[4096]{};
    GetWindowTextW(formatEdit_, format, _countof(format));
    SyncMetricsFromFormat();
    if (format[0] == L'\0') {
        SetWindowTextW(formatPreview_, L"(Default Full/Compact layout)");
        return;
    }
    SensorSnapshot sample{};
    sample.cpuTemperature = 54.0;
    sample.cpuTemperatureValid = true;
    sample.cpuUsage = 23.0;
    sample.cpuUsageValid = true;
    sample.gpuTemperature = 47.0;
    sample.gpuTemperatureValid = true;
    sample.diskTemperature = 39.0;
    sample.diskTemperatureValid = true;
    sample.cpuPower = 42.0;
    sample.cpuPowerValid = true;
    sample.memoryUsage = 63.0;
    sample.memoryUsedBytes = 12ULL * 1024ULL * 1024ULL * 1024ULL;
    sample.memoryTotalBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    sample.memoryValid = true;
    sample.cpuClockMHz = 3400.0;
    sample.cpuClockValid = true;
    sample.gpuUsage = 24.0;
    sample.gpuUsageValid = true;
    sample.gpuMemoryUsedBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    sample.gpuMemoryTotalBytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    sample.gpuMemoryValid = true;
    sample.gpuPower = 18.0;
    sample.gpuPowerValid = true;
    sample.gpuFanPercent = 42.0;
    sample.gpuFanValid = true;
    sample.diskReadBytesPerSecond = 18ULL * 1024ULL * 1024ULL;
    sample.diskWriteBytesPerSecond = 3ULL * 1024ULL * 1024ULL;
    sample.diskIoValid = true;
    sample.batteryPercent = 78.0;
    sample.batteryValid = true;
    sample.batteryState = BatteryCharging;
    sample.systemPower = 26.0;
    sample.systemPowerValid = true;
    sample.networkValid = true;
    sample.downloadBytesPerSecond = 12ULL * 1024ULL * 1024ULL;
    sample.uploadBytesPerSecond = 2ULL * 1024ULL * 1024ULL;
    const auto preview = BuildFormattedTaskbarLayout(sample, format);
    std::wstring text;
    for (int row = 0; row < preview.rows; ++row) {
        if (row != 0) text += L"\r\n";
        int currentColumn = 0;
        for (const auto& run : preview.runs) {
            if (run.row != row) continue;
            while (currentColumn < run.column) {
                text += L"    ";
                ++currentColumn;
            }
            text += run.text;
        }
    }
    SetWindowTextW(formatPreview_, text.c_str());
}

void SettingsWindow::SaveAndClose() {
    Config updated = *config_;
    updated.taskbarEnabled = SendMessageW(taskbarEnabled_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.displayMode = SendMessageW(displayMode_, CB_GETCURSEL, 0, 0) == 1
        ? DisplayMode::Compact : DisplayMode::Full;
    updated.showCpuTemperature = SendMessageW(taskbarChecks_[0], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showCpuUsage = SendMessageW(taskbarChecks_[1], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showGpuTemperature = SendMessageW(taskbarChecks_[2], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showDiskTemperature = SendMessageW(taskbarChecks_[3], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showNetwork = SendMessageW(taskbarChecks_[4], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showPower = SendMessageW(taskbarChecks_[5], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showMemory = SendMessageW(taskbarChecks_[6], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showGpuUsage = SendMessageW(taskbarChecks_[7], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showVram = SendMessageW(taskbarChecks_[8], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showDiskIo = SendMessageW(taskbarChecks_[9], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showCpuClock = SendMessageW(taskbarChecks_[10], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showGpuPower = SendMessageW(taskbarChecks_[11], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showFan = SendMessageW(taskbarChecks_[12], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showBattery = SendMessageW(taskbarChecks_[13], BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.showSystemPower = SendMessageW(taskbarChecks_[14], BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (diskSource_) {
        const LRESULT selection = SendMessageW(diskSource_, CB_GETCURSEL, 0, 0);
        if (selection != CB_ERR) {
            const LRESULT drive = SendMessageW(
                diskSource_, CB_GETITEMDATA, static_cast<WPARAM>(selection), 0);
            if (drive != CB_ERR) updated.storageDriveIndex = static_cast<int>(drive);
        }
    }
    wchar_t interval[16]{};
    GetWindowTextW(interval_, interval, _countof(interval));
    const int value = _wtoi(interval);
    if (value >= 250 && value <= 10000) updated.refreshIntervalMs = value;
    updated.startWithWindows = SendMessageW(startup_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.taskbarValueColorCustom = valueColorCustom_;
    updated.taskbarValueColor = static_cast<unsigned int>(valueColor_ & 0x00FFFFFFu);
    updated.thresholdColorsEnabled = thresholdColorsEnabled_;
    updated.cpuTempWarning = cpuTempWarning_;
    updated.cpuTempCritical = cpuTempCritical_;
    updated.gpuTempWarning = gpuTempWarning_;
    updated.gpuTempCritical = gpuTempCritical_;
    updated.ramWarning = ramWarning_;
    updated.ramCritical = ramCritical_;
    updated.batteryWarning = batteryWarning_;
    updated.batteryCritical = batteryCritical_;
    updated.warningColor = static_cast<unsigned int>(warningColor_ & 0x00FFFFFFu);
    updated.criticalColor = static_cast<unsigned int>(criticalColor_ & 0x00FFFFFFu);
    updated.taskbarFontName = taskbarFont_.lfFaceName;
    updated.taskbarFontSize = taskbarFontSize_;
    updated.taskbarFontWeight = std::max(100L, std::min(900L, taskbarFont_.lfWeight));
    wchar_t taskbarFormat[4096]{};
    if (formatEdit_) GetWindowTextW(formatEdit_, taskbarFormat, _countof(taskbarFormat));
    updated.taskbarFormat = taskbarFormat;
    if (!updated.taskbarFormat.empty()) {
        ApplySensorDemandToMetrics(updated, SensorDemandFromFormat(updated.taskbarFormat));
    }
    SaveBandValueColor(valueColorCustom_, valueColor_);
    SaveBandAlertSettings(updated);
    SaveBandFont(updated.taskbarFontName, updated.taskbarFontSize, updated.taskbarFontWeight);
    SaveBandFormat(updated.taskbarFormat);
    *config_ = updated;
    if (!config_->Save()) {
        MessageBoxW(hwnd_,
                    L"Settings were written, but the Windows startup task could not be updated.\n\n"
                    L"Taskbar Hardware Monitor requires administrator rights for hardware access. "
                    L"Please run the monitor as administrator and save the setting again.",
                    L"Startup configuration failed", MB_OK | MB_ICONWARNING);
        return;
    }
    if (owner_) PostMessageW(owner_, WM_APP + 3, 0, 0);
    DestroyWindow(hwnd_);
}

LRESULT CALLBACK SettingsWindow::AlertProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && message == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case AlertSaveId: self->SaveThresholdSettings(); return 0;
        case AlertCancelId: DestroyWindow(window); return 0;
        case WarningColorChooseId: self->ChooseWarningColor(); return 0;
        case CriticalColorChooseId: self->ChooseCriticalColor(); return 0;
        default: break;
        }
    }
    if (self && message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && (draw->CtlID == WarningColorPreviewId || draw->CtlID == CriticalColorPreviewId)) {
            const COLORREF color = draw->CtlID == WarningColorPreviewId
                ? self->alertWarningColor_ : self->alertCriticalColor_;
            HBRUSH fill = CreateSolidBrush(color);
            FillRect(draw->hDC, &draw->rcItem, fill);
            DeleteObject(fill);
            FrameRect(draw->hDC, &draw->rcItem,
                      reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            return TRUE;
        }
    }
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT || message == WM_CTLCOLORBTN) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, TextColor);
        SetBkColor(dc, PanelBackground);
        static HBRUSH brush = CreateSolidBrush(PanelBackground);
        return reinterpret_cast<LRESULT>(brush);
    }
    if (message == WM_ERASEBKGND) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        static HBRUSH brush = CreateSolidBrush(WindowBackground);
        FillRect(dc, &client, brush);
        return 1;
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY && self) {
        if (self->alertWindow_ == window) self->alertWindow_ = nullptr;
        self->alertEnabled_ = nullptr;
        self->cpuWarning_ = nullptr;
        self->cpuCritical_ = nullptr;
        self->gpuWarning_ = nullptr;
        self->gpuCritical_ = nullptr;
        self->ramWarningEdit_ = nullptr;
        self->ramCriticalEdit_ = nullptr;
        self->batteryWarningEdit_ = nullptr;
        self->batteryCriticalEdit_ = nullptr;
        self->warningColorPreview_ = nullptr;
        self->criticalColorPreview_ = nullptr;
        if (self->hwnd_ && IsWindow(self->hwnd_)) {
            EnableWindow(self->hwnd_, TRUE);
            ActivateSettingsWindow(self->hwnd_);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::HelpProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_COMMAND && LOWORD(wParam) == HelpCloseId) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT || message == WM_CTLCOLORBTN) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, TextColor);
        SetBkColor(dc, PanelBackground);
        static HBRUSH brush = CreateSolidBrush(PanelBackground);
        return reinterpret_cast<LRESULT>(brush);
    }
    if (message == WM_ERASEBKGND) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        static HBRUSH brush = CreateSolidBrush(WindowBackground);
        FillRect(dc, &client, brush);
        return 1;
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY && self) {
        if (self->helpWindow_ == window) self->helpWindow_ = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && message == WM_COMMAND) {
        if (LOWORD(wParam) == SaveId) self->SaveAndClose();
        if (LOWORD(wParam) == CloseId) DestroyWindow(window);
        if (LOWORD(wParam) == ValueColorChooseId) self->ChooseValueColor();
        if (LOWORD(wParam) == ValueColorAutoId) self->SetAutomaticValueColor();
        if (LOWORD(wParam) == AlertSettingsId) self->ShowThresholdSettings();
        if (LOWORD(wParam) == FontChooseId) self->ChooseTaskbarFont();
        if (LOWORD(wParam) == FormatVariablesId) self->ShowFormatVariables();
        if (LOWORD(wParam) == FormatResetId) self->ResetTaskbarFormat();
        if (LOWORD(wParam) == HelpId) self->ShowHelp();
        if (LOWORD(wParam) == FormatEditId) {
            if (HIWORD(wParam) == EN_SETFOCUS) self->ShowFormatHint();
            if (HIWORD(wParam) == EN_KILLFOCUS) self->HideFormatHint();
            if (HIWORD(wParam) == EN_CHANGE) {
                self->UpdateFormatPreview();
                if (GetFocus() == self->formatEdit_) self->ShowFormatHint();
            }
        }
        return 0;
    }
    if (self && message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlID == ValueColorPreviewId) {
            const COLORREF preview = self->valueColorCustom_
                ? self->valueColor_
                : (IsWindowEnabled(window) ? RGB(245, 245, 245) : RGB(180, 180, 180));
            HBRUSH fill = CreateSolidBrush(preview);
            FillRect(draw->hDC, &draw->rcItem, fill);
            DeleteObject(fill);
            FrameRect(draw->hDC, &draw->rcItem,
                      reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            return TRUE;
        }
    }
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT || message == WM_CTLCOLORBTN) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, TextColor);
        SetBkColor(dc, PanelBackground);
        static HBRUSH brush = CreateSolidBrush(PanelBackground);
        return reinterpret_cast<LRESULT>(brush);
    }
    if (message == WM_ERASEBKGND) {
        const HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        static HBRUSH brush = CreateSolidBrush(WindowBackground);
        FillRect(dc, &client, brush);
        return 1;
    }
    if (self && message == WM_MOVE && self->formatHint_ && IsWindowVisible(self->formatHint_)) {
        self->ShowFormatHint();
    }
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY && self) {
        if (self->alertWindow_ && IsWindow(self->alertWindow_)) DestroyWindow(self->alertWindow_);
        self->alertWindow_ = nullptr;
        if (self->helpWindow_ && IsWindow(self->helpWindow_)) DestroyWindow(self->helpWindow_);
        self->helpWindow_ = nullptr;
        if (self->formatHint_) DestroyWindow(self->formatHint_);
        self->formatHint_ = nullptr;
        if (self->uiFont_) DeleteObject(self->uiFont_);
        self->uiFont_ = nullptr;
        self->hwnd_ = nullptr;
        self->owner_ = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace monitor
