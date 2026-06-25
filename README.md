# spacecat_havemax

Limits how many of a given item type a single player may carry **anywhere** in
their inventory — hands, clothing, and every nested container — driven entirely
by a JSON config in the server profile. Optionally pins an item to specific
attachment slots (e.g. "NVG may only be worn, never stashed").

## What it does

- **Count limit:** caps the number of each configured item class across a
  player's whole inventory hierarchy. Picking up a container (into hands or
  inventory) counts the restricted items *inside* it too, so you can't dodge the
  limit by stuffing them in a backpack.
- **Slot restriction (optional):** if an item lists allowed `slots`, it may only
  be attached to those slots and may **not** sit loose in cargo. It can still
  pass through the hands so the player can equip it.
- **Server-authoritative:** all checks run on the server. The config lives in the
  server profile; clients need no copy.
- **Inert until configured:** with an empty config the mod changes nothing.

## Configuration

File: `%profiles%/spacecat/havemax.json` (i.e. `$profile:spacecat/havemax.json`).
Created automatically (empty) on first server start, next to a
`havemax.example.json` reference file. Reloaded every mission start.

It is a JSON **object** with two global flags and an `items` array:

```json
{
    "show_notifications": true,
    "log_events": true,
    "items": [
        {
            "className": "NVGoggles",
            "max": 1,
            "slots": ["NVG", "Eyewear"],
            "notification": "You may only carry one pair of night-vision goggles."
        },
        {
            "className": "Canteen",
            "max": 2,
            "slots": [],
            "notification": "You can carry at most two canteens."
        }
    ]
}
```

**Global flags:**

| Field | Type | Meaning |
|---|---|---|
| `show_notifications` | bool | When `true`, a blocked player is shown the matching item's `notification` text (if non-blank). |
| `log_events` | bool | When `true`, every blocked attempt is logged to the server script log / `.RPT`. |

**Per item (in `items`):**

| Field | Type | Meaning |
|---|---|---|
| `className` | string | Item class to limit. Case-insensitive. A base class (e.g. `Plate_Carrier_Base`) matches all of its subclasses, which share one combined limit. |
| `max` | int | Max number allowed anywhere in one player's inventory. `0` forbids the item entirely. |
| `slots` | string[] | Optional. Attachment slot names the item may occupy. Empty/omitted = count rule only. Non-empty = item may only be worn in those slots and never placed loose in cargo. |
| `notification` | string | Optional. Message shown to the player when this item is blocked (only if `show_notifications` is `true`). Blank/omitted = no message. |

Slot names are DayZ's `InventorySlots` names (e.g. `NVG`, `Eyewear`, `Back`,
`Shoulder`, `Melee`, `Body`, `Hips`).

A `havemax.example.json` showing this format is written next to the live file on
every load. To trace rejections in detail, set `SpacecatHaveMaxLogic.s_Debug = true`
(logs every check + denial to the server `.RPT`).

### Notifications & logging

- A single inventory action calls the engine's `CanReceive*` hooks more than once
  (predictive + authoritative passes, plus drag-hover validation). To avoid a
  burst of duplicate messages/log lines, the notification and log reaction is
  **throttled per player** (~1.5 s). One failed attempt = one message + one log line.
- The notification uses the "important" chat channel (server → client). The log
  line includes the player name/ID, the item, the reason, and the rule.

### Behaviour notes / limits

- The slot rule governs the **item's own class** only. A restricted item already
  nested inside a container the player picks up is counted (count rule) but not
  retro-actively slot-validated, so existing loadouts aren't orphaned.
- At `max`, the player can't take another into hands either — acquiring the
  (max+1)th is blocked at the point of pickup. Direct slot/hand **swaps** of
  already-owned items are unaffected (relocating owned items never double-counts).
- **Lowering `max` below what players already own:** the cargo/attachment hooks
  are also consulted when stored characters load. If a player's persisted
  inventory exceeds a freshly-tightened limit, the surplus items can be rejected
  on load and lost. Tighten limits before players accumulate, or accept the
  cleanup. (This is the same caveat that applies to any inventory-gating mod.)

## Layout

- `config.cpp` — engine declarations (CfgPatches, CfgMods)
- `$PBOPREFIX$` — in-game data path (`spacecat_havemax`)
- `scripts/3_Game`, `scripts/4_World`, `scripts/5_Mission` — Enforce Script modules
- `data/` — models (.p3d), textures (.paa), materials (.rvmat)
- `gui/` — UI layouts (.layout) and controllers
- `workbench/` — Workbench project (`dayz.gproj`, `DayZSetting.xml`) preconfigured with this mod's script paths; open via `/dayz-launch-workbench --mod spacecat_havemax`

## Conventions

See `.claude/skills/_shared/dayz-conventions.md` in the parent template repo for the full DayZ workflow (asset/config/script/server conventions, P:\ requirements, build & deploy pipeline).

## Author

SpaceCat
