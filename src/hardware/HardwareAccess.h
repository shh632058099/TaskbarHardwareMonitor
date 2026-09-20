#pragma once

#include <cstddef>
#include <cstdint>

namespace monitor {

class IHardwareAccess {
public:
    virtual ~IHardwareAccess() = default;
    virtual bool ReadMsr(std::uint32_t cpu, std::uint32_t index,
                         std::uint64_t& value) = 0;
    virtual bool ReadPciConfig(std::uint32_t address, void* buffer,
                               std::size_t size) = 0;
};

class UnavailableHardwareAccess final : public IHardwareAccess {
public:
    bool ReadMsr(std::uint32_t, std::uint32_t, std::uint64_t&) override { return false; }
    bool ReadPciConfig(std::uint32_t, void*, std::size_t) override { return false; }
};

} // namespace monitor
