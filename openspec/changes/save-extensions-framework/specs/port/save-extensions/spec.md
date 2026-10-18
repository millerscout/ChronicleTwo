# Spec Delta

## Purpose

Lets mods and first-party features persist per-save-slot JSON state beside the retail save image without changing `SAVEDATA_FORMAT` size or checksum layout.

## ADDED Requirements

### Requirement: Sidecar file location and naming

The port SHALL store save extensions in a JSON file adjacent to the save image, named `<save-basename>.extensions.json` under the same directory as the flat save file (for example `saves/BESCES-51190dkcl3.extensions.json` and `saves/autosave.extensions.json`).

#### Scenario: Manual slot save path

- **WHEN** the active save image path is `saves/BESCES-51190dkcl0`
- **THEN** the extensions file path is `saves/BESCES-51190dkcl0.extensions.json`

#### Scenario: Autosave path

- **WHEN** the active save image path is `saves/autosave`
- **THEN** the extensions file path is `saves/autosave.extensions.json`

### Requirement: Sidecar document schema

Each extensions file SHALL be a JSON object with integer `schema_version` equal to `1`, string `save_file` matching the save image basename, unsigned integer `unique_counter` copied from the save image header at last write, and object `mods` whose keys are mod namespace ids and values are arbitrary JSON objects.

#### Scenario: Valid document loads

- **WHEN** the extensions file parses as JSON and contains `schema_version` 1 and a `mods` object
- **THEN** the port loads each `mods` entry into the in-memory extension store

#### Scenario: Invalid schema version

- **WHEN** the extensions file has `schema_version` other than `1`
- **THEN** the port treats the file as absent and uses an empty extension store

### Requirement: Missing sidecar defaults

When no extensions file exists or it cannot be read or parsed, the port SHALL use an empty `mods` map and SHALL NOT fail loading the retail save image.

#### Scenario: Legacy save without sidecar

- **WHEN** a valid retail save image loads and no extensions file exists
- **THEN** gameplay proceeds with no mod save data

### Requirement: Counter mismatch warning

When `unique_counter` in the extensions file differs from the save image header `unique_counter` at load time, the port SHALL still load the `mods` data and SHALL log a diagnostic that the sidecar may be stale.

#### Scenario: Copied binary without sidecar update

- **WHEN** the save image `unique_counter` is 100 and the sidecar lists `unique_counter` 99
- **THEN** mod data from the sidecar is available and a stale-sidecar diagnostic is emitted

### Requirement: Load timing

The port SHALL load extensions only after the save image bytes pass existing validation (size, version, checksum rules) and before gameplay reads mod-owned state for that session.

#### Scenario: Corrupt save image

- **WHEN** `SaveSlotRead` returns a state other than ready
- **THEN** extensions are not loaded and the previous session extension binding is cleared

### Requirement: Save timing

When the port successfully writes a save image to disk for the bound session, it SHALL write the extensions file for that same basename with `unique_counter` and `save_file` updated to match the written image.

#### Scenario: Manual save after mod data change

- **WHEN** mod data in memory changed since load and the user completes a successful slot save
- **THEN** the matching `.extensions.json` file reflects the in-memory `mods` map

### Requirement: Mod namespace isolation

The port SHALL expose mod data only by namespace id string keys under `mods`. The engine SHALL NOT interpret or merge unknown namespace subtrees except to persist them verbatim.

#### Scenario: Two mods coexist

- **WHEN** `mods` contains `example.mod_a` and `example.mod_b` with distinct JSON objects
- **THEN** reading or writing one namespace does not alter the other

### Requirement: Retail blob unchanged

Save extensions SHALL NOT add fields to `CSaveData` or change `sizeof(SAVEDATA_FORMAT)`.

#### Scenario: Checksum validation unchanged

- **WHEN** a save image without an extensions file is written and reloaded
- **THEN** existing check digit validation behavior is unchanged from current port behavior

### Requirement: Backup documentation

Port documentation SHALL state that copying or backing up a save with mod state requires both the save image and its `.extensions.json` file when present.

#### Scenario: Documented backup set

- **WHEN** a user reads the PC save documentation for the save root layout
- **THEN** extensions sidecars are listed alongside autosave spawn JSON as optional companion files
