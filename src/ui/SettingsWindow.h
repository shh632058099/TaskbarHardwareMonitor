#pragma once

#include "../config/Config.h"

#include <windows.h>

namespace monitor {

class SettingsWindow {
public:
    bool Show(HINSTANCE instance, HWND owner, Config* config);

private:
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK HelpProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK AlertProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void SaveAndClose();
    void ChooseValueColor();
    void SetAutomaticValueColor();
    void ChooseTaskbarFont();
    void UpdateFontDisplay();
    void ShowFormatVariables();
    void ShowHelp();
    void ShowThresholdSettings();
    void SaveThresholdSettings();
    void ChooseWarningColor();
    void ChooseCriticalColor();
    void ShowFormatHint();
    void HideFormatHint();
    void ResetTaskbarFormat();
    void UpdateFormatPreview();
    void SyncMetricsFromFormat();

    HWND hwnd_{};
    HWND interval_{};
    HWND startup_{};
    HWND taskbarEnabled_{};
    HWND displayMode_{};
    HWND diskSource_{};
    HWND valueColorPreview_{};
    HWND fontDisplay_{};
    HWND formatEdit_{};
    HWND formatPreview_{};
    HWND formatHint_{};
    HWND helpWindow_{};
    HWND alertWindow_{};
    HWND alertEnabled_{};
    HWND cpuWarning_{};
    HWND cpuCritical_{};
    HWND gpuWarning_{};
    HWND gpuCritical_{};
    HWND ramWarningEdit_{};
    HWND ramCriticalEdit_{};
    HWND batteryWarningEdit_{};
    HWND batteryCriticalEdit_{};
    HWND warningColorPreview_{};
    HWND criticalColorPreview_{};
    HWND taskbarChecks_[15]{};
    HWND owner_{};
    HFONT uiFont_{};
    Config* config_{};
    LOGFONTW taskbarFont_{};
    int taskbarFontSize_ = 11;
    COLORREF valueColor_ = RGB(245, 245, 245);
    bool valueColorCustom_ = false;
    bool thresholdColorsEnabled_ = false;
    int cpuTempWarning_ = 75;
    int cpuTempCritical_ = 90;
    int gpuTempWarning_ = 75;
    int gpuTempCritical_ = 90;
    int ramWarning_ = 85;
    int ramCritical_ = 95;
    int batteryWarning_ = 20;
    int batteryCritical_ = 10;
    COLORREF warningColor_ = RGB(255, 190, 0);
    COLORREF criticalColor_ = RGB(255, 80, 80);
    COLORREF alertWarningColor_ = RGB(255, 190, 0);
    COLORREF alertCriticalColor_ = RGB(255, 80, 80);
};

} // namespace monitor
