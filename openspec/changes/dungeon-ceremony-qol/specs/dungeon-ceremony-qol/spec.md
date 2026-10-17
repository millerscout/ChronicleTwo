# Spec Delta

## Purpose

Lets players optionally shorten dungeon ceremony events—chest openings, floor clears, and related scripted beats—without altering combat timing or mandatory story outside those ceremonies.

## ADDED Requirements

### Requirement: Ceremony QoL mode defaults off

The game SHALL expose a ceremony QoL setting with at least three modes: **Off**, **Manual**, and **Auto**. Default SHALL be **Off** (retail pacing). The value SHALL persist across sessions with other game options.

#### Scenario: Fresh install

- **WHEN** no saved configuration exists
- **THEN** dungeon ceremony QoL mode is Off

#### Scenario: Player selects Auto and restarts

- **WHEN** the player sets ceremony QoL to Auto, saves, and relaunches
- **THEN** the mode remains Auto

### Requirement: Setting visible in game options

The ceremony QoL mode SHALL appear in the in-game options menu and the PC port overlay Game page, using the same persisted key as other `game.*` settings.

#### Scenario: Retail options menu

- **WHEN** the player opens in-game options
- **THEN** they can set ceremony QoL to Off, Manual, or Auto

#### Scenario: PC overlay

- **WHEN** the player opens the PC Game options tab
- **THEN** they can set the same ceremony QoL modes

### Requirement: Manual message reveal skip on advance input

When mode is **Manual** and a qualifying dungeon ceremony event is active, pressing the confirm/advance control used for event messages SHALL immediately complete reveal of the active event message text for the current page.

#### Scenario: Typewriter in progress

- **WHEN** mode is Manual, a ceremony event is running, and an event message is revealing text
- **THEN** pressing advance once completes that page’s reveal without waiting for draw speed

#### Scenario: Mode off

- **WHEN** mode is Off under the same conditions
- **THEN** advance input behaves as retail for this feature

#### Scenario: After reveal complete

- **WHEN** the current page is fully revealed
- **THEN** advance input follows retail behavior for page advance or script continuation

### Requirement: Auto message reveal skip

When mode is **Auto** and a qualifying dungeon ceremony event is active, the engine SHALL automatically complete reveal of event message text each frame while text is still revealing, without player input.

#### Scenario: Typewriter in progress

- **WHEN** mode is Auto and an event message is mid-reveal during a ceremony event
- **THEN** reveal completes within that frame without pressing advance

#### Scenario: Mode manual

- **WHEN** mode is Manual and text is mid-reveal
- **THEN** reveal does not complete until the player presses advance (unless retail behavior already instant-reveals)

#### Scenario: Choices after text

- **WHEN** mode is Auto and a message requires a menu choice or explicit confirm after text is shown
- **THEN** behavior matches retail for that choice (auto mode does not auto-select options)

### Requirement: Ceremony scope gating

Manual skip, auto skip, and ceremony time acceleration SHALL apply only during designated dungeon ceremony events. They SHALL NOT apply during field combat, inventory menus, town/edit loops, or unrelated events.

#### Scenario: Dungeon inventory open

- **WHEN** the dungeon menu is open
- **THEN** ceremony QoL does not alter messages or timing

#### Scenario: Town conversation

- **WHEN** a non-dungeon event is running
- **THEN** ceremony QoL has no effect

### Requirement: Item and flag safety

Ceremony QoL SHALL NOT skip past script operations that grant items, set floor flags, or record medals. Full script skip that bypasses native commands is out of scope.

#### Scenario: Chest grants item

- **WHEN** a treasure ceremony completes with Manual or Auto mode
- **THEN** items and floor state match retail outcomes

### Requirement: Ceremony time acceleration

When mode is **Manual** or **Auto**, scripted frame waits and object-motion playback for qualifying ceremonies SHALL progress up to **2×** retail speed. When mode is **Off**, timing SHALL match retail.

#### Scenario: Enabled ceremony wait

- **WHEN** mode is Auto and a ceremony script waits on frames
- **THEN** wait elapses up to twice as fast as retail

#### Scenario: Disabled

- **WHEN** mode is Off
- **THEN** wait and motion timing match retail
