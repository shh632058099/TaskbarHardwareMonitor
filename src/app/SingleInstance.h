#pragma once
#include <windows.h>

namespace monitor {
class SingleInstance {
public:
    bool Acquire() { mutex_=CreateMutexW(nullptr,TRUE,L"Local\\TaskbarHardwareMonitor.SingleInstance"); return mutex_ && GetLastError()!=ERROR_ALREADY_EXISTS; }
    ~SingleInstance() { if(mutex_) CloseHandle(mutex_); }
private: HANDLE mutex_{};
};
}
