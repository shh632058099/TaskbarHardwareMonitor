#include "SensorRegistry.h"

#include <utility>

namespace monitor {

void SensorRegistry::Clear() {
    values_.clear();
}

void SensorRegistry::Upsert(SensorValue value) {
    if (!value.identifier.empty()) values_[value.identifier] = std::move(value);
}

const SensorValue* SensorRegistry::Find(const std::wstring& identifier) const {
    const auto found = values_.find(identifier);
    return found == values_.end() ? nullptr : &found->second;
}

SensorCollection SensorRegistry::Values() const {
    SensorCollection values;
    values.reserve(values_.size());
    for (const auto& entry : values_) values.push_back(entry.second);
    return values;
}

} // namespace monitor
