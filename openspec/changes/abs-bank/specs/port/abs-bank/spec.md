# Spec Delta

## Purpose

Optional QoL that banks weapon experience from dungeon orbs into per-save extension data and lets the player apply that ABS to weapons in the item menu.

## ADDED Requirements

### Requirement: Config toggle defaults off

The port SHALL expose `game.abs_bank` in PC settings with choices that include Off and On. When Off, weapon experience pickups SHALL use retail `AddExpWeaponParam` routing. When On, pickups SHALL not call `AddExpWeaponParam` for orb collection.

#### Scenario: Faithful default

- **WHEN** `game.abs_bank` is Off and the player collects a weapon experience orb
- **THEN** equipped weapons receive ABS using retail rules

#### Scenario: Banking enabled

- **WHEN** `game.abs_bank` is On and the player collects an orb with positive `exp`
- **THEN** the banked total increases by that amount and equipped weapons do not receive that orb's ABS

### Requirement: Bank persistence

The port SHALL store the bank float in the active save's extensions under `mods.chronicle_two.abs_bank.banked_abs`. The value SHALL round-trip across load, manual save, and autosave when extensions are written.

#### Scenario: Load existing save

- **WHEN** a save loads with `banked_abs` 120.5 in its sidecar
- **THEN** the in-memory bank equals 120.5 for that session

#### Scenario: Spend reduces bank

- **WHEN** the player applies 40 ABS from the bank to a weapon
- **THEN** `banked_abs` decreases by 40 (not below zero) and the sidecar reflects the new total on the next successful save write

### Requirement: Apply ABS from bank to bag weapons

When `game.abs_bank` is On and the bank is greater than zero, the item menu SHALL let the player apply ABS from the bank to a weapon shown on the weapon page (bag or equipped), up to the weapon's remaining ABS capacity, using partial amounts.

#### Scenario: Partial fill

- **WHEN** the bank holds 100, a weapon has 30 of 200 ABS, and the player applies 50 from the bank to that weapon
- **THEN** the weapon gains 30 ABS, the bank loses 30, and no ABS is wasted beyond the weapon's max

#### Scenario: Level up

- **WHEN** applied ABS crosses the weapon's level threshold
- **THEN** the weapon levels using the same rules as retail ABS gain (including feedback the game already uses for weapon level-up)

### Requirement: Bank visibility in menu

When `game.abs_bank` is On, the item menu SHALL show the current banked total in a player-visible place on the weapon flow (for example the weapon stats panel or an added command label), using retail font and layout conventions.

#### Scenario: Player checks bank before spending

- **WHEN** the player opens the weapon stats view with banking enabled and 250 banked
- **THEN** the UI displays that 250 (or equivalent formatted gauge text) without opening PC settings

### Requirement: Event script orbs

Weapon experience pickups created through the same dungeon pickup type as monster orbs SHALL follow the same banking rules when `game.abs_bank` is On.

#### Scenario: Scripted orb

- **WHEN** a `PULL_ITEM_WEAPON_EXP` pickup is collected with banking On
- **THEN** its `exp` value is added to the bank
