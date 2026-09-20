# Reliability and Diagnostics Design

## Goal

Improve configuration safety, persist DeskBand menu changes promptly, validate custom display formats in the settings window, and provide a manual diagnostic view with a forced sensor probe.

## Scope

### Configuration safety

`Config::Save` writes the complete JSON payload to a sibling temporary file, flushes and closes it, then replaces `config.json` with `ReplaceFileW` (falling back to `MoveFileExW` when the destination does not yet exist). A failed save preserves the last valid config file.

`Config::Load` keeps default values for missing or invalid fields. It will not treat a malformed field as a valid value. The existing limited parser remains in scope for this change; replacing it with a third-party JSON dependency is explicitly out of scope.

### Deferred persistence for DeskBand commands

When `BandCommandSetDisplay` or `BandCommandSetMetrics` changes configuration, the monitor marks configuration persistence as pending. Its worker waits at most one second after the latest change before calling `Config::Save`. More commands during that second reset the deadline. Explicit settings-window Save remains immediate. Failure to write is retained as pending and retried on the next worker wake.

### Display-format validation

The settings window validates the format on each edit and shows a compact status line beside the existing preview. It reports the first error only: unmatched opening/closing brace, empty conditional body, or unknown variable/modifier. Valid input displays `Format valid`. Validation does not block preview rendering or Save, preserving compatibility with literal text and existing formats.

### Manual diagnostics

Settings gains a `Diagnostics...` child window with a `Re-detect` button and read-only status text. It is manual-only: it performs no timer-based polling and it is not persisted.

Clicking Re-detect posts a request to the monitor window. The worker forces one `SensorManager::Update` using every demand flag. It publishes the resulting snapshot as usual and posts the result to the settings UI. The UI maps validity flags to brief statuses:

- CPU temperature / CPU usage / CPU power: `Available` or `Unavailable`
- Memory / CPU clock / network: `Available` or `Unavailable`
- GPU metrics: `Available` or `Unavailable (driver or supported GPU not found)`
- Disk temperature / disk I/O: `Available` or `Unavailable (no supported drive)`
- Battery / system power: `Available` or `Unavailable (no battery or unsupported state)`

The diagnostics result is discarded if the window has closed. The UI never accesses `SensorManager` directly.

## Interfaces

- `Config::Save()` remains the persistence boundary and gains atomic replacement semantics.
- `ValidateTaskbarFormat(const std::wstring&) -> std::wstring` returns an empty string for valid input or a concise human-readable error.
- `SensorManager::UpdateAll()` performs a single forced update with all defined demand flags.
- `SettingsWindow` receives an owner window for posting diagnostic requests and a result message carrying a copied `SensorSnapshot` through an owned heap allocation released by the receiver.

## Testing

Native unit tests cover format validation failures and valid nested conditions; configuration save/reload using a temporary path; and status mapping from synthetic snapshots. Debug builds and the complete CTest suite are run after implementation.

## Constraints

- Native C++17 / Win32; Windows 8.1 and Windows 10 x64 remain supported.
- No background diagnostic polling and no new network communication.
- Existing taskbar monitoring collection stays demand-driven except for an explicit manual re-detect request.
