#pragma once

#include "InternalThermoFanProvider.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace monitor {

constexpr std::size_t DellThermoFanDataSize = 24;
constexpr std::uint32_t DellThermoFanWmiQueryTimeoutMilliseconds = 250;

struct DellThermoFanSnapshot {
    double cpuTemperature = 0.0;
    bool cpuTemperatureValid = false;
    double gpuTemperature = 0.0;
    bool gpuTemperatureValid = false;
    std::uint16_t cpuFanRpm = 0;
    bool cpuFanRpmValid = false;
    std::uint16_t gpuFanRpm = 0;
    bool gpuFanRpmValid = false;
};

bool IsDellSmbiosManufacturer(const std::wstring& manufacturer);
bool DecodeDellThermoFanSnapshot(const std::uint8_t* data, std::size_t size,
                                 DellThermoFanSnapshot& snapshot);
void AddDellThermoFanSensors(const DellThermoFanSnapshot& snapshot,
                             std::uint64_t timestamp, SensorCollection& sensors);

class IDellThermoFanDataSource {
public:
    virtual ~IDellThermoFanDataSource() = default;
    virtual bool ReadManufacturer(std::wstring& manufacturer, std::uint32_t timeoutMilliseconds) = 0;
    virtual bool ReadThermoFanData(std::vector<std::uint8_t>& data,
                                   std::uint32_t timeoutMilliseconds) = 0;
};

class DellThermoFanWmiProvider final : public IInternalThermoFanProvider {
public:
    DellThermoFanWmiProvider();
    explicit DellThermoFanWmiProvider(IDellThermoFanDataSource& source);
    bool Read(DellThermoFanSnapshot& snapshot);
    bool Read(SensorSnapshot& snapshot, std::uint64_t timestamp,
              SensorCollection& sensors) override;
    bool Enabled() const { return available_; }
private:
    IDellThermoFanDataSource* source_ = nullptr;
    bool ownsComInitialization_ = true;
    bool manufacturerChecked_ = false;
    bool manufacturerIsDell_ = false;
    bool available_ = false;
    std::uint64_t nextManufacturerRetryTick_ = 0;
    std::uint64_t nextDataRetryTick_ = 0;
    std::uint64_t lastSuccessfulReadTick_ = 0;
    DellThermoFanSnapshot cachedSnapshot_{};
};

} // namespace monitor
