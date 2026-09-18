#include "SharedSnapshotPublisher.h"

#include <chrono>

namespace monitor {

SharedSnapshotPublisher::~SharedSnapshotPublisher() {
    if (snapshot_) UnmapViewOfFile(snapshot_);
    if (mapping_) CloseHandle(mapping_);
    if (command_) UnmapViewOfFile(command_);
    if (commandMapping_) CloseHandle(commandMapping_);
}

bool SharedSnapshotPublisher::Open() {
    if (mapping_ && snapshot_) {
        return true;
    }
    SetLastError(ERROR_SUCCESS);
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(SharedSensorSnapshot), SharedSensorMappingName);
    if (!mapping_) return false;
    const bool newSnapshotMapping = GetLastError() != ERROR_ALREADY_EXISTS;
    snapshot_ = static_cast<SharedSensorSnapshot*>(MapViewOfFile(
        mapping_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedSensorSnapshot)));
    if (!snapshot_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
    }
    if (newSnapshotMapping) {
        *snapshot_ = {};
        snapshot_->version = 2;
    }
    commandMapping_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(SharedBandCommand), SharedBandCommandMappingName);
    const bool newCommandMapping = commandMapping_ && GetLastError() != ERROR_ALREADY_EXISTS;
    if (commandMapping_) {
        command_ = static_cast<SharedBandCommand*>(MapViewOfFile(
            commandMapping_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedBandCommand)));
        if (!command_) {
            CloseHandle(commandMapping_);
            commandMapping_ = nullptr;
        }
        if (newCommandMapping && command_) *command_ = {};
    }
    return true;
}

bool SharedSnapshotPublisher::ReadCommand(SharedBandCommand& result) {
    if (!command_) return false;
    for (int attempt = 0; attempt != 3; ++attempt) {
        const auto before = command_->sequence;
        if (before == commandSequence_ || (before & 1u)) continue;
        const auto candidate = *command_;
        const auto after = command_->sequence;
        if (before != after || (after & 1u) || candidate.version != 1 ||
            candidate.sequence != after) {
            continue;
        }
        result = candidate;
        commandSequence_ = after;
        return true;
    }
    return false;
}

void SharedSnapshotPublisher::Publish(const SensorSnapshot& source, const Config& config) {
    if (!snapshot_) return;
    const auto sequence = snapshot_->sequence + 1;
    snapshot_->sequence = sequence | 1u;
    auto next = ToSharedSnapshot(source, config);
    next.sequence = sequence + 1;
    next.timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    *snapshot_ = next;
}

} // namespace monitor
