#include <windows.h>
#include <objbase.h>
#include <cstdio>

namespace {

// ITrayDeskBand is a documented Shell interface available since Windows Vista.
// Define the tiny ABI locally so the helper also builds with SDKs where the
// interface declaration is hidden by version guards.
struct __declspec(uuid("6D67E846-5B9C-4DB8-9CBC-DDE12F4254F1")) ITrayDeskBandLocal : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE ShowDeskBand(REFCLSID clsid) = 0;
    virtual HRESULT STDMETHODCALLTYPE HideDeskBand(REFCLSID clsid) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsDeskBandShown(REFCLSID clsid) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeskBandRegistrationChanged() = 0;
};

constexpr CLSID TrayDeskBandClsid =
    {0xe6442437, 0x6c68, 0x4f52, {0x94, 0xdd, 0x2c, 0xfe, 0xd2, 0x67, 0xef, 0xb9}};
constexpr CLSID HardwareMonitorBandClsid =
    {0x65c3a923, 0x7a8e, 0x4a54, {0x9e, 0x3a, 0x22, 0xc8, 0xe2, 0x5a, 0x9d, 0x01}};

void PrintFailure(const wchar_t* action, HRESULT hr) {
    ::fwprintf(stderr, L"%ls failed: 0x%08lX\n", action,
                  static_cast<unsigned long>(hr));
}

} // namespace

int wmain() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init)) {
        PrintFailure(L"CoInitializeEx", init);
        return 1;
    }

    ITrayDeskBandLocal* tray = nullptr;
    HRESULT hr = CoCreateInstance(
        TrayDeskBandClsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER,
        __uuidof(ITrayDeskBandLocal), reinterpret_cast<void**>(&tray));
    if (FAILED(hr) || !tray) {
        PrintFailure(L"CoCreateInstance(CLSID_TrayDeskBand)", hr);
        CoUninitialize();
        return 2;
    }

    // Explorer caches registered deskbands. Refresh that cache before asking
    // it to show a newly registered band.
    hr = tray->DeskBandRegistrationChanged();
    if (FAILED(hr)) {
        PrintFailure(L"DeskBandRegistrationChanged", hr);
        tray->Release();
        CoUninitialize();
        return 3;
    }

    hr = tray->ShowDeskBand(HardwareMonitorBandClsid);
    if (FAILED(hr)) {
        PrintFailure(L"ShowDeskBand", hr);
        tray->Release();
        CoUninitialize();
        return 4;
    }

    // Explorer applies ShowDeskBand asynchronously. Give the taskbar a short,
    // bounded window to finish creating the band instead of treating the first
    // transient S_FALSE as a hard failure.
    for (int attempt = 0; attempt != 20; ++attempt) {
        hr = tray->IsDeskBandShown(HardwareMonitorBandClsid);
        if (hr == S_OK) break;
        if (FAILED(hr)) break;
        Sleep(100);
    }
    if (hr != S_OK) {
        PrintFailure(L"IsDeskBandShown", hr);
        tray->Release();
        CoUninitialize();
        return 5;
    }

    ::wprintf(L"Hardware Monitor DeskBand is shown.\n");
    tray->Release();
    CoUninitialize();
    return 0;
}
