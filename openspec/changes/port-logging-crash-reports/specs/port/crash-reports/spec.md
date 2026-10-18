# Spec Delta

## Purpose

Makes every fatal crash of the PC port leave a self-contained, symbolized report that explains where and in what game state it died, without needing the original executable afterwards.

## ADDED Requirements

### Requirement: Fatal signals produce a crash report
On Linux and macOS, SIGSEGV, SIGBUS, SIGILL, SIGFPE, and SIGABRT SHALL each produce a crash report file under `crashes/` in the save root, named `crash-<YYYYMMDD-HHMMSS>-<pid>.txt`. The handler SHALL run on an alternate signal stack, so a stack overflow is reported too.

#### Scenario: Null dereference
- **WHEN** the game dereferences an invalid pointer and receives SIGSEGV
- **THEN** a crash report is written and its path is printed to stderr

#### Scenario: Stack overflow
- **WHEN** unbounded recursion exhausts the main thread stack
- **THEN** a crash report is still written

### Requirement: Crash report contents
A crash report SHALL contain all of the following:

- the signal name and code, and the faulting address
- the thread that crashed
- the general-purpose registers
- the raw backtrace as module plus offset
- the executable's build ID and load base
- the game version and command line
- the uptime
- the current game breadcrumbs
- the most recent log lines

#### Scenario: Required sections present
- **WHEN** any fatal signal is reported
- **THEN** the report contains labelled sections for signal, registers, backtrace, build, breadcrumbs, and recent log

### Requirement: Symbolization against the running image
On Linux, backtrace frames SHALL be resolved to function, file, and line by reading the executable the crashed process actually has mapped. This SHALL work even if the file on disk was replaced or deleted while the game ran. If no symbolizer tool is available, the report SHALL keep raw frames and say that symbolization was skipped.

#### Scenario: Binary rebuilt during the session
- **WHEN** the game binary on disk is relinked while the game is running and the game then crashes
- **THEN** the report's frames name the functions of the binary that was running, and its build ID matches the crashed process

#### Scenario: No symbolizer installed
- **WHEN** neither `llvm-symbolizer` nor `addr2line` is on `PATH`
- **THEN** the report contains raw frames with offsets and a line stating that symbolization was skipped

### Requirement: Default crash behavior preserved
After writing the report, the handler SHALL restore the default action and re-raise the signal. Exit status, core dumps (for example systemd-coredump), and attached debuggers SHALL behave as they do without the handler.

#### Scenario: Core dump still produced
- **WHEN** the game crashes on a system with core dumps enabled
- **THEN** a core dump is recorded for the same signal, as well as the crash report

### Requirement: Uncaught exceptions reported
An uncaught C++ exception or a call to `std::terminate` SHALL produce a crash report. When the exception type and `what()` message are available, the report SHALL include them.

#### Scenario: Uncaught std::runtime_error
- **WHEN** a `std::runtime_error("bad scene")` escapes the main loop
- **THEN** the crash report records the exception type and `bad scene`

### Requirement: Game state breadcrumbs
The game SHALL keep a small set of named breadcrumbs that describe its current state: the active loop or mode, the scene or map ID, the open menu, the dungeon floor, and the last asset loaded. Reading the breadcrumbs from the crash handler SHALL be async-signal-safe. Updating a breadcrumb SHALL NOT allocate memory.

#### Scenario: Crash during edit-mode drawing
- **WHEN** the game crashes while the edit loop is drawing map 12
- **THEN** the report's breadcrumbs show the edit loop and map 12

### Requirement: Opt-out for debugging
`--no-crash-handler` SHALL disable installation of the crash handler. The session log SHALL still be written.

#### Scenario: Running under a debugger
- **WHEN** the game starts with `--no-crash-handler`
- **THEN** fatal signals reach the default action directly and no crash report is written

### Requirement: Crash report retention
At startup the game SHALL delete the oldest crash reports so at most 20 remain. The session log of a crashed run SHALL be kept until session-log rotation removes it.

#### Scenario: Twenty-first report
- **WHEN** 20 crash reports exist and the game starts
- **THEN** the oldest report is deleted

### Requirement: Windows crash capture
On Windows, an unhandled structured exception SHALL write the same text report, with raw frames, plus a minidump file next to it.

#### Scenario: Access violation on Windows
- **WHEN** the game raises an access violation on Windows
- **THEN** `crash-<timestamp>-<pid>.txt` and `crash-<timestamp>-<pid>.dmp` are written under `crashes/`
