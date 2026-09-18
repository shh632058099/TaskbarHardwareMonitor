# Taskbar Hardware Monitor

一个面向 Windows 8.1 / Windows 10 x64 的轻量级任务栏硬件监控工具。项目使用原生 C++17 / Win32 实现，通过真正的 Explorer DeskBand 集成到任务栏，而不是使用 TopMost、Layered Window 或跟随任务栏位置的覆盖窗口。

## 功能特点

- 原生 Windows DeskBand / Taskbar Toolbar Shell Band。
- 任务栏直接显示 CPU 温度、CPU 占用、内存、网络、GPU、磁盘、电池、功耗等数据。
- Full / Compact 两种显示模式。
- 支持单行 / 双行布局。
- 支持自定义 `Display format`、变量、换行和列对齐。
- 固定宽度与稳定数字布局，避免任务栏内容随数值变化左右抖动。
- 按需采集：没有显示的指标不会后台查询硬件。
- `Collection interval (ms)` 就是真实传感器采集周期。
- 支持任务栏标签色、数值色、字体、字号等配置。
- 支持 Windows 深色 / 浅色任务栏。
- 支持高权限开机启动，并自动维护启动路径。
- Explorer 早于 Monitor 启动时，DeskBand 会先隐藏，第一份有效数据到达后再显示。
- 提供便携 ZIP 打包脚本，解压后即可使用。

## 系统要求

- Windows 8.1 x64 或 Windows 10 x64。
- 当前版本不以 Windows 11 为主要支持目标。
- CPU 温度 / CPU Package 功耗使用 PawnIO 时需要管理员权限。
- NVIDIA GPU 指标需要系统已安装 NVIDIA 驱动及 NVML。

## 快速使用

从便携 ZIP 解压后，双击：

```text
start-monitor.cmd
```

启动脚本会：

1. 注册当前目录中的 `TaskbarBand.dll`。
2. 通知 Explorer 刷新并显示 DeskBand。
3. 启动 `TaskbarHardwareMonitor.exe`。
4. 主程序按 manifest 请求管理员权限，以便访问 PawnIO/MSR 等硬件接口。

建议先把整个目录解压到固定位置，不要直接从 ZIP 内运行。

## 运行文件

```text
TaskbarHardwareMonitor.exe    后台采集、设置窗口、共享数据发布
TaskbarBand.dll               Explorer 加载的任务栏 DeskBand
TaskbarBandActivator.exe      注册变更通知与 DeskBand 激活工具
config.json                   用户配置
start-monitor.cmd             便携版启动入口
```

`PawnIO_setup.exe` 和 `IntelMSR.bin` 已作为资源嵌入 `TaskbarHardwareMonitor.exe`，无需单独复制。

## 指标与采集后端

| 指标 | 主要实现 | 说明 |
| --- | --- | --- |
| CPU Temperature | PawnIO + Intel MSR，WMI fallback | 当前 Intel 路径为主要支持目标 |
| CPU Usage | `GetSystemTimes` | 轻量采集 |
| CPU Clock | `CallNtPowerInformation` | 系统 CPU 频率信息 |
| CPU Power | Intel RAPL via PawnIO/MSR | 依赖 CPU 支持 |
| RAM | `GlobalMemoryStatusEx` | 占用率、已用、总量 |
| Network | IP Helper API | 缓存活动接口，定期重新发现 |
| GPU Temperature / Usage / VRAM / Power / Fan | NVIDIA NVML | 当前主要支持 NVIDIA |
| Disk Temperature | NVMe SMART / ATA SMART | 某些 USB/RAID 桥接设备可能不可读取 |
| Disk I/O | Windows disk performance counters | 与温度采集相互独立 |
| Battery | `GetSystemPowerStatus` | 电量和状态 |
| System Power | Battery discharge state | 仅电池放电场景有意义 |

读取失败或当前硬件不支持的指标显示为 `--`。

## Collection interval 与按需采集

Settings 中的：

```text
Collection interval (ms)
```

就是实际采集周期，而不是单纯的 UI 刷新间隔。

例如设置为 `1000`：

- 已启用的 CPU 温度每 1000 ms 尝试读取一次。
- 已启用的网络、RAM、GPU、磁盘等同样按 1000 ms 采集。
- 未启用的指标不会执行对应硬件查询。

允许范围：`250 ~ 10000 ms`。

如果 `Taskbar enabled` 关闭，则传感器 demand 为 0，硬件采集暂停。

## Metrics 与 Display format

当 `Display format` 为空时，Metrics 勾选项决定显示内容和采集内容。

当 `Display format` 非空时，Format 成为最终定义：

- Metrics 会自动与 Format 中实际引用的变量同步。
- Metrics 控件进入只读状态。
- Format 没有引用的指标不会采集。
- 纯文字 Format 不会触发任何传感器采集。

常用变量：

```text
{cpu_temp}        CPU 温度
{cpu_usage}       CPU 占用
{cpu_clock}       CPU 频率
{power}           CPU Package 功耗
{gpu_temp}        GPU 温度
{gpu_usage}       GPU 占用
{vram}            显存
{vram_used}       已用显存
{vram_total}      总显存
{gpu_power}       GPU 功耗
{fan}             GPU 风扇
{disk_temp}       磁盘温度
{ssd_temp}        磁盘温度兼容别名
{disk_read}       磁盘读取速度
{disk_write}      磁盘写入速度
{down}            下载速度
{up}              上传速度
{ram_usage}       内存占用率
{ram_used}        已用内存
{ram_total}       总内存
{battery}         电池百分比 + 状态
{battery_percent} 电池百分比
{battery_status}  电池状态
{system_power}    电池放电时整机功耗
```

格式控制：

```text
Enter   直接换到任务栏第二行
\n      与 Enter 等价
\t      进入下一列；两行相同列自动对齐
```

示例：

```text
温度:{cpu_temp}\t占用:{cpu_usage}\t内存:{ram_usage}
上行:{up}\t下行:{down}\t电池:{battery}
```

## 开机启动

`Start with Windows` 不使用传统 `HKCU\\...\\Run`，而是创建 Windows Task Scheduler 登录任务：

```text
Trigger   : User logon
LogonType : Interactive token
RunLevel  : Highest
```

这样登录后 Monitor 可以直接获得 PawnIO/MSR 所需要的权限，不会因为普通 Run 启动项权限不足而只出现空数据。

如果程序目录被移动：

- 从新目录启动程序后，若 `start_with_windows=true`，程序会尝试自动更新计划任务路径。
- 在 Settings 点一次 Save 也会重新创建 / 更新任务。

## Explorer 启动顺序

Windows 登录时 Explorer 通常比硬件监控进程更早启动。

当前 DeskBand 的处理方式是：

```text
Explorer 加载 TaskbarBand.dll
        ↓
DeskBand 创建但保持隐藏
        ↓
Monitor 启动并完成首次采集
        ↓
发布有效 shared snapshot
        ↓
DeskBand 显示并恢复真实宽度
```

如果 Monitor 已经运行而只是 Explorer 重启，DeskBand 会立即读取现有 snapshot 并恢复显示。

## 从源码构建

需要：

- Visual Studio / Build Tools，包含 MSVC C++ 工具链。
- Windows SDK。
- CMake 3.20+。
- x64 构建环境。

示例：

```powershell
cmake -S . -B build -A x64 -DBUILD_UNIT_TESTS=ON
cmake --build build --config Release
.\build\Release\TaskbarHardwareMonitorTests.exe
```

主要目标：

```text
TaskbarHardwareMonitor
TaskbarBand
TaskbarBandActivator
TaskbarHardwareMonitorTests
```

## 本机开发重载

开发过程中需要重新编译并让 Explorer 加载新 DLL 时，可以使用：

```powershell
.\restart-taskbar-monitor.ps1
```

脚本会请求管理员权限，停止旧 Monitor、重新构建、运行测试、重新注册 DeskBand、重启 Explorer 并启动新 Monitor。

## 生成便携 ZIP

运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\package-release.ps1
```

脚本会：

1. 创建独立 `build-package` Release 构建。
2. 运行 native tests。
3. 只收集最终运行所需文件。
4. 将打包配置中的 `start_with_windows` 强制设为 `false`。
5. 带上 README、项目许可证和第三方许可证声明。
6. 在项目根目录生成：

```text
TaskbarHardwareMonitor-portable.zip
```

ZIP 会自动校验关键文件并输出 SHA256。

## 项目结构

```text
src/app/          主程序生命周期与 Worker
src/band/         Explorer DeskBand COM 实现
src/config/       JSON 配置与高权限开机任务
src/ipc/          Monitor / DeskBand shared memory IPC
src/monitor/      CPU/GPU/网络/磁盘/电池等传感器
src/ui/           Settings 与任务栏布局
src/activator/    DeskBand 激活工具
resources/        manifest、PawnIO 二进制资源、便携启动脚本
tests/            原生单元测试
```

## 已知限制

- CPU MSR 温度 / RAPL 当前以 Intel CPU 为主要支持路径。
- GPU 高级指标当前以 NVIDIA NVML 为主要支持路径。
- AMD / Intel GPU 暂未实现完整硬件指标采集。
- 某些磁盘控制器、USB 转接器或 RAID 驱动不会暴露标准 SMART 温度。
- Windows 11 不是当前主要适配目标。

## License

本项目源码采用 **GNU General Public License v2.0 or later (`GPL-2.0-or-later`)**。

选择 GPL-2.0-or-later 的主要原因是当前发行包会嵌入并分发 PawnIO。PawnIO 本身采用 GPL-2.0-or-later 并带有针对特定独立 IOCTL 客户端的例外；`IntelMSR.bin` 对应 PawnIO.Modules 的 LGPL-2.1-or-later 模块。为了让当前“源码 + 内嵌第三方二进制”的发布模型保持简单且许可兼容，本项目采用 GPL-2.0-or-later。

完整项目许可证见 [LICENSE](LICENSE)，第三方组件与许可证见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。

> 许可证说明仅用于记录本项目当前的开源发布方式，不构成法律意见。重新分发或替换第三方二进制时，应重新核对相应上游许可证要求。
