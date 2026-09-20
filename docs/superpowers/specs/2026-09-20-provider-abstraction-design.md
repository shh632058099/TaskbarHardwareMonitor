# Provider Abstraction Design

## Goal

Introduce reusable sensor-provider abstractions from `temp/remove-pawnio-kmdf` while preserving the current PawnIO-based Intel path and excluding all KMDF, driver, IOCTL, and driver-installation work.

## Scope

- Add temperature and power provider interfaces.
- Keep PawnIO as the preferred Intel temperature and package-power backend.
- Move WMI/ACPI temperature probing behind a provider interface.
- Add CPU-vendor identity and the AMD temperature provider as a safely disabled-by-default capability: no unverified register map is supplied by production code, so unsupported AMD systems fall back to WMI/ACPI.
- Add internal, backend-neutral sensor values and a registry without changing the existing `SensorSnapshot`, shared-memory protocol, configuration format, taskbar layout, or settings UI.
- Add future-provider interfaces for GPU, storage, and network. Existing monitors retain their current behavior and implement the interfaces.

## Exclusions

- No KMDF project, kernel access implementation, IOCTL protocol, driver packaging, signing, installation, Super I/O, SMBus, EC, or arbitrary hardware register access.
- No removal of PawnIO resources, code, documentation, or licenses.
- No claim of released AMD CPU direct-sensor support and no speculative PCI register probing.

## Architecture

`CpuMonitor` owns `PawnIoTemperatureProvider`, `WmiTemperatureProvider`, and `PawnIoPowerProvider`. `TemperatureManager` calls PawnIO first and WMI/ACPI only when PawnIO fails. `PowerManager` delegates to PawnIO now, creating a stable seam for future backends.

AMD code consists of CPUID identity decoding, pure temperature decoding, and an `AmdTemperatureProvider` that requires an explicit register map. The normal `CpuMonitor` construction supplies no map, so an AMD CPU can only use WMI/ACPI until a separately verified mapping is added.

The generic sensor registry is populated from the existing snapshot after every collection. It is internal-only and does not alter existing UI or IPC consumers.

## Safety and Compatibility

- Windows 8.1 / Windows 10 x64 and C++17 remain the target.
- Existing display values, persistence, diagnostics, and demand-driven collection behavior remain unchanged.
- All new provider selection, AMD gating, decoding, and registry behavior receive native unit tests.
