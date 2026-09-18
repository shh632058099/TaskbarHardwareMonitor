#include "CpuTemperature.h"

namespace monitor {
namespace {

bool IsPlausible(double celsius) {
    return celsius > -20.0 && celsius < 150.0;
}

} // namespace

bool DecodeTemperatureTenthsKelvin(long value, double& celsius) {
    celsius = static_cast<double>(value) / 10.0 - 273.15;
    return IsPlausible(celsius);
}

bool DecodeTemperatureCelsius(long value, double& celsius) {
    celsius = static_cast<double>(value);
    return IsPlausible(celsius);
}

} // namespace monitor
