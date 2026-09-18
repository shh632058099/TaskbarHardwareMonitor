#include "TaskbarBand.h"

#include <windows.h>
#include <new>
#include <string>

namespace {
HMODULE g_module = nullptr;
LONG g_objects = 0;
LONG g_locks = 0;
}

namespace monitor {

void ModuleAddObject() { InterlockedIncrement(&g_objects); }
void ModuleReleaseObject() { InterlockedDecrement(&g_objects); }

constexpr CLSID TaskbarBandClsid =
    {0x65c3a923, 0x7a8e, 0x4a54, {0x9e, 0x3a, 0x22, 0xc8, 0xe2, 0x5a, 0x9d, 0x01}};

class ClassFactory final : public IClassFactory {
public:
    ClassFactory() : refs_(1) { InterlockedIncrement(&g_objects); }
    ~ClassFactory() { InterlockedDecrement(&g_objects); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (iid != IID_IUnknown && iid != IID_IClassFactory) return E_NOINTERFACE;
        *result = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&refs_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const auto refs = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (refs == 0) delete this;
        return refs;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** result) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* band = new (std::nothrow) TaskbarBand();
        if (!band) return E_OUTOFMEMORY;
        const HRESULT status = band->QueryInterface(iid, result);
        band->Release();
        return status;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_locks);
        else InterlockedDecrement(&g_locks);
        return S_OK;
    }

private:
    LONG refs_;
};

} // namespace monitor

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(
    REFCLSID clsid, REFIID iid, void** result) {
    if (!result) return E_POINTER;
    *result = nullptr;
    if (clsid != monitor::TaskbarBandClsid) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) monitor::ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT status = factory->QueryInterface(iid, result);
    factory->Release();
    return status;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow() {
    return g_objects == 0 && g_locks == 0 ? S_OK : S_FALSE;
}

namespace {

constexpr wchar_t ClassIdText[] = L"{65C3A923-7A8E-4A54-9E3A-22C8E25A9D01}";
constexpr wchar_t CategoryIdText[] = L"{00021492-0000-0000-C000-000000000046}";
constexpr wchar_t ApprovedPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

HRESULT SetRegistryValue(HKEY root, const std::wstring& path, const wchar_t* name,
                         const std::wstring& value) {
    HKEY key{};
    DWORD disposition = 0;
    const LSTATUS openStatus = RegCreateKeyExW(
        root, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, nullptr, &key, &disposition);
    if (openStatus != ERROR_SUCCESS) return HRESULT_FROM_WIN32(openStatus);
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LSTATUS setStatus = RegSetValueExW(
        key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), bytes);
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(setStatus);
}

HRESULT DeleteRegistryTree(HKEY root, const std::wstring& path) {
    const LSTATUS status = RegDeleteTreeW(root, path.c_str());
    if (status == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(status);
}

} // namespace

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(g_module, modulePath, ARRAYSIZE(modulePath));
    if (!length || length >= ARRAYSIZE(modulePath)) return HRESULT_FROM_WIN32(GetLastError());

    const std::wstring clsidPath = L"Software\\Classes\\CLSID\\" +
        std::wstring(ClassIdText);
    HRESULT status = SetRegistryValue(HKEY_CURRENT_USER, clsidPath, nullptr,
                                      L"Taskbar Hardware Monitor");
    if (FAILED(status)) return status;
    status = SetRegistryValue(HKEY_CURRENT_USER, clsidPath + L"\\InprocServer32",
                              nullptr, modulePath);
    if (FAILED(status)) return status;
    status = SetRegistryValue(HKEY_CURRENT_USER, clsidPath + L"\\InprocServer32",
                              L"ThreadingModel", L"Apartment");
    if (FAILED(status)) return status;
    status = SetRegistryValue(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\CLSID\\" + std::wstring(ClassIdText) +
            L"\\Implemented Categories\\" + CategoryIdText,
        nullptr, L"");
    if (FAILED(status)) return status;
    return SetRegistryValue(HKEY_CURRENT_USER, ApprovedPath, ClassIdText,
                            L"Taskbar Hardware Monitor");
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer() {
    const std::wstring path = L"Software\\Classes\\CLSID\\" +
        std::wstring(ClassIdText);
    const HRESULT status = DeleteRegistryTree(HKEY_CURRENT_USER, path);
    HKEY approved{};
    RegOpenKeyExW(HKEY_CURRENT_USER, ApprovedPath, 0, KEY_SET_VALUE, &approved);
    if (approved) {
        RegDeleteValueW(approved, ClassIdText);
        RegCloseKey(approved);
    }
    return status;
}
