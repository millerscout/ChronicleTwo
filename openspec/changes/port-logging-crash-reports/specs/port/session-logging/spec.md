# Spec Delta

## Purpose

Gives every run of the PC port a persistent, leveled log on disk, so a session's behavior can be reconstructed after the fact, including after a crash.

## ADDED Requirements

### Requirement: Per-session log file
Each game run SHALL create a new log file under `logs/` in the save root. The file name SHALL include the start time and process ID. The log SHALL be opened before configuration loading, so configuration and data errors are recorded.

#### Scenario: Log created at startup
- **WHEN** the game starts normally
- **THEN** a file `logs/session-<YYYYMMDD-HHMMSS>-<pid>.log` exists under the save root and its first lines record the build ID, version, command line, and platform

#### Scenario: Save root not writable
- **WHEN** the log file cannot be created
- **THEN** the game continues to start, logs only to stderr, and prints one warning that names the path it could not open

### Requirement: Stderr captured into the log
Everything the process writes to stderr, including output from Kazarov, Vulkan validation, and SDL, SHALL also be written to the session log. It SHALL still appear on the original stderr.

#### Scenario: Third-party stderr output
- **WHEN** a library writes `Vulkan validation: <message>` to stderr
- **THEN** the same line appears in the terminal and in the session log

### Requirement: Leveled log lines
The logging API SHALL support the levels error, warn, info, debug, and trace, plus a category tag. Each line written to the file SHALL carry a monotonic timestamp, the level, the category, and the thread name or ID.

#### Scenario: Line format
- **WHEN** code logs `info` in category `scene` with text `loading map 3`
- **THEN** the file gains a line containing the elapsed time, `INFO`, `scene`, the thread, and `loading map 3`

### Requirement: Configurable verbosity
The minimum level SHALL default to `info`. It SHALL be settable with `--log-level <level>` or the `<prefix>_LOG_LEVEL` environment variable, and the command-line option SHALL take precedence. Lines below the minimum level SHALL be discarded before formatting.

#### Scenario: Debug enabled from the command line
- **WHEN** the game starts with `--log-level debug`
- **THEN** debug lines are written and trace lines are not

#### Scenario: Unknown level
- **WHEN** `--log-level loud` is given
- **THEN** the game exits with the usage exit status and lists the valid levels

### Requirement: Log rotation
At startup the game SHALL delete the oldest session logs so at most 10 remain, counting the new one. Rotation SHALL NOT delete files it did not create, and it SHALL NOT delete crash reports.

#### Scenario: Eleventh session
- **WHEN** 10 session logs already exist and the game starts
- **THEN** the oldest one is deleted and 10 remain

### Requirement: Recent lines retained in memory
The most recent log lines (at least the last 200) SHALL be kept in memory, so a crash report can include them even if they have not yet reached the file.

#### Scenario: Buffered lines at crash time
- **WHEN** the process crashes immediately after logging a line
- **THEN** that line appears in the crash report's recent log section

### Requirement: Log flushed on exit and on errors
The log SHALL be flushed when the game exits normally, when `std::exit` is called, and after every error-level line.

#### Scenario: Exit with a usage error
- **WHEN** the game exits with the no-data status after logging an error
- **THEN** that error line is present in the log file on disk
