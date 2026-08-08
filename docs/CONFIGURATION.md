# Configuration

All options use the `ItemEnchant.` prefix in `conf/mod_item_enchant_system.conf.dist`.

## Maximum-level priority

The first non-zero match wins:

1. `Max.ByItem`
2. `Max.<Quality>`
3. `Max.<Category>`
4. `GlobalMaxEnchant`

Example:

```ini
ItemEnchant.GlobalMaxEnchant = 25
ItemEnchant.Max.Weapon = 20
ItemEnchant.Max.Epic = 10
ItemEnchant.Max.ByItem = "19019:30,17182:5"
```

## Chance tables

Keys represent the level being reached, not the current level. Values are percentages and may contain decimals.

```ini
ItemEnchant.Rates.Default = "1:100,2:100,3:100,4:90,5:80,10:35,16:10,25:1"
ItemEnchant.Rates.Weapon = "1:100,2:100,3:100,4:85,5:70"
ItemEnchant.ScrollRate.Blessed = 1.15
```

A missing level inherits the closest configured lower level. Chances are stored and rolled as integer basis points (`10000 = 100%`) to avoid floating-point boundary behavior.

## Stat modes

- `1`: Strength, Agility, Stamina, Intellect, and Spirit only.
- `2`: every ordinary item stat plus enabled synthetic values.
- `3`: only ids in `CustomStats`.

Synthetic ids available to mode 3 are:

| ID | Value |
|---:|---|
| 10000 | Armor |
| 10001 | Shield block |
| 10002-10007 | Holy, Fire, Nature, Frost, Shadow, Arcane resistance |
| 10010 | Weapon damage |

`ForbiddenStats` always wins over the selected mode.

Likewise, `BlacklistQualities` always wins over the individual `AllowPoor`, `AllowEpic`, and other quality switches.

## Formula modes

- `1`: `base * (1 + level * LinearPercentPerLevel / 100)`
- `2`: `base + level * FixedAmountPerLevel`
- `3`: `base * (1 + configured table percent / 100)`
- `4`: `base * ExponentialMultiplier ^ level`

Arbitrary expressions are deliberately not evaluated from configuration. This avoids embedding an unsafe expression interpreter in the worldserver; exponential mode covers the requested `BaseStat * 1.03^level` form.

## Bonding and expansion filters

`AllowedBondingTypes` accepts native 3.3.5a `ItemBondingType` values. An empty value accepts every bonding type.

The Wrath `ItemTemplate` structure has no expansion field, so the module never guesses an expansion from item level. Administrators can explicitly classify entries as `0=Classic`, `1=The Burning Crusade`, or `2=Wrath`:

```ini
ItemEnchant.AllowedExpansions = "1,2"
ItemEnchant.AllowUnknownExpansion = 0
ItemEnchant.Expansion.ByItem = "19019:0,28772:1,49623:2"
ItemEnchant.Expansion.Ranges = "20000-29999:1,40000-59999:2"
```

An exact `Expansion.ByItem` mapping takes priority over a range. Unknown entries pass only when `AllowUnknownExpansion` is enabled. Leaving `AllowedExpansions` empty disables the expansion filter.

`AllowTransmogItems` defaults to enabled. AzerothCore has no common transmog-state API, so blocking transmogrified instances requires a small adapter for the particular transmog module in use; this standalone module intentionally does not query assumed third-party tables.

`EnableDatabaseCache` controls eager per-player preload on login. When disabled, levels are loaded lazily and still retained until logout; this preserves operational consistency and prevents periodic equipment reconciliation from repeatedly querying the same item.

## Reloading

`.enchant reload` first removes the old module modifiers from every online player, reloads the configuration, and then applies each equipped item with the new rules. A realm restart is not required for formula, stat-list, or aura changes.
