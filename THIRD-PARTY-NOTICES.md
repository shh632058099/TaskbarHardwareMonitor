# Third-Party Notices

Taskbar Hardware Monitor 使用或可选调用以下第三方组件。项目自身采用 `GPL-2.0-or-later`；第三方组件仍分别受其原始许可证约束。

## PawnIO

- Component: `PawnIO_setup.exe`
- Embedded file version: `2.1.0.0`
- SHA256: `A3A46226C5E2824F4CDD42BE0EECBABFC672C86F7889710F5AB1E6AD385B47A0`
- Upstream: https://github.com/namazso/PawnIO
- License: GNU General Public License v2.0 or later, with the PawnIO special exception described by the upstream project.

PawnIO 官方许可证允许独立模块通过 device IO control interface 与 PawnIO 通信，并对这种组合提供专门例外。当前 Taskbar Hardware Monitor 不链接 `PawnIOLib`，而是通过 Windows device IO control 与 PawnIO 设备通信。

当前发行方式会把 `PawnIO_setup.exe` 作为 Windows resource 嵌入 `TaskbarHardwareMonitor.exe`，在首次需要 PawnIO 且系统尚未安装时释放并执行安装器。

PawnIO 的 GPL 基础许可证文本与本项目根目录的 `LICENSE` 相同版本系列。PawnIO 官方版权及特殊例外声明另存于 `LICENSES/PawnIO-NOTICE.md`，对应源码请以上游仓库为准。重新分发修改版 PawnIO 时，必须遵守 PawnIO 自身的源码提供与许可证义务。

## PawnIO.Modules - IntelMSR

- Component: `IntelMSR.bin`
- SHA256: `D6ED85D65AB17A22F813EF98207D6D537155EE2DED5976A21CB48413C9B92E5F`
- Corresponding upstream source: https://github.com/namazso/PawnIO.Modules/blob/main/IntelMSR.p
- Upstream repository: https://github.com/namazso/PawnIO.Modules
- License: GNU Lesser General Public License v2.1 or later (`LGPL-2.1-or-later`).

`IntelMSR.bin` 被嵌入主程序，在运行时释放到临时文件、读入内存并通过 PawnIO IOCTL 加载。LGPL 2.1 文本随项目保存在 `LICENSES/LGPL-2.1.txt`，并随便携 ZIP 一起分发。

## NVIDIA NVML

GPU 温度、利用率、显存、功耗和风扇指标通过 NVIDIA NVML 运行时接口读取。

本项目：

- 不静态链接 NVML；
- 不在源码仓库或便携 ZIP 中重新分发 `nvml.dll`；
- 仅在系统已安装 NVIDIA 驱动并提供 NVML 时动态加载。

NVML 及 NVIDIA 驱动继续受 NVIDIA 自身许可证约束。

## Microsoft Windows system components

项目调用 Windows SDK / 系统提供的 Win32、COM、Task Scheduler、IP Helper、WMI、Power Management、SMART / storage IOCTL 等接口。这些系统组件不随本项目重新分发，继续受 Microsoft Windows / SDK 的相关许可证约束。

## LibreHardwareMonitor

当前项目**不依赖、不链接、也不重新分发 LibreHardwareMonitor**。之前用于参考的外部源码目录已从工作区删除，当前构建和运行均不需要 LibreHardwareMonitor。
