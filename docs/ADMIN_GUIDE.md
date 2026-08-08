# Administrator guide

## Default scrolls

| Entry | Type | Failure behavior |
|---:|---|---|
| 900100 | Normal | Target item is destroyed |
| 900101 | Blessed | Item survives; level resets to zero |
| 900102 | Safe | Item and current level are preserved |
| 900103 | Crystal | Guaranteed up to `CrystalMaxLevel` |
| 900104 | Event | Configurable multiplier, maximum, and failure mode |
| 900105 | GM | Guaranteed, administrator-restricted, uses `GMMaxEnchant` |

Changing an entry requires changing both world data and the matching `ItemEnchant.Scroll.*` option. Every scroll template must keep `ScriptName=item_mod_enchant_scroll` and a valid on-use spell so the stock client emits its item-use request.

## Commands

Player permission `2000`:

```text
.enchant info <bag> <slot>
.enchant use <scrollBag> <scrollSlot> <targetBag> <targetSlot>
```

Administrator permission `2001` (selected player, otherwise self):

```text
.enchant add <bag> <slot> <levels>
.enchant set <bag> <slot> <level>
.enchant remove <bag> <slot>
.enchant max <bag> <slot>
.enchant reload
.enchant stats
```

The supplied auth SQL links player operations to role `199` and administration to role `196`.

## Auditing and recovery

- `mod_item_enchant_history` is append-only and has one row per completed operation.
- `mod_item_enchant_pending` is the write-ahead journal. Recent pending rows must not be deleted while investigating a crash.
- Old unresolved rows are marked `abandoned` only after seven days at startup.
- `module.itemenchant` logs contain operation ids that match the database history.

For a manual audit:

```sql
SELECT * FROM mod_item_enchant_history WHERE owner_guid = 1 ORDER BY id DESC LIMIT 100;
SELECT * FROM mod_item_enchant_pending WHERE status = 'pending';
SELECT * FROM mod_item_enchant ORDER BY enchant_level DESC LIMIT 100;
```
