# Design

## Context

- The PC port has no logging layer. About 15 `fprintf(stderr, …)` calls sit in `port/src`, and Kazarov (a git submodule that must not be edited here) prints renderer and Vulkan validation messages directly to stderr. Nothing persists after the terminal closes.
- No signal handler is installed. Crashes rely on systemd-coredump, and a core is only useful with the exact executable that produced it. On 2026-10-18 a relink during the session replaced that executable. The core's build ID (`3be371a8…`) matched no file on disk, and systemd's frame names had been resolved against the newer binary, so they were wrong.
- What the core still showed: a general protection fault in texture code (`mg_texture.cpp`, inside or near `MgTextureLookup`) dereferencing `0x000c001d00030004`, called from `mgCFrame::Draw` → `mgEndDraw` → `EditLoop_EditDraw` → `EditLoop`. `MgTextureLookup` does a hash lookup keyed by `mgCTexture*`, so a corrupted or freed registry entry is a likely cause.
- Builds go through Kazarov configs in `kazarov.conf`, which map onto `CMakePresets.json` presets and build directories. The executable is a Debug PIE built with clang++ and lld, and carries full debug info.
- `main.cpp` `Run()` parses options, then calls `ConfigLoad()`. `PathsSaveRoot()` and `SettingsEnv()` (Kazarov `platform/paths.hpp` and `platform/settings.hpp`) supply the save directory and the env prefix.

## Goals / Non-Goals

**Goals:**
- A crash report that someone reading it later can act on without the core dump or the original binary.
- No measurable frame-time cost while the game runs normally.
- Existing behavior unchanged: exit codes, core dumps, gdb, and `--frames` and headless test runs.

**Non-Goals:**
- Uploading crash reports anywhere, or adding any telemetry.
- Fixing the 2026-10-18 crash itself. This change only adds a sanitizer reproduction task for it. A fix becomes its own change once the cause is known.
- A structured (JSON) log format, log viewers, or in-game log UI.
- Editing Kazarov's logging calls.

## Decisions

### 1. Capture stderr with a pipe and tee thread, not by replacing every call site
At startup, `log` duplicates the original fd 2, creates a pipe, and `dup2`s the write end onto fd 2. A reader thread splits incoming bytes into lines and writes each line to the original stderr, the session file, and the in-memory ring. The logging API writes through the same line sink directly, without going through the pipe, so the order of its own lines is preserved.
- *Why:* This covers Kazarov, SDL, Vulkan loader output, and the existing `fprintf` calls with no submodule changes.
- *Alternatives:* Running the game under a wrapper script (`| tee`) misses headless runs, the ring, and crash integration. Calling `freopen` on stderr to a file loses terminal output. Using `SDL_SetLogOutputFunction` alone misses non-SDL output.
- On a crash, the handler drains whatever is left in the pipe with non-blocking reads before writing the ring.

### 2. A preallocated ring and fixed-size breadcrumbs make the crash path async-signal-safe
The ring is a fixed array of 256 slots, each 256 bytes, with an atomic head index. Breadcrumbs are a fixed table of `{const char* key; char value[64]; std::atomic<uint32_t> seq}`, updated with a seqlock-style write. The crash handler only reads these and writes them with `write(2)`. It does no `malloc`, no stdio, and no locks.
- *Alternative:* Formatting with `std::format` inside the handler can deadlock if the crash happened inside `malloc`.

### 3. Symbolize in a child process against `/proc/<pid>/exe`
The handler first writes the raw report: signal, `ucontext` registers, frames walked with `_Unwind_Backtrace` (frame pointers are already kept in Debug), the `PT_NOTE` build ID read at startup, and the load base from `dl_iterate_phdr` cached at startup. It then calls `fork()` and, in the child, `execve`s `llvm-symbolizer --obj=/proc/<parent-pid>/exe --relative-address` (or `addr2line -f -C -i -e`), with offsets passed on the command line and output appended to the report. The parent `waitpid`s with a 10 s timeout, then re-raises the signal.
- *Why:* `/proc/<pid>/exe` still resolves to the mapped inode after the path has been deleted or relinked, which is exactly the failure from 2026-10-18. The symbolizer runs outside the corrupted process, so it can allocate freely.
- *Alternatives:* In-process DWARF parsing (libbacktrace or cpptrace) allocates and can crash inside a corrupted heap. Copying the 450 MB binary aside at every launch costs too much disk. Crashpad or Breakpad is a heavy dependency, and its minidumps still need symbol files.
- The symbolizer path is found at startup (`PATH` lookup) and stored, so the handler never searches `PATH`.

### 4. Chain to the default action
Install with `sigaction(SA_SIGINFO | SA_ONSTACK | SA_RESETHAND)` on a 64 KiB `sigaltstack`. After writing the report, restore `SIG_DFL` and `raise()`. Earlier handlers (SDL's) are saved and called first for non-fatal handling. `std::set_terminate` records the current exception's type (`__cxa_current_exception_type`) and `what()`, then calls `abort()`, which reaches the same SIGABRT path.

### 5. Crash handling stays on in Debug and Release, with `--no-crash-handler` as the off switch
gdb intercepts signals before the handler runs, so a separate off switch is mainly for rr and for experiments. Headless test runs keep the handler so CI crashes leave reports too.

### 6. `CT_CHECK` is always on, and the pointer test is cheap
`CT_CHECK(cond, fmt, …)` expands to `[[unlikely]]` branch → `ct::CheckFailed(#cond, __FILE__, __LINE__, fmt, …)` → log error, flush, `abort()`. `ct::PlausiblePointer(p)` returns true when `p != nullptr`, `p` is in canonical user space (`< 0x0000'8000'0000'0000`), and `p` is aligned for the type. It does not probe mappings: no `msync` or `mincore`, because those are system calls on hot paths.
- *Alternative:* Silently returning an empty handle on a bad pointer would hide corruption and turn crashes into invisible rendering glitches. The spec requires a loud failure.

### 7. The sanitizer preset is a new Kazarov config
Add `linux-x64-asan` to `KZ_CONFIGS` and `KZ_CONTAINER_CONFIGS`, a `kz_config` case with `KZ_BUILD_DIR=port/build/pc-asan`, and configure, build, and test presets that inherit `linux-x64` and add `CT_SANITIZE=address,undefined`. `port/CMakeLists.txt` applies `-fsanitize=…`, `-fno-omit-frame-pointer`, and `-fno-sanitize-recover=undefined` to `ct_options` and the link options when `CT_SANITIZE` is set. The crash handler detects ASan (`__has_feature(address_sanitizer)`), leaves SIGSEGV to ASan, and registers `__sanitizer_set_death_callback` to append the breadcrumbs to the log.

## Risks / Trade-offs

- [The tee thread dies or blocks, and stderr writers block on a full pipe] → The pipe uses a 1 MiB buffer (`F_SETPIPE_SZ`). If the reader exits, fd 2 is restored to the original stderr.
- [`fork()` in a signal handler with a corrupted heap] → The child only calls `execve` (async-signal-safe) with argv prebuilt on the alt stack. A timeout and `SIGKILL` stop a hung child.
- [ASan memory overhead (2–3×) on an already large Debug build] → The preset is opt-in and has its own build directory. Document that it is meant for reproducing crashes, not daily play.
- [`CT_CHECK` aborts on data the retail game tolerated] → Add checks only where a failed check would otherwise segfault anyway. Do not add them to merely suspicious values.
- [Log disk usage from debug-level spam] → The default level is `info`, and rotation caps the file count. Cap each session file at 64 MiB, after which only `warn` and above are written.
- [macOS has no `/proc`] → Use `_dyld_get_image_header`, `atos -o <path> -l <base>` with the path cached at startup, and raw frames as a fallback. A path that is replaced mid-session is a documented limitation there.

## Migration Plan

- The change is additive. Rollback is a revert. `--no-crash-handler` and `--log-level warn` reduce the behavior at runtime without a rebuild.
- Existing `fprintf(stderr)` sites keep working through stderr capture. Migrating them to `CT_LOG_*` can happen gradually.

## Verification notes

- 2026-10-18 crash (edit-loop draw, texture pointer `0x000c001d00030004`): not reproduced. `linux-x64-asan` with `--jump town:10 --fast-load --input tools/input/area-town-walk.input --frames 450 --headless` reached the town script `map/m/m01/m01_1.stb` and then aborted in `options::BuiltPages` (`port/src/options/rows.cpp`, `pages[i].name == names[i]`), before texture lookup in the edit draw. `CT_CHECK(ct::PlausiblePointer)` rejects that pointer in `MgTextureLookup` without a general protection fault. A follow-up should replay a captured edit-draw input once the options-page assertion is understood.
- UBSan alignment: retail packed `SPI_TAG_HASH` records are misaligned for the host. `ps2/src` keeps `-fno-sanitize=alignment` (a library linked after `ct_options`, so `-fsanitize=undefined` does not turn it back on). `port/src/scriptinterpreter.cpp` has the same exception. Other undefined-behavior checks stay on.
- The Kazarov image's clang 23 has no compiler-rt. `kz_before_build` for `linux-x64-asan` installs `libclang-rt-23-dev` when `libclang_rt.asan.a` is missing.
- macOS (`4.1`) and Windows (`4.2`) crash paths are implemented. No macOS or Windows host was available, so the death tests and `--crash-test segv` minidump check were not run.

## Open Questions

- Whether to also mirror the session log into the Kazarov run output directory for `run.sh` users. This is cosmetic and can be decided during implementation.
