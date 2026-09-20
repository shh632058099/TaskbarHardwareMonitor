#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "SensorTypes.h"
#include "NetworkProvider.h"

namespace monitor {
class NetworkMonitor : public INetworkProvider {
public:
    void SetAdapter(const std::wstring& adapter) {
        if (adapter_ == adapter) return;
        adapter_ = adapter;
        cachedInterfaceIndices_.clear();
        lastInterfaceScanTick_ = 0;
        initialized_ = false;
    }
    void Update(SensorSnapshot& snapshot) override;
    void ResetSampling() override { initialized_ = false; }
private:
    bool RefreshInterfaceCache(std::uint64_t& inputBytes, std::uint64_t& outputBytes);
    bool ReadCachedInterfaces(std::uint64_t& inputBytes, std::uint64_t& outputBytes) const;
    std::wstring adapter_ = L"auto";
    std::vector<DWORD> cachedInterfaceIndices_;
    ULONGLONG lastInterfaceScanTick_ = 0;
    std::uint64_t in_ = 0, out_ = 0;
    ULONGLONG tick_ = 0;
    bool initialized_ = false;
};
}
