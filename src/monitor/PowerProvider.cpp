#include "PowerProvider.h"

namespace monitor {

bool PowerManager::ReadPackagePower(std::uint64_t nowMilliseconds, double& watts) {
    return primary_.ReadPackagePower(nowMilliseconds, watts);
}

void PowerManager::Reset() {
    primary_.Reset();
}

} // namespace monitor
