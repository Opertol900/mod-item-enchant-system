# Architecture

## Components

- `EnchantConfig`: parses and validates all runtime options.
- `EnchantRates`: converts percentage tables to integer basis points and applies scroll multipliers.
- `EnchantConditions`: classifies item templates and performs all server-authoritative eligibility checks.
- `EnchantItem`: owns localized item naming and the safe server-side `+N Item` representation.
- `EnchantDatabase`: owns the level cache, persistence, history, operation journal, and atomic character transaction.
- `EnchantStatCalculator`: scales template stats and applies static armor, block, and resistance deltas.
- `EnchantManager`: owns selection sessions, throttling, attempt state, result execution, aura refresh, and recovery.
- `EnchantScroll`: binds the configured item templates to the manager using `ItemScript`.
- `EnchantCommands`: exposes player fallback and RBAC-protected administration commands.
- `mod_item_enchant_system.cpp`: registers world, player, and all-item hooks.

`EnchantPackets` is intentionally absent. The stock item-use opcode and gossip packets already provide the required interaction and avoid a client dependency.

## Attempt transaction

1. Resolve scroll and target by live item GUID and validate ownership.
2. Repeat all restrictions server-side and acquire the per-player operation lock.
3. Calculate the target level, chance in basis points, roll, and final failure behavior once.
4. Synchronously insert an immutable `pending` journal row.
5. Apply the predetermined result and its equipment modifiers to memory, then consume one scroll.
6. Save inventory, enchant state, history, and journal completion in one synchronous characters-database transaction.
7. Verify the journal completion, refresh the visual aura, and release the operation lock.

If the process stops after step 4 but before step 6, login recovery replays the already-recorded result. It never rolls a second chance. If the atomic transaction completed, the journal row is already complete and recovery has nothing to do.

## Stat integration

`OnPlayerApplyItemModsBefore` scales ordinary item-template stats. `OnPlayerApplyWeaponDamage` scales exposed weapon damage. Armor, block, and explicit resistances are applied as tracked extra deltas on login/equip and removed on unequip/destruction. The tracker is idempotent and reconciles equipped/broken state once per second, so breaking or repairing an item cannot leave stale static bonuses. A live level change first removes old modifiers, swaps the cached level, and then applies the new modifiers.

## Data ownership

The actual `Item` remains the source of ownership, bag position, binding, and deletion state. The module stores only the item GUID and enhancement metadata. A foreign key removes stale level rows when an `item_instance` is deleted.
