#pragma once

namespace monitor {

bool DecodeTemperatureTenthsKelvin(long value, double& celsius);
bool DecodeTemperatureCelsius(long value, double& celsius);

} // namespace monitor
