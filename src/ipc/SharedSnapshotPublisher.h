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
private:
    HANDLE mapping_{};
    SharedSensorSnapshot* snapshot_{};
    HANDLE commandMapping_{};
    SharedBandCommand* command_{};
    std::uint32_t commandSequence_{};
};
}
