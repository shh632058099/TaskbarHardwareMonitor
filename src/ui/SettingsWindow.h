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
    void SaveAndClose();
    void ChooseValueColor();
    void SetAutomaticValueColor();
    void ChooseTaskbarFont();
    void UpdateFontDisplay();
    void ShowFormatVariables();
    void ShowHelp();
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
    HWND taskbarChecks_[15]{};
    HWND owner_{};
    HFONT uiFont_{};
    Config* config_{};
    LOGFONTW taskbarFont_{};
    int taskbarFontSize_ = 11;
    COLORREF valueColor_ = RGB(245, 245, 245);
    bool valueColorCustom_ = false;
};

} // namespace monitor
