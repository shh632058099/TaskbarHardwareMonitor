#pragma once

#include <cstdint>

namespace monitor {

class IPowerProvider {
public:
    virtual ~IPowerProvider() = default;
    virtual bool ReadPackagePower(std::uint64_t nowMilliseconds, double& watts) = 0;
    virtual void Reset() = 0;
};

class PowerManager final : public IPowerProvider {
public:
    explicit PowerManager(IPowerProvider& primary) : primary_(primary) {}

    bool ReadPackagePower(std::uint64_t nowMilliseconds, double& watts) override;
    void Reset() override;

private:
    IPowerProvider& primary_;
};

} // namespace monitor
