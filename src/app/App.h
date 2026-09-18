#pragma once

#include "SingleInstance.h"
#include "../config/Config.h"
#include "../monitor/SensorManager.h"
#include "../ui/SettingsWindow.h"
#include "../ipc/SharedSnapshotPublisher.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace monitor {

class App {
public:
    bool Initialize(HINSTANCE instance);
    int Run();

private:
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void Worker();
    void RequestRefresh();

    HINSTANCE instance_{};
    HWND hwnd_{};
    Config config_{};
    SensorManager sensors_{};
    SettingsWindow settings_{};
    SingleInstance instanceGuard_{};
    SharedSnapshotPublisher snapshotPublisher_{};
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<int> refreshIntervalMs_{1000};
    std::mutex configMutex_;
    std::condition_variable wake_;
    std::mutex wakeMutex_;
    bool refreshRequested_ = false;
};

} // namespace monitor
