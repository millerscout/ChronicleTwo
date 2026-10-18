# port/abs-bank-weapon-panel-spend Specification

## Purpose
Let players add and remove banked ABS on the main weapon inspect panel using shoulder buttons, without weapon level-down on refund.

## Requirements

### Requirement: Spend on main weapon inspect panel

When `game.abs_bank` is On, the item menu is showing a weapon on **`ITEM_MENU_PANEL_WEAPON`** (main weapon inspect / build-up board view, not the stats sub-panel or evolution extend mode), and `view_weapon` is `USED_ITEM_TYPE_WEAPON`, the port SHALL accept bank transfers for that weapon only.

#### Scenario: Banking off

- **WHEN** `game.abs_bank` is Off and the player is on the main weapon inspect panel
- **THEN** R2 and L2 do not debit or credit the bank (no ABS transfer); on the port they also do not change party inventory pages (see party-page requirement below)

#### Scenario: Wrong panel

- **WHEN** banking is On but `key_arg_no` is not `ITEM_MENU_PANEL_WEAPON` (for example stats sub-panel or bag board)
- **THEN** R2/L2 bank transfer rules in this capability do not apply (except any legacy spend path explicitly removed by design)

### Requirement: R2 adds ABS from bank

While the main weapon inspect panel is active and banking is On, **holding or pressing R2** SHALL move ABS from `banked_abs` to the viewed weapon’s `abs.now`, capped by remaining gauge capacity (`abs.max - abs.now`).

#### Scenario: Partial bank

- **WHEN** the bank holds 50, the weapon has 180/200 ABS, and the player applies 30 via R2
- **THEN** the weapon becomes 200/200, the bank becomes 20, and no ABS is wasted beyond the gauge max

#### Scenario: Level up on add

- **WHEN** R2 additions cause `abs.now` to reach the weapon’s level-up threshold
- **THEN** the weapon levels using the same rules as retail ABS gain (`IsLevelUp` / `LevelUp`), and MAY play the same menu level-up feedback the port uses for other weapon level-ups in the item menu

### Requirement: L2 refunds ABS without level down

While the main weapon inspect panel is active and banking is On, **holding or pressing L2** SHALL move ABS from the viewed weapon’s `abs.now` back to `banked_abs`.

#### Scenario: Refund increases bank

- **WHEN** the weapon has 120 ABS on the gauge and the player refunds 40 via L2
- **THEN** `abs.now` decreases by 40 and `banked_abs` increases by 40

#### Scenario: No level down

- **WHEN** the player refunds ABS with L2
- **THEN** `weapon.level` (and any retail level-derived stats) SHALL NOT decrease; refund stops at the minimum `abs.now` allowed for the current level (retail-consistent floor, not below the current level’s ABS baseline)

#### Scenario: Empty gauge

- **WHEN** `abs.now` is already at the refund floor for the current level
- **THEN** L2 does not decrease ABS further or credit the bank

### Requirement: Player-visible controls

When banking is On and the main weapon inspect panel is showing a weapon, the port SHALL display the current bank total (existing balance UI) and a short hint that **R2 adds** and **L2 removes** banked ABS on this screen.

#### Scenario: Hint visible

- **WHEN** banking is On, the player inspects a weapon on the main panel, and `banked_abs` is 100
- **THEN** the UI shows the bank amount and R2/L2 guidance without opening PC settings

### Requirement: Item menu party pages on L1/R1 (port)

On the port, the item menu SHALL change party inventory pages (Max, Monica, Ridepod, Monster) only when the player presses **L1** (previous page) or **R1** (next page). **L2** and **R2** SHALL NOT change party pages in the item menu.

#### Scenario: L2 on character page does not cycle party

- **WHEN** the player is on a character item-menu page (for example Max) and presses L2
- **THEN** the party page does not change

#### Scenario: R2 on weapon page does not cycle party when banking

- **WHEN** `game.abs_bank` is On, the player is on the main weapon inspect context, and R2 adds ABS per the spend requirements above
- **THEN** the party page does not change because of R2

### Requirement: Evolution build-up mode unchanged

Bank R2/L2 transfers SHALL NOT apply during **`MENU_ITEM_EXTEND_BUILD_UP`** (evolution branch picker) or other extended ask modes; only the main weapon inspect panel defined above.

#### Scenario: View build-up command

- **WHEN** the player opens “View build-up” evolution UI
- **THEN** R2/L2 do not debit or credit the bank for ABS gauge edits
