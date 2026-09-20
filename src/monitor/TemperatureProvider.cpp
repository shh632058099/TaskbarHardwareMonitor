#include "TemperatureProvider.h"

namespace monitor {

bool TemperatureManager::ReadPackageTemperature(double& celsius) {
    return primary_.ReadPackageTemperature(celsius) ||
           fallback_.ReadPackageTemperature(celsius);
}

} // namespace monitor
