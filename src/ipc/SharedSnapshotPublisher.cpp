#include "SharedSnapshotPublisher.h"

namespace monitor {

SharedSnapshotPublisher::~SharedSnapshotPublisher() {
    if (snapshot_) UnmapViewOfFile(snapshot_);
    if (mapping_) CloseHandle(mapping_);
    if (command_) UnmapViewOfFile(command_);
    if (commandMapping_) CloseHandle(commandMapping_);
    if (snapshotEvent_) CloseHandle(snapshotEvent_);
    if (commandEvent_) CloseHandle(commandEvent_);
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
    snapshotEvent_ = CreateEventW(nullptr, FALSE, FALSE, SharedSensorEventName);
    commandEvent_ = CreateEventW(nullptr, FALSE, FALSE, SharedBandCommandEventName);
    if (!snapshotEvent_ || !commandEvent_) {
        if (snapshotEvent_) CloseHandle(snapshotEvent_);
        if (commandEvent_) CloseHandle(commandEvent_);
        snapshotEvent_ = nullptr;
        commandEvent_ = nullptr;
        UnmapViewOfFile(snapshot_);
        snapshot_ = nullptr;
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
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
        if (newCommandMapping && command_) {
            *command_ = {};
        } else if (command_) {
            // Do not replay the last command after the monitor process restarts.
            const auto sequence = command_->sequence;
            if (!(sequence & 1u)) commandSequence_ = sequence;
        }
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

DWORD SharedSnapshotPublisher::WaitForWake(DWORD timeoutMs) const {
    if (commandEvent_) return WaitForSingleObject(commandEvent_, timeoutMs);
    Sleep(timeoutMs);
    return WAIT_TIMEOUT;
}

void SharedSnapshotPublisher::Wake() const {
    if (commandEvent_) SetEvent(commandEvent_);
}

void SharedSnapshotPublisher::Publish(const SensorSnapshot& source, const Config& config) {
    if (!snapshot_) return;
    const auto sequence = snapshot_->sequence + 1;
    snapshot_->sequence = sequence | 1u;
    auto next = ToSharedSnapshot(source, config);
    next.sequence = sequence + 1;
    next.timestamp = static_cast<std::uint64_t>(GetTickCount64());
    *snapshot_ = next;
    if (snapshotEvent_) SetEvent(snapshotEvent_);
}

} // namespace monitor
