#include "AmdTemperature.h"

#include <intrin.h>

namespace monitor {

AmdCpuIdentity DetectAmdCpuIdentity() {
    int registers[4]{};
    __cpuid(registers, 1);
    const unsigned int version = static_cast<unsigned int>(registers[0]);
    unsigned int family = (version >> 8) & 0xFu;
    unsigned int model = (version >> 4) & 0xFu;
    const unsigned int extendedFamily = (version >> 20) & 0xFFu;
    const unsigned int extendedModel = (version >> 16) & 0xFu;
    if (family == 0xFu) family += extendedFamily;
    if (family == 0x0Fu || family == 0x17u || family == 0x19u) model |= extendedModel << 4;
    return {family, model, version & 0xFu};
}

} // namespace monitor
