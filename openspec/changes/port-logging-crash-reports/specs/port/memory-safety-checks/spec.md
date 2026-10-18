# Spec Delta

## Purpose

Catches memory and invariant errors in the PC port where they happen, rather than as later random segfaults, through a sanitizer build and checked invariants that report with game context.

## ADDED Requirements

### Requirement: Sanitizer build preset
A `linux-x64-asan` configure, build, and test preset SHALL build the game and the tests with AddressSanitizer and UndefinedBehaviorSanitizer, frame pointers kept, and debug info. It SHALL use its own build directory, so it never overwrites the normal Debug binary.

#### Scenario: Building the sanitizer configuration
- **WHEN** a developer runs `./build.sh linux-x64-asan`
- **THEN** `port/build/pc-asan/darkchronicle` and its tests are built with ASan and UBSan, and `port/build/pc/darkchronicle` is untouched

#### Scenario: Out-of-bounds read detected
- **WHEN** the sanitizer build reads past the end of a heap buffer
- **THEN** the process stops at the faulting access with an ASan report that names the allocation site

### Requirement: Sanitizer reports reach the session log
When the game runs as a sanitizer build, sanitizer diagnostics SHALL appear in the session log, followed by the game breadcrumbs.

#### Scenario: ASan report in the log
- **WHEN** ASan aborts the sanitizer build
- **THEN** the session log contains the ASan report and the breadcrumbs that were current at that moment

### Requirement: Checked invariants
`CT_CHECK(condition, message…)` SHALL be available to port code. When the condition fails, it SHALL log the expression, file, line, message, and breadcrumbs at error level, and then abort, which produces a crash report. Checks SHALL be active in Debug and Release builds.

#### Scenario: Failed check
- **WHEN** `CT_CHECK(tex != nullptr, "texture %d", id)` fails
- **THEN** the log and the crash report contain `tex != nullptr`, the source location, and `texture <id>`, and the process exits through SIGABRT

### Requirement: Pointer plausibility in texture and frame lookups
Texture lookup and frame drawing code that follows pointers taken from game data SHALL check that each pointer is non-null and a canonical user-space address before dereferencing it. A failed check SHALL be reported through `CT_CHECK`.

#### Scenario: Corrupt texture pointer
- **WHEN** texture lookup receives a pointer value such as `0x000c001d00030004`
- **THEN** a failed-check report naming the texture lookup is produced instead of a general protection fault inside it
