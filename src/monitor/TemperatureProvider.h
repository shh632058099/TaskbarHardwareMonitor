#pragma once

namespace monitor {

class ITemperatureProvider {
public:
    virtual ~ITemperatureProvider() = default;
    virtual bool ReadPackageTemperature(double& celsius) = 0;
};

class TemperatureManager final : public ITemperatureProvider {
public:
    TemperatureManager(ITemperatureProvider& primary, ITemperatureProvider& fallback)
        : primary_(primary), fallback_(fallback) {}

    bool ReadPackageTemperature(double& celsius) override;

private:
    ITemperatureProvider& primary_;
    ITemperatureProvider& fallback_;
};

} // namespace monitor
