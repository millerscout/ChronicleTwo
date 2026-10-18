# Spec Delta

## Purpose

Shows what the player carries on the hub's Item screen: equipment, active items and the bag of the save in use, with names, icons, descriptions and weapon stats, in the hub's look and controls.

## ADDED Requirements

### Requirement: Item screen reads the save in use
Choosing Item on the hub's main menu SHALL open the Item screen. Its contents SHALL come from the save in use when it opens: the dungeon's copy of the save inside a dungeon, the town save elsewhere. Nothing on the screen SHALL change the save.

#### Scenario: Opening in a dungeon
- **WHEN** the player picks up an item on a dungeon floor and opens Item from the hub
- **THEN** the item is listed

#### Scenario: Browsing changes nothing
- **WHEN** the player moves through every tab and entry and backs out
- **THEN** the save holds the same items, counts, equipment and Gilda as before

### Requirement: Item tabs
The Item screen SHALL have the tabs Equipped, Weapons, Clothing, Ridepod, Attachments, Items, Materials, Fishing and Key items, in that order. The player SHALL change tab with the hub's tab buttons or by pointing at a tab. Each bag tab SHALL list the bag's items of its item types, in bag order. A tab with nothing in it SHALL say so.

#### Scenario: Item types per tab
- **WHEN** the bag holds a sword, a hat, a Ridepod arm, a gem, a Bread, a Georama material, a fish and a dungeon key
- **THEN** they appear under Weapons, Clothing, Ridepod, Attachments, Items, Materials, Fishing and Key items respectively, each once

#### Scenario: Empty tab
- **WHEN** the bag holds no fishing items and the player opens the Fishing tab
- **THEN** the list says the tab is empty and the details panel is blank

### Requirement: Equipped tab
The Equipped tab SHALL list, under a heading for each, Max's and Monica's five equipment slots and three active item slots, and the Ridepod's four parts. Each entry SHALL name its slot, and an empty slot SHALL show as empty.

#### Scenario: Max's weapon
- **WHEN** Max holds a wrench in his melee slot
- **THEN** the Equipped tab lists it under Max with the melee slot's name

### Requirement: Overflow places
Items in the bag's overflow places SHALL be listed in their tabs after the bag's other items, each marked as overflow.

#### Scenario: Full bag
- **WHEN** the bag is full and a picked-up item went to an overflow place
- **THEN** its tab lists it last, marked as overflow

### Requirement: Item rows
Each row SHALL show the item's icon, its name as the game shows it (a renamed weapon shows its new name), and its count when the item stacks. When an item's icon cannot be drawn, the row SHALL show a badge for its tab instead.

#### Scenario: Stacked item
- **WHEN** the bag holds 5 of an item that stacks
- **THEN** its row shows its name and a count of 5

### Requirement: Item details
The details panel SHALL show the focused entry's icon, name, a line saying what it is (its tab and, for equipment, whose), and its description in the game language. A weapon SHALL also show its level, attack, durability out of its maximum, its ABS gauge, its eight attribute values and its synthesis points.

#### Scenario: Weapon details
- **WHEN** the player focuses a weapon
- **THEN** the details panel shows its level, attack, durability, ABS gauge, the eight attribute values and its synthesis points, matching the game's item menu for that weapon

#### Scenario: Description with a value
- **WHEN** the player focuses an item whose description names a number or another item
- **THEN** the description shows that number or name as the game's item menu does

### Requirement: Item screen header
The Item screen SHALL show the player's Gilda and an ABS Bank button that opens the hub's ABS Bank screen. Backing out of ABS Bank SHALL return to the Item screen with the ABS Bank button focused.

#### Scenario: To ABS Bank and back
- **WHEN** the player activates ABS Bank on the Item screen and then backs out
- **THEN** the Item screen is shown with the ABS Bank button focused

### Requirement: Item screen without a save
When no save is loaded, the Item screen SHALL open, say that there is no save to show, and still allow backing out.

#### Scenario: Preview without a save
- **WHEN** the hub opens before any save is loaded and the player opens Item
- **THEN** the screen says there is no save to show, and back returns to the main menu
