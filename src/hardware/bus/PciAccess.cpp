#include "PciAccess.h"

namespace monitor {

std::uint32_t MakePciConfigAddress(std::uint8_t bus, std::uint8_t device,
                                   std::uint8_t function, std::uint16_t offset) {
    return (static_cast<std::uint32_t>(bus) << 16) |
           (static_cast<std::uint32_t>(device) << 11) |
           (static_cast<std::uint32_t>(function) << 8) | offset;
}

bool ReadPciConfigValue(IHardwareAccess& access, std::uint8_t bus, std::uint8_t device,
                        std::uint8_t function, std::uint16_t offset, std::uint8_t width,
                        std::uint32_t& value) {
    if (device > 31 || function > 7 || offset > 0xFFC ||
        (width != 1 && width != 2 && width != 4) || offset + width > 0x1000) return false;
    value = 0;
    return access.ReadPciConfig(MakePciConfigAddress(bus, device, function, offset),
                                &value, width);
}

} // namespace monitor
