# Proposal

## Why

On 2026-10-18 the PC port crashed with SIGSEGV after a 25-minute session, and the cause could not be fully identified. The game writes no log file, so nothing showed what the player was doing. The only evidence was a systemd core dump, and it could not be symbolized because a rebuild had already replaced the executable on disk (the build IDs differed). Every future crash will be just as opaque unless the game records its own context while it still has its own binary mapped. There is also no sanitizer build, so latent memory bugs like this one (a packed integer field dereferenced as a pointer inside texture lookup) only show up as random segfaults in play sessions.

## What Changes

- Add a session log. Each run writes a timestamped log file under the save root, and old logs are rotated away. The log captures everything the process sends to stderr, including Kazarov, Vulkan validation, and SDL output, as well as messages from a new leveled logging API.
- Add a crash handler for SIGSEGV, SIGBUS, SIGILL, SIGFPE, and SIGABRT, and for uncaught C++ exceptions. When the game crashes, the handler writes a crash report holding the signal, faulting address, registers, raw backtrace, build ID, load base, the last log lines, and the game's current state (loop, scene/map, menu). It then symbolizes the backtrace against the still-mapped executable, so the report is useful even when the binary on disk has been rebuilt. Finally it hands the signal back to the default action, so systemd-coredump and debuggers still behave as before.
- Add a game state "breadcrumb" API so subsystems (main loop, scene load, menus, dungeon) can record what the game is doing at almost no cost.
- Add `CT_CHECK`, a checked-invariant macro that logs file, line, the failing expression, and the current breadcrumbs before aborting. Use it at the pointer-chasing hot spots in texture and frame code (`mg_texture.cpp`, `mg_frame.cpp`).
- Add an `linux-x64-asan` CMake preset (AddressSanitizer + UndefinedBehaviorSanitizer, frame pointers kept) so a reproduced crash can be caught where the memory first goes wrong, not where it later explodes.
- Add CLI options and env vars: `--log-level`, `--no-crash-handler`, `<prefix>_LOG_LEVEL`, and a printed path to the crash report on stderr when a crash occurs.
- Move the existing `fprintf(stderr, …)` call sites in `port/src` onto the logging API (stderr capture keeps them working before this migration).

## Capabilities

### New Capabilities
- `port/session-logging`: per-run log files, levels, stderr capture, rotation, and the in-memory recent-lines buffer.
- `port/crash-reports`: fatal signal and uncaught exception handling, crash report contents and location, in-process symbolization, breadcrumbs, and chaining to the default action.
- `port/memory-safety-checks`: the sanitizer build preset and the `CT_CHECK` invariant macro with its reporting behavior.

### Modified Capabilities
- None. No main specs exist yet.

## Impact

- New code: `port/src/platform/log.{hpp,cpp}`, `port/src/platform/crash.{hpp,cpp}`, `port/src/platform/breadcrumbs.{hpp,cpp}`, and tests under `port/src/tests/`.
- Touched: `port/src/main.cpp` (install logging and the crash handler before `ConfigLoad`), `port/src/mainloop.cpp`, scene, menu, and dungeon entry points (breadcrumbs), `mg_texture.cpp` and `mg_frame.cpp` (`CT_CHECK`), `port/CMakeLists.txt`, `CMakePresets.json`, `port/src/tests/CMakeLists.txt`.
- No change to the Kazarov submodule. Its stderr output is captured instead of edited.
- Runtime dependency on Linux: `llvm-symbolizer` or `addr2line` on `PATH` for symbolized frames. Without one, the report keeps raw addresses and build ID.
- Disk: logs and crash reports under the save root, capped by rotation (defaults: 10 session logs, 20 crash reports).
- Platforms: Linux gets the full feature set. macOS gets signal handling and raw frames. Windows gets an unhandled exception filter and a minidump. Performance is unaffected outside crashes. Logging is buffered, and breadcrumbs are plain stores.
