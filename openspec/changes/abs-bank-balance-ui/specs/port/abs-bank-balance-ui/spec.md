# Spec Delta

## Purpose

When weapon EXP banking is enabled, players can see their banked ABS total in normal play without opening the weapon stats spend panel or external files.

## ADDED Requirements

### Requirement: Dungeon balance readout

When `game.abs_bank` is On and the player is in a dungeon field state with the status board visible, the port SHALL draw the current banked ABS total as readable text using the same font conventions as other dungeon HUD overlays.

#### Scenario: Banking on with balance

- **WHEN** banking is On, the player is on a dungeon floor with the status board shown, and `banked_abs` is 120
- **THEN** the HUD shows a label that includes 120 (or equivalent integer formatting)

#### Scenario: Banking off

- **WHEN** `game.abs_bank` is Off
- **THEN** no bank balance HUD element is drawn in the dungeon

#### Scenario: Zero balance while enabled

- **WHEN** banking is On and `banked_abs` is 0
- **THEN** the HUD MAY show zero or hide the readout; behavior MUST match the design document and stay consistent across sessions

### Requirement: Item menu weapon view readout

When `game.abs_bank` is On and the item menu shows a weapon or fishing rod on the main weapon view (not only the stats sub-panel), the port SHALL display the current banked ABS total in a player-visible location on that view.

#### Scenario: Bag weapon inspection

- **WHEN** banking is On, the player opens the item menu on a bag weapon, and `banked_abs` is 250
- **THEN** the weapon view shows 250 (or equivalent formatting) without navigating to the stats sub-panel

#### Scenario: Banking off in menu

- **WHEN** `game.abs_bank` is Off
- **THEN** the item menu weapon view does not show a bank total

### Requirement: Read-only display

Balance UI added by this capability SHALL NOT debit the bank or change `banked_abs`; spending remains on the weapon stats panel defined by abs-bank.

#### Scenario: View does not spend

- **WHEN** the player only views the dungeon HUD or weapon view readout
- **THEN** `banked_abs` is unchanged
