# port/game-language-config Specification

## Purpose

Lets players persist their PAL language choice in `config.json`, skip the boot language picker when configured, and change language from PC SETTINGS without corrupting active gameplay buffers.

## Requirements

### Requirement: Persisted language in config

The port SHALL store the player's PAL language in `config.json` as `game.language` when set.

#### Scenario: Round-trip

- **WHEN** `config.json` contains `"game": { "language": "german" }`
- **THEN** `ConfigLoad` sets the configured language to German (`LANG_GERMAN`)

### Requirement: Boot skip when configured

- **WHEN** `game.language` is set and the game starts without `--jump` skipping `SCElogoFade`
- **THEN** the boot language picker is not shown and `LanguageCode` matches the config

### Requirement: PC SETTINGS language change

- **WHEN** the player changes Language on the PC SETTINGS Game page
- **THEN** `config.json` is updated and `LanguageChange` runs using a dedicated scratch buffer rather than the active scene `read_buffer`
