#pragma once

#include "Sensor.h"

#include <map>

namespace monitor {

class SensorRegistry {
public:
    void Clear();
    void Upsert(SensorValue value);
    const SensorValue* Find(const std::wstring& identifier) const;
    SensorCollection Values() const;
private:
    std::map<std::wstring, SensorValue> values_;
};

} // namespace monitor
