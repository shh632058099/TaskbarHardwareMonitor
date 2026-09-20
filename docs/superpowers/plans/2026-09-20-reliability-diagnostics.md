# Reliability and Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make configuration writes resilient, persist DeskBand changes promptly, validate custom formats, and add manual sensor diagnostics.

**Architecture:** Keep `Config` as the only JSON persistence boundary and add atomic replacement there. Add pure helpers for format validation and diagnostic text so their behavior is unit-testable. The monitor worker continues to own hardware access; settings requests and receives diagnostics through window messages only.

**Tech Stack:** C++17, Win32, CMake, native unit tests/CTest.

**Spec:** `docs/superpowers/specs/2026-09-20-reliability-diagnostics-design.md`

## Global Constraints

- Native C++17 / Win32; Windows 8.1 and Windows 10 x64 remain supported.
- No background diagnostic polling and no new network communication.
- Existing taskbar monitoring collection stays demand-driven except for an explicit manual re-detect request.

---

### Task 1: Atomic configuration persistence

**Files:**
- Modify: `src/config/Config.cpp`
- Modify: `tests/TestMain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `bool Config::Save() const`, which writes atomically and leaves an existing config intact on a failed replacement.

- [ ] **Step 1: Write the failing test**

Add a temporary-path round-trip test that assigns `refreshIntervalMs = 750`, `taskbarFormat = L"CPU:{cpu_temp}"`, calls `Save`, then loads a new `Config` from that path and asserts both values. Remove the temporary file through `DeleteFileW` in test cleanup.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: FAIL because `Config.cpp` is not linked into `TaskbarHardwareMonitorTests`.

- [ ] **Step 3: Link the real Config implementation and implement atomic save**

Add `src/config/Config.cpp` to the test executable. Build JSON into an `std::ostringstream`, write it to `path + L".tmp"`, close it, then use `ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)`. If the original file does not exist, use `MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`. On any error delete only the temporary file and return false.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: PASS.

### Task 2: Display-format validation helper and settings feedback

**Files:**
- Modify: `src/ui/TaskbarLayout.h`
- Modify: `src/ui/TaskbarLayout.cpp`
- Modify: `src/ui/SettingsWindow.h`
- Modify: `src/ui/SettingsWindow.cpp`
- Modify: `tests/TestMain.cpp`

**Interfaces:**
- Produces: `std::wstring ValidateTaskbarFormat(const std::wstring& format)`; empty return means valid.

- [ ] **Step 1: Write failing validation tests**

Add assertions that `ValidateTaskbarFormat(L"CPU:{cpu_temp}")` is empty, unmatched `L"{cpu_temp"` returns `L"Missing closing }"`, `L"cpu_temp}"` returns `L"Unexpected }"`, `L"{gpu_temp?}"` returns `L"Conditional section is empty"`, and `L"{not_a_metric}"` returns `L"Unknown variable: not_a_metric"`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: FAIL at compilation because `ValidateTaskbarFormat` does not exist.

- [ ] **Step 3: Implement validation and surface its result**

Parse braces with nesting depth, validate conditional expressions using the supported variable list already accepted by `FormatVariable`, and validate modifiers per variable type. Add a `formatStatus_` static control below the preview. `UpdateFormatPreview` sets it to `Format valid` for valid non-empty input, the specific validation error for invalid input, and an empty string for default layout.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: PASS.

### Task 3: Deferred DeskBand-command persistence

**Files:**
- Modify: `src/app/App.h`
- Modify: `src/app/App.cpp`

**Interfaces:**
- Produces: worker-owned `configSavePending_` and a one-second save deadline updated by display and metric commands.

- [ ] **Step 1: Add a failing behavior test seam**

Extract `DWORD MillisecondsUntilConfigSave(ULONGLONG now, ULONGLONG deadline, bool pending)` into `src/app/App.h` as an inline helper. Add tests asserting pending at 500 ms returns 500, pending at/after deadline returns 0, and non-pending returns `INFINITE`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: FAIL at compilation because the helper does not exist.

- [ ] **Step 3: Implement delayed persistence**

Set `configSavePending_ = true` and `configSaveDeadline_ = GetTickCount64() + 1000` after processing `BandCommandSetDisplay` or `BandCommandSetMetrics`. Include the helper result in the worker wait timeout. On an expired deadline copy the protected config and call `Save`; clear pending only on success. Do not delay the settings window's existing explicit Save.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: PASS.

### Task 4: Manual diagnostics flow

**Files:**
- Modify: `src/monitor/SensorDemand.h`
- Modify: `src/monitor/SensorManager.h`
- Modify: `src/monitor/SensorManager.cpp`
- Modify: `src/ui/SettingsWindow.h`
- Modify: `src/ui/SettingsWindow.cpp`
- Modify: `src/app/App.h`
- Modify: `src/app/App.cpp`
- Modify: `tests/TestMain.cpp`

**Interfaces:**
- Produces: `constexpr std::uint32_t AllSensorDemand`, `SensorSnapshot SensorManager::UpdateAll()`, and `std::wstring BuildDiagnosticsText(const SensorSnapshot&)`.

- [ ] **Step 1: Write failing diagnostics tests**

Create a zeroed `SensorSnapshot` and assert diagnostics contains `CPU temperature: Unavailable`; then set `cpuTemperatureValid`, `memoryValid`, and `networkValid` and assert it contains `CPU temperature: Available`, `Memory: Available`, and `Network: Available`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: FAIL at compilation because `BuildDiagnosticsText` does not exist.

- [ ] **Step 3: Implement worker-owned re-detection and diagnostics UI**

Define `AllSensorDemand` as the OR of every `Demand*` bit. Implement `UpdateAll` as `Update(AllSensorDemand, true)`. Add a Diagnostics button to Settings and a child window with read-only text and `Re-detect` button. On click post `WM_APP + 6` to the owner. App handles it by setting a worker diagnostic request; the worker calls `UpdateAll`, publishes the snapshot, and posts a heap-allocated copied snapshot back to the still-valid settings window using a dedicated result message. The result receiver converts it with `BuildDiagnosticsText`, owns and deletes the allocation, and ignores it when the window has closed.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -C Debug --output-on-failure`

Expected: PASS.

### Task 5: Integration verification

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document diagnostics and safe config behavior**

Add a concise Settings section stating that Diagnostics is manually run and lists availability only; add that configuration writes are safely replaced after a complete write.

- [ ] **Step 2: Configure and build Debug**

Run: `$cmake = (Get-Content -Raw .cmake-path).Trim(); & $cmake -S . -B build -DBUILD_UNIT_TESTS=ON; & $cmake --build build --config Debug`

Expected: all targets build with exit code 0.

- [ ] **Step 3: Run the full test suite and diff check**

Run: `$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'; & $ctest --test-dir build -C Debug --output-on-failure; git diff --check`

Expected: all tests pass and `git diff --check` reports no whitespace errors.
