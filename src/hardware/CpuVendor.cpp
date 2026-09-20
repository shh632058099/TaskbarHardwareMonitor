#include "CpuVendor.h"

#include <intrin.h>

#include <cstring>
#include <string>

namespace monitor {

CpuVendor DetectCpuVendor() {
    int registers[4]{};
    __cpuid(registers, 0);
    char vendor[13]{};
    std::memcpy(vendor, &registers[1], sizeof(int));
    std::memcpy(vendor + 4, &registers[3], sizeof(int));
    std::memcpy(vendor + 8, &registers[2], sizeof(int));
    const std::string name(vendor);
    if (name == "GenuineIntel") return CpuVendor::Intel;
    if (name == "AuthenticAMD") return CpuVendor::Amd;
    return CpuVendor::Unknown;
}

} // namespace monitor
