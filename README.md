# mod-item-enchant-system

Standalone Lineage II-style item enchant system for AzerothCore 3.3.5a.

Targeted core revision: `a16ef90f282452006a99f25985b54f5568cf507d` (2026-07-21).

MY DISCORD SERVER : https://discord.gg/8pNMKwBmAe

## Features

- Normal, Blessed, Safe, Crystal, Event, and administrator scrolls.
- Server-side item selection through the stock gossip window; no client patch, Lua, or custom packet is required.
- Per-level and per-category chance tables in the configuration.
- Global, category, quality, and item-entry maximum levels with deterministic priority.
- Configurable item classes, subclasses, inventory types, bonding types, expansion maps, quality, item level, required level, allowlist, denylist, display denylist, and entry ranges.
- Main-stat, all-stat, or custom-stat scaling with linear, fixed, table, and exponential formulas.
- Optional weapon damage, armor, block, resistance, and milestone aura scaling.
- Per-item persistence, history, cache, request throttling, and one active operation per player.
- Write-ahead operation journal and synchronous atomic commit for crash recovery and duplicate protection.
- English and Russian runtime messages and scroll localizations.
- Player and administrator commands protected by RBAC.

## Installation

1. Copy `mod-item-enchant-system` to the AzerothCore `modules` directory.
2. Re-run CMake and build the server normally.
3. Copy `conf/mod_item_enchant_system.conf.dist` to the server configuration directory. AzerothCore's module config loader will create/use the non-`.dist` copy according to the normal module workflow.
4. Start the server once so the module SQL updater applies the auth, characters, and world SQL files.
5. Grant or distribute the scroll entries `900100` through `900105` as desired.

Do not import the same SQL manually after the module updater has already recorded it.

## Player use

Right-click an enchant scroll. The standard gossip window lists eligible equipped and carried items. Select an item to perform exactly one server-authoritative attempt.

The fallback command is:

```text
.enchant use <scrollBag> <scrollSlot> <targetBag> <targetSlot>
```

Use `.enchant info <bag> <slot>` to inspect the persistent level.

## Important client limitations

A clean 3.3.5a client receives item names and templates from a shared static cache. A server-only module cannot safely rename one particular item instance to `+N Sword`, recolor that instance, or add an instance-specific tooltip line. The level is therefore shown in the selection window, messages, logs, and commands. Implementing per-instance tooltip/name rendering requires a separate client patch and is intentionally outside this module.

Likewise, per-item weapon glow is not exposed by an AzerothCore module hook. The supplied optional milestone aura is a safe character-level visual substitute. No interface files are included or changed.

Wrath heirloom stat distribution is calculated through a core path that does not expose the ordinary per-item stat hook. Heirlooms are disabled by default. Enabling them permits selection, but only the hook-exposed/static portions are guaranteed to scale; full heirloom scaling would require a narrowly scoped core hook.

There is no universal server-side "transmogrified" flag shared by AzerothCore transmog modules. `AllowTransmogItems` is therefore an integration reservation and defaults to allowing items; a specific transmog module must expose its state before a reliable deny check can be connected. The enchant module never reads or modifies third-party transmog tables by itself.

See the `docs` directory for architecture, configuration, administration, and extension details.

## Author
Opertol900

## License
This project is licensed under the GNU AGPL v3.
