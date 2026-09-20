#pragma once

namespace monitor {

enum class CpuVendor { Unknown, Intel, Amd };
CpuVendor DetectCpuVendor();

} // namespace monitor
