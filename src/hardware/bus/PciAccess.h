#pragma once

#include "../HardwareAccess.h"

namespace monitor {

std::uint32_t MakePciConfigAddress(std::uint8_t bus, std::uint8_t device,
                                   std::uint8_t function, std::uint16_t offset);
bool ReadPciConfigValue(IHardwareAccess& access, std::uint8_t bus, std::uint8_t device,
                        std::uint8_t function, std::uint16_t offset, std::uint8_t width,
                        std::uint32_t& value);

} // namespace monitor
