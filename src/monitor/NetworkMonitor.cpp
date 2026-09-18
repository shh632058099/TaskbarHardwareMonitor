#include "NetworkMonitor.h"

#include <windows.h>
#include <iphlpapi.h>
#include <ipifcons.h>

#include <algorithm>
#include <cwctype>
#include <vector>

namespace monitor {
namespace {

void Lowercase(std::wstring& value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(towlower(character));
    });
}

bool IsVirtualAdapter(const std::wstring& name) {
    return name.find(L"hyper-v") != std::wstring::npos ||
           name.find(L"vmware") != std::wstring::npos ||
           name.find(L"virtualbox") != std::wstring::npos ||
           name.find(L"wsl") != std::wstring::npos ||
           name.find(L"vpn") != std::wstring::npos;
}

} // namespace

bool NetworkMonitor::RefreshInterfaceCache(std::uint64_t& inputBytes,
                                          std::uint64_t& outputBytes) {
    DWORD size = 0;
    if (GetIfTable(nullptr, &size, TRUE) != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<PMIB_IFTABLE>(buffer.data());
    if (GetIfTable(table, &size, TRUE) != NO_ERROR) {
        return false;
    }

    std::wstring selected = adapter_;
    Lowercase(selected);
    std::vector<DWORD> nextIndices;
    inputBytes = 0;
    outputBytes = 0;

    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const auto& row = table->table[index];
        if (row.dwType == MIB_IF_TYPE_LOOPBACK ||
            row.dwOperStatus != MIB_IF_OPER_STATUS_OPERATIONAL) {
            continue;
        }

        std::wstring name(row.wszName);
        Lowercase(name);
        if (selected != L"auto" && name != selected) continue;
        if (selected == L"auto" && IsVirtualAdapter(name)) continue;

        nextIndices.push_back(row.dwIndex);
        inputBytes += row.dwInOctets;
        outputBytes += row.dwOutOctets;
    }

    if (nextIndices.empty()) {
        cachedInterfaceIndices_.clear();
        lastInterfaceScanTick_ = GetTickCount64();
        initialized_ = false;
        return false;
    }

    if (nextIndices != cachedInterfaceIndices_) initialized_ = false;
    cachedInterfaceIndices_ = std::move(nextIndices);
    lastInterfaceScanTick_ = GetTickCount64();
    return true;
}

bool NetworkMonitor::ReadCachedInterfaces(std::uint64_t& inputBytes,
                                          std::uint64_t& outputBytes) const {
    if (cachedInterfaceIndices_.empty()) return false;
    inputBytes = 0;
    outputBytes = 0;
    for (const DWORD interfaceIndex : cachedInterfaceIndices_) {
        MIB_IFROW row{};
        row.dwIndex = interfaceIndex;
        if (GetIfEntry(&row) != NO_ERROR ||
            row.dwType == MIB_IF_TYPE_LOOPBACK ||
            row.dwOperStatus != MIB_IF_OPER_STATUS_OPERATIONAL) {
            return false;
        }
        inputBytes += row.dwInOctets;
        outputBytes += row.dwOutOctets;
    }
    return true;
}

void NetworkMonitor::Update(SensorSnapshot& snapshot) {
    const auto now = GetTickCount64();
    std::uint64_t inputBytes = 0;
    std::uint64_t outputBytes = 0;

    const bool periodicRescan = !lastInterfaceScanTick_ ||
        now - lastInterfaceScanTick_ >= 30000;
    bool found = false;
    if (periodicRescan) {
        found = RefreshInterfaceCache(inputBytes, outputBytes);
    } else {
        found = ReadCachedInterfaces(inputBytes, outputBytes);
        if (!found) {
            cachedInterfaceIndices_.clear();
            lastInterfaceScanTick_ = 0;
            initialized_ = false;
            found = RefreshInterfaceCache(inputBytes, outputBytes);
        }
    }

    if (!found) {
        initialized_ = false;
        snapshot.networkValid = false;
        return;
    }
    if (!initialized_ || inputBytes < in_ || outputBytes < out_ || now <= tick_) {
        in_ = inputBytes;
        out_ = outputBytes;
        tick_ = now;
        initialized_ = true;
        snapshot.networkValid = false;
        return;
    }

    const double seconds = (now - tick_) / 1000.0;
    if (seconds <= 0.0) {
        snapshot.networkValid = false;
        return;
    }

    snapshot.downloadBytesPerSecond = static_cast<std::uint64_t>((inputBytes - in_) / seconds);
    snapshot.uploadBytesPerSecond = static_cast<std::uint64_t>((outputBytes - out_) / seconds);
    in_ = inputBytes;
    out_ = outputBytes;
    tick_ = now;
    snapshot.networkValid = true;
}

} // namespace monitor
