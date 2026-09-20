#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

enum class SensorType { Temperature, Voltage, Fan, Clock, Load, Power, Energy, Data };

struct SensorValue {
    std::wstring identifier;
    std::wstring name;
    SensorType type = SensorType::Data;
    double value = 0.0;
    bool valid = false;
    std::uint64_t timestamp = 0;
};

using SensorCollection = std::vector<SensorValue>;

} // namespace monitor
