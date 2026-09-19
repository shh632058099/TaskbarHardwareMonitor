#include "App.h"

#include <windows.h>
#include <algorithm>

namespace monitor {

bool App::Initialize(HINSTANCE instance) {
    if (!instanceGuard_.Acquire()) {
        return false;
    }

    instance_ = instance;
    wchar_t executablePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    config_.path = executablePath;
    const auto separator = config_.path.find_last_of(L"\\/");
    config_.path = config_.path.substr(0, separator + 1) + L"config.json";
    config_.Load();
    // Migrate the legacy HKCU Run entry to the elevated logon task automatically.
    // Startup registration failure must not prevent the monitor from running manually.
    config_.ApplyStartupSetting();
    if (!snapshotPublisher_.Open()) {
        return false;
    }

    sensors_.SetNetworkAdapter(config_.networkAdapter);
    sensors_.SetStorageDrive(config_.storageDriveIndex);
    refreshIntervalMs_ = config_.refreshIntervalMs;
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = Proc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"TaskbarMonitorHost";
    RegisterClassW(&windowClass);

    hwnd_ = CreateWindowExW(
        0, windowClass.lpszClassName, L"", 0, 0, 0, 0, 0,
        nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }

    worker_ = std::thread(&App::Worker, this);
    return true;
}

void App::RequestRefresh() {
    {
        std::lock_guard<std::mutex> lock(wakeMutex_);
        refreshRequested_ = true;
    }
    snapshotPublisher_.Wake();
}

void App::Worker() {
    SensorSnapshot sample{};
    bool haveSample = false;
    ULONGLONG lastCollectionTick = 0;
    ULONGLONG lastPublishTick = 0;

    while (!stop_) {
        bool commandRequestsRefresh = false;
        SharedBandCommand command{};
        while (snapshotPublisher_.ReadCommand(command)) {
            std::lock_guard<std::mutex> configLock(configMutex_);
            if (command.action == BandCommandSetDisplay) {
                config_.displayMode = command.displayMode ? DisplayMode::Compact : DisplayMode::Full;
                commandRequestsRefresh = true;
            } else if (command.action == BandCommandSetMetrics) {
                config_.taskbarEnabled = (command.displayFlags & TaskbarEnabled) != 0;
                if (config_.taskbarFormat.empty()) {
                    config_.showCpuTemperature = (command.displayFlags & ShowCpuTemperature) != 0;
                    config_.showCpuUsage = (command.displayFlags & ShowCpuUsage) != 0;
                    config_.showGpuTemperature = (command.displayFlags & ShowGpuTemperature) != 0;
                    config_.showDiskTemperature = (command.displayFlags & ShowDiskTemperature) != 0;
                    config_.showNetwork = (command.displayFlags & ShowNetwork) != 0;
                    config_.showPower = (command.displayFlags & ShowPower) != 0;
                    config_.showMemory = (command.displayFlags & ShowMemory) != 0;
                    config_.showGpuUsage = (command.displayFlags & ShowGpuUsage) != 0;
                    config_.showVram = (command.displayFlags & ShowVram) != 0;
                    config_.showDiskIo = (command.displayFlags & ShowDiskIo) != 0;
                    config_.showCpuClock = (command.displayFlags & ShowCpuClock) != 0;
                    config_.showGpuPower = (command.displayFlags & ShowGpuPower) != 0;
                    config_.showFan = (command.displayFlags & ShowFan) != 0;
                    config_.showBattery = (command.displayFlags & ShowBattery) != 0;
                    config_.showSystemPower = (command.displayFlags & ShowSystemPower) != 0;
                }
                commandRequestsRefresh = true;
            } else if (command.action == BandCommandOpenSettings) {
                PostMessageW(hwnd_, WM_APP + 4, 0, 0);
            } else if (command.action == BandCommandExit) {
                PostMessageW(hwnd_, WM_APP + 5, 0, 0);
            }
        }

        bool forceRefresh = commandRequestsRefresh;
        {
            std::lock_guard<std::mutex> lock(wakeMutex_);
            forceRefresh = forceRefresh || refreshRequested_;
            refreshRequested_ = false;
        }

        const ULONGLONG now = GetTickCount64();
        const DWORD collectionInterval = static_cast<DWORD>(refreshIntervalMs_.load());
        const bool collectionDue = !haveSample || forceRefresh ||
            now - lastCollectionTick >= collectionInterval;

        if (collectionDue) {
            std::uint32_t demand = 0;
            {
                std::lock_guard<std::mutex> configLock(configMutex_);
                demand = SensorDemandFromConfig(config_);
            }
            sample = sensors_.Update(demand, forceRefresh);
            haveSample = true;
            lastCollectionTick = GetTickCount64();
            {
                std::lock_guard<std::mutex> configLock(configMutex_);
                snapshotPublisher_.Publish(sample, config_);
            }
            lastPublishTick = GetTickCount64();
        } else if (lastPublishTick == 0 ||
                   now - lastPublishTick >= SnapshotHeartbeatIntervalMs) {
            // Heartbeat refreshes IPC liveness only; it does not query hardware sensors.
            {
                std::lock_guard<std::mutex> configLock(configMutex_);
                snapshotPublisher_.Publish(sample, config_);
            }
            lastPublishTick = GetTickCount64();
        }

        const ULONGLONG waitStart = GetTickCount64();
        const ULONGLONG collectionElapsed = waitStart - lastCollectionTick;
        const ULONGLONG publishElapsed = waitStart - lastPublishTick;
        const DWORD untilCollection = collectionElapsed >= collectionInterval
            ? 0u : static_cast<DWORD>(collectionInterval - collectionElapsed);
        const DWORD untilHeartbeat = publishElapsed >= SnapshotHeartbeatIntervalMs
            ? 0u : static_cast<DWORD>(SnapshotHeartbeatIntervalMs - publishElapsed);
        const DWORD timeout = (std::min)(untilCollection, untilHeartbeat);
        snapshotPublisher_.WaitForWake(timeout);
    }
}

int App::Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    stop_ = true;
    snapshotPublisher_.Wake();
    if (worker_.joinable()) {
        worker_.join();
    }
    config_.Save();
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self && message == WM_APP + 3) {
        {
            std::lock_guard<std::mutex> configLock(self->configMutex_);
            self->sensors_.SetNetworkAdapter(self->config_.networkAdapter);
            self->sensors_.SetStorageDrive(self->config_.storageDriveIndex);
            self->refreshIntervalMs_ = self->config_.refreshIntervalMs;
        }
        self->RequestRefresh();
        return 0;
    }
    if (self && message == WM_APP + 4) {
        std::lock_guard<std::mutex> configLock(self->configMutex_);
        self->settings_.Show(self->instance_, window, &self->config_);
        return 0;
    }
    if (self && message == WM_APP + 5) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        if (self) {
            self->stop_ = true;
            self->snapshotPublisher_.Wake();
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace monitor
