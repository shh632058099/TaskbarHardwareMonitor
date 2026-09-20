#pragma once

#include "../ipc/SharedSensorSnapshot.h"

#include <windows.h>
#include <shobjidl.h>
#include <ocidl.h>
#include <objbase.h>

namespace monitor {

class TaskbarBand final : public IDeskBand2, public IObjectWithSite,
                          public IPersistStream, public IInputObject {
public:
    TaskbarBand();
    ~TaskbarBand();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE GetWindow(HWND*) override;
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override;
    HRESULT STDMETHODCALLTYPE ShowDW(BOOL) override;
    HRESULT STDMETHODCALLTYPE CloseDW(DWORD) override;
    HRESULT STDMETHODCALLTYPE ResizeBorderDW(const RECT*, IUnknown*, BOOL) override;
    HRESULT STDMETHODCALLTYPE GetBandInfo(DWORD, DWORD, DESKBANDINFO*) override;
    HRESULT STDMETHODCALLTYPE CanRenderComposited(BOOL*) override;
    HRESULT STDMETHODCALLTYPE SetCompositionState(BOOL) override;
    HRESULT STDMETHODCALLTYPE GetCompositionState(BOOL*) override;

    HRESULT STDMETHODCALLTYPE SetSite(IUnknown*) override;
    HRESULT STDMETHODCALLTYPE GetSite(REFIID, void**) override;

    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*) override;
    HRESULT STDMETHODCALLTYPE IsDirty(void) override;
    HRESULT STDMETHODCALLTYPE Load(IStream*) override;
    HRESULT STDMETHODCALLTYPE Save(IStream*, BOOL) override;
    HRESULT STDMETHODCALLTYPE GetSizeMax(ULARGE_INTEGER*) override;

    HRESULT STDMETHODCALLTYPE UIActivateIO(BOOL, LPMSG) override;
    HRESULT STDMETHODCALLTYPE HasFocusIO(void) override;
    HRESULT STDMETHODCALLTYPE TranslateAcceleratorIO(LPMSG) override;

private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    static VOID CALLBACK SnapshotEventCallback(PVOID, BOOLEAN);
    static VOID CALLBACK SettingsEventCallback(PVOID, BOOLEAN);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    bool ReadSnapshot(SharedSensorSnapshot&);
    void ConnectSharedMemory();
    void ConnectSettingsNotifications();
    void ArmSettingsNotification();
    void RefreshSnapshotState(HWND, bool settingsChanged = false);
    void Paint(HDC);
    void SafeClose();
    void UpdateBandSize(int width);

    LONG refs_ = 1;
    HWND hwnd_{};
    IUnknown* site_{};
    HANDLE mapping_{};
    const SharedSensorSnapshot* shared_{};
    HANDLE commandMapping_{};
    SharedBandCommand* command_{};
    HANDLE snapshotEvent_{};
    HANDLE commandEvent_{};
    HANDLE snapshotWait_{};
    HKEY settingsKey_{};
    HANDLE settingsEvent_{};
    HANDLE settingsWait_{};
    HWND tooltip_{};
    std::wstring tooltipText_;
    SharedSensorSnapshot lastVisualSnapshot_{};
    bool hasLastVisualSnapshot_ = false;
    bool shellShowRequested_ = true;
    bool monitorReady_ = false;
    bool monitorEnabled_ = false;
    SIZE idealSize_{428, 1};
};

} // namespace monitor
