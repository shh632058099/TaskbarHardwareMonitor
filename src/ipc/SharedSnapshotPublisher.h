#pragma once

#include "SharedSensorSnapshot.h"
#include <windows.h>

namespace monitor {
class SharedSnapshotPublisher {
public:
    ~SharedSnapshotPublisher();
    bool Open();
    void Publish(const SensorSnapshot&, const Config&);
    bool ReadCommand(SharedBandCommand&);
    DWORD WaitForWake(DWORD timeoutMs) const;
    void Wake() const;
private:
    HANDLE mapping_{};
    SharedSensorSnapshot* snapshot_{};
    HANDLE commandMapping_{};
    SharedBandCommand* command_{};
    std::uint32_t commandSequence_{};
    HANDLE snapshotEvent_{};
    HANDLE commandEvent_{};
};
}
