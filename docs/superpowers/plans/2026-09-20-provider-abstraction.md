# Provider Abstraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reuse backend-neutral provider abstractions from `temp/remove-pawnio-kmdf` without importing KMDF or changing current monitor behavior.

**Architecture:** Add small provider interfaces around the existing PawnIO and WMI paths, keep PawnIO first for Intel systems, and carry an AMD provider only behind an explicit map. Add generic sensor-registry data internally after legacy collection completes.

**Tech Stack:** C++17, Win32, native CMake tests.

**Spec:** `docs/superpowers/specs/2026-09-20-provider-abstraction-design.md`

## Global Constraints

- Preserve current Windows 8.1 / Windows 10 x64 support.
- Do not add KMDF, driver, IOCTL, install, signing, Super I/O, SMBus, or EC code.
- Preserve PawnIO as the active Intel backend and do not change existing IPC, UI, or config formats.
- AMD direct reads require an explicit verified map; production code provides none.

---

### Task 1: Temperature and power seams

**Files:**
- Create: `src/monitor/TemperatureProvider.h`, `src/monitor/TemperatureProvider.cpp`
- Create: `src/monitor/WmiTemperatureProvider.h`, `src/monitor/WmiTemperatureProvider.cpp`
- Create: `src/monitor/PowerProvider.h`, `src/monitor/PowerProvider.cpp`
- Modify: `src/monitor/CpuMonitor.h`, `src/monitor/CpuMonitor.cpp`, `CMakeLists.txt`, `tests/TestMain.cpp`

**Produces:** `ITemperatureProvider`, `TemperatureManager`, `IPowerProvider`, and `PowerManager`; CpuMonitor delegates to PawnIO then WMI.

- [ ] Add fake-provider tests showing primary temperature success wins, fallback is used after primary failure, and a power manager delegates and resets.
- [ ] Run the native test executable and confirm the new tests fail because the interfaces do not exist.
- [ ] Implement the provider types and move the existing WMI probe intact into `WmiTemperatureProvider`.
- [ ] Adapt CpuMonitor with thin PawnIO temperature and power providers; retain stale-value semantics.
- [ ] Build Debug and run CTest.

### Task 2: AMD-safe provider capability

**Files:**
- Create: `src/hardware/CpuVendor.h`, `src/hardware/CpuVendor.cpp`
- Create: `src/hardware/HardwareAccess.h`, `src/hardware/HardwareAccess.cpp`
- Create: `src/hardware/amd/AmdTemperature.h`, `src/hardware/amd/AmdTemperature.cpp`, `src/hardware/amd/AmdCpuIdentity.cpp`
- Create: `src/hardware/bus/PciAccess.h`, `src/hardware/bus/PciAccess.cpp`
- Modify: `src/monitor/CpuMonitor.h`, `src/monitor/CpuMonitor.cpp`, `CMakeLists.txt`, `tests/TestMain.cpp`

**Produces:** CPU vendor/AMD identity decoding and an AMD provider that returns unavailable without an explicit map.

- [ ] Add pure tests for supported-family gating, temperature decoding, explicit map success with fake access, and empty-map rejection.
- [ ] Run the native test executable and confirm the tests fail because AMD types do not exist.
- [ ] Add read-only access abstractions and AMD provider code from the source branch, without kernel access or a production map.
- [ ] Route normal AMD CpuMonitor construction to WMI-only fallback; retain injectable AMD provider construction for tests.
- [ ] Build Debug and run CTest.

### Task 3: Generic sensor registry and extension interfaces

**Files:**
- Create: `src/hardware/Sensor.h`, `src/hardware/SensorProvider.h`, `src/hardware/SensorProvider.cpp`, `src/hardware/SensorRegistry.h`, `src/hardware/SensorRegistry.cpp`
- Create: `src/monitor/SnapshotSensorProvider.h`, `src/monitor/SnapshotSensorProvider.cpp`
- Create: `src/monitor/GpuProvider.h`, `src/monitor/StorageProvider.h`, `src/monitor/NetworkProvider.h`
- Modify: `src/monitor/GpuMonitor.h`, `src/monitor/StorageMonitor.h`, `src/monitor/NetworkMonitor.h`, `src/monitor/SensorManager.h`, `src/monitor/SensorManager.cpp`, `CMakeLists.txt`, `tests/TestMain.cpp`

**Produces:** internal sensor collection reflecting the current legacy snapshot and stable provider extension interfaces.

- [ ] Add tests for sensor-identifier merge behavior and conversion of CPU, memory, network, and disk snapshot fields into generic values.
- [ ] Run the native test executable and confirm the new tests fail because generic sensor types do not exist.
- [ ] Add registry and snapshot provider implementation; record the collection timestamp and preserve legacy snapshot behavior.
- [ ] Make current GPU, storage, and network monitors implement their matching interfaces without changing their methods' observable behavior.
- [ ] Build Debug and run CTest.

### Task 4: Integration verification

**Files:**
- Modify: `README.md`

- [ ] Document the provider order and clarify that AMD direct temperature access is intentionally inactive pending verified hardware maps.
- [ ] Configure and build Debug with `BUILD_UNIT_TESTS=ON`.
- [ ] Run CTest and `git diff --check`.
- [ ] Review the final diff against the exclusions, ensuring no driver-related files are introduced and user-owned working-tree changes remain untouched.
