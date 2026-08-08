# Developer guide

## Public extension points

The module exposes singleton services through their headers:

```cpp
uint8 level = sEnchantDatabase.GetLevel(item);
ValidationResult result = sEnchantConditions.Validate(player, item, ScrollType::Normal);
uint32 chance = sEnchantRates.GetChanceBasisPoints(level + 1, category, ScrollType::Normal);
```

Use `sEnchantManager.SetLevelByCommand` for a live administrative level change because it safely removes and reapplies equipped modifiers. Do not update `mod_item_enchant` directly while the owner is online.

### Transmog detection

A transmog module can register a read-only instance detector during script startup:

```cpp
sEnchantConditions.SetTransmogDetector([](Item const* item)
{
    return item && MyTransmogStore::Instance().HasAppearance(item->GetGUID());
});
```

When `ItemEnchant.AllowTransmogItems = 0`, a detected item is rejected. Without a registered detector, no third-party schema or runtime state is assumed.

## Adding a scroll type

1. Extend `ScrollType` and its `ToString` conversion.
2. Add a config entry and world item template using `item_mod_enchant_scroll`.
3. Define rate and failure behavior in `EnchantRates` and `EnchantManager`.
4. Add recovery coverage for the new deterministic result.

## Schema evolution

Add a new uniquely named SQL file under the appropriate `data/sql/db-*` directory. Never rewrite a migration that may already be recorded by AzerothCore's module updater. The initial files use `CREATE TABLE IF NOT EXISTS` only to make first installation idempotent; later changes should be explicit `ALTER TABLE` migrations.

## Security invariants

- Never trust a client bag/slot or gossip action without resolving the current item GUID again.
- Never consume a scroll before a pending journal row exists.
- Never roll during recovery.
- Keep inventory persistence and journal completion in the same synchronous transaction.
- Do not expose the GM scroll without both RBAC and session security checks.
- Use basis points for chance comparisons and clamp every configured percentage.

## Compatibility

The implementation uses script hooks present in the targeted AzerothCore revision and does not patch core files. If a future core changes `PlayerScript` signatures, adapt only `mod_item_enchant_system.cpp`; the domain components are independent of hook registration.
