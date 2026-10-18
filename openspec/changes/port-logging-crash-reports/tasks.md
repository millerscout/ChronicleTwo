# Tasks

## 1. Session logging

- [x] 1.1 Add `port/src/platform/log.{hpp,cpp}` with levels, categories, `CT_LOG_ERROR/WARN/INFO/DEBUG/TRACE`, the fixed 256-slot ring, and a line sink (file + original stderr). Register it in `port/CMakeLists.txt`, and verify `log_test` covers level filtering, line format (elapsed time, level, category, thread), and the ring keeping the last 200+ lines
- [x] 1.2 Create `logs/session-<YYYYMMDD-HHMMSS>-<pid>.log` under `PathsSaveRoot()` with a header (build ID, version, command line, platform), stderr-only fallback, and a single warning when the file cannot be opened. Verify with a test using a temporary save root and a read-only save root
- [x] 1.3 Capture stderr with a pipe and a tee thread (1 MiB pipe, fd 2 restored if the reader exits), and verify a test that writes with raw `fprintf(stderr, …)` finds the line in both the captured original stderr and the session file
- [x] 1.4 Add rotation that keeps 10 `session-*.log` files and never touches other files, plus the 64 MiB per-file cap that drops below `warn` after the cap. Verify with a rotation test that seeds 11 logs and one unrelated file
- [x] 1.5 Add `--log-level` and `<prefix>_LOG_LEVEL` (the CLI wins, an invalid value exits with `kExitUsage` and lists the levels). Flush on exit, `std::exit`, and every error line, and verify with option-parsing tests and a run with `--log-level debug --frames 30 --headless` whose log has debug lines and no trace lines
- [x] 1.6 Initialize logging at the top of `Run()` in `port/src/main.cpp`, before `ConfigLoad()`, and verify a normal `./build.sh linux-x64` run writes a session log whose header and Vulkan device line are present
- [x] 1.7 Move the existing `fprintf(stderr, …)` sites in `port/src` (main, dataread, mg_texture, scenesnd, item_icon3d, visualmotion, platform/config, mglib, present, menuchr) to `CT_LOG_*`, and verify `rg 'fprintf\(stderr' port/src --type cpp -g '!tests/**'` returns only intentional pre-logging usage messages
- [x] 1.8 Document logging (file location, levels, env var, rotation) in `docs/PC.md`, and verify the documented path and flags match a real run

## 2. Breadcrumbs

- [x] 2.1 Add `port/src/platform/breadcrumbs.{hpp,cpp}`: a fixed table with 64-byte values, seqlock writes, a signal-safe `BreadcrumbsWrite(int fd)`, and no allocation on update. Verify `breadcrumbs_test` checks updates, truncation, and a concurrent writer/reader stress case
- [x] 2.2 Set breadcrumbs for the active loop or mode (`mainloop.cpp`, `EditLoop`), scene/map ID (scene load), open menu (menusys entry points), dungeon floor (`dng_main.cpp`), and last asset loaded (`dataread.cpp`). Verify that a headless `--jump` run into a dungeon logs the breadcrumb values at `debug` level

## 3. Crash handler (Linux)

- [x] 3.1 Add `port/src/platform/crash.{hpp,cpp}`: at startup, cache the build ID (`PT_NOTE`), load base (`dl_iterate_phdr`), symbolizer path (`llvm-symbolizer`, then `addr2line`), the `crashes/` directory, and a prebuilt argv buffer. Install `sigaction(SA_SIGINFO|SA_ONSTACK|SA_RESETHAND)` on a 64 KiB `sigaltstack` for SEGV, BUS, ILL, FPE, and ABRT, chaining to earlier handlers. Verify `crash_test` checks the cached build ID against `readelf -n` output
- [x] 3.2 Write the raw report with `write(2)` only: signal, code, fault address, thread, registers from `ucontext`, frames as module+offset, build ID, load base, version, command line, uptime, breadcrumbs, the drained pipe plus ring. Verify with a gtest death test that forks, dereferences `0x000c001d00030004`, and checks every labelled section exists in the report
- [x] 3.3 Symbolize: `fork` + `execve` the cached symbolizer against `/proc/<pid>/exe` with a 10 s timeout and `SIGKILL`. Append the frames, or append "symbolization skipped" when no tool exists. Verify with a death test whose symbolized frames name the crashing test function, and a second one run with an empty `PATH`
- [x] 3.4 Verify the binary-replaced case end to end: start the game, relink or `mv` a different binary over `port/build/pc/darkchronicle`, then trigger a crash through a debug-only `--crash-test segv` option. The report's build ID must equal the running process's, and its frames must name the real crash site
- [x] 3.5 Re-raise with `SIG_DFL` after reporting, and verify `coredumpctl list` shows a core for the `--crash-test segv` run and the shell exit status is 139
- [x] 3.6 Add `std::set_terminate` that records the exception type and `what()`, then aborts. Verify with a death test that throws `std::runtime_error("bad scene")` and finds both in the report
- [x] 3.7 Verify stack overflow is reported: `--crash-test stack` recurses until SIGSEGV, and a report is written
- [x] 3.8 Add `--no-crash-handler`, crash-report rotation to 20, and the stderr line `crash report: <path>`. Verify with tests for rotation and for no report when the handler is disabled
- [x] 3.9 Document the crash report location, contents, `--no-crash-handler`, and the symbolizer dependency in `docs/PC.md`, and verify against a real `--crash-test segv` report

## 4. Crash handler (macOS and Windows)

- [x] 4.1 macOS: use the same signal path with `_dyld` image info, raw frames, and `atos` symbolization against the startup-cached path. Verify the death tests pass with `./build.sh macos-arm64` on a macOS host, or record that they were skipped if no host is available. Skipped: no macOS host; see design.md verification notes.
- [ ] 4.2 Windows: `SetUnhandledExceptionFilter` writes the text report and a `MiniDumpWriteDump` `.dmp` under `crashes/`. Verify that `--crash-test segv` on a `windows-x64` build produces both files

## 5. Memory-safety checks

- [x] 5.1 Add `CT_SANITIZE` handling to `port/CMakeLists.txt`, `linux-x64-asan` configure, build, and test presets in `CMakePresets.json` (`binaryDir` `port/build/pc-asan`), and a `linux-x64-asan` case in `kazarov.conf`. Verify `./build.sh linux-x64-asan` builds and `port/build/pc/darkchronicle` is unchanged (same mtime)
- [x] 5.2 Under ASan, leave SIGSEGV to ASan and register `__sanitizer_set_death_callback` so the breadcrumbs reach the log. Verify a test that reads one past the end of a heap array in the ASan build gets an ASan report followed by breadcrumbs in the session log
- [ ] 5.3 Run `darkchronicle_tests` under the ASan preset and fix or file every finding. Verify `ctest --preset linux-x64-asan` passes, or every remaining finding has a tracking note in this change
- [x] 5.4 Add `CT_CHECK` and `ct::PlausiblePointer` (`port/src/platform/check.hpp`), active in Debug and Release. Verify a death test checks that a failed check logs the expression, location, message, and breadcrumbs, and exits through SIGABRT with a crash report
- [x] 5.5 Guard the pointer-following paths in `mg_texture.cpp` (`MgTextureLookup`, `MgTexturePrepareAnimationDestination`, `Resolve`) and `mg_frame.cpp` (`mgCFrame::Draw` texture and attribute pointers) with `CT_CHECK(ct::PlausiblePointer(…))`. Verify a unit test that feeds `0x000c001d00030004` to texture lookup gets a failed-check report, not SIGSEGV, and existing `mg_*` tests still pass
- [x] 5.6 Try to reproduce the 2026-10-18 crash (edit loop drawing, texture lookup) in the ASan build by replaying the session area with an input script. Record the ASan or `CT_CHECK` report, or "not reproduced", in this change's design notes for a follow-up fix change
- [ ] 5.7 Document the ASan workflow (build, run, reading reports, memory overhead) in `docs/PC.md`, and verify the documented commands run as written

## 6. Integration

- [ ] 6.1 Play a normal session (title → town → dungeon → menu → quit) on `linux-x64`. Verify there are no new frame-time regressions in the existing bench (`port/run/bench`), the session log is complete, no crash report is written, and the exit status is 0
- [ ] 6.2 Run the full `darkchronicle_tests` suite on `linux-x64` and `linux-x64-release`, and verify everything passes, including the new log, breadcrumb, crash, and check tests

## Workflow follow-up

- Run `/opsx-apply` to implement, then archive the change after review.
