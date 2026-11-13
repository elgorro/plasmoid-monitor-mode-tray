---
title: "Configuration"
weight: 1
date: 2026-11-13T20:16:44+01:00
lastmod: 2026-11-13T20:16:44+01:00
---

pmmt reads its displays and modes from one file:

```text
$XDG_CONFIG_HOME/pmmt/modes.toml      (usually ~/.config/pmmt/modes.toml)
```

The location follows the XDG Base Directory Specification: if
`$XDG_CONFIG_HOME` is unset or empty, `~/.config` is used. The file is
[TOML v1.0.0](https://toml.io/en/v1.0.0) and must be UTF-8.

## Example

```toml
version = 1

[displays.acer]
id = "ACR:SA243Y:1234ABCD"   # from `pmmt displays`
connector = "DP-8"           # optional hint
after_brightness = ["0xE8"]  # codes the monitor clears when 0x10 is written

[modes.day]
display = "acer"
brightness = 100
contrast = 50

[modes.evening]
display = "acer"
brightness = 60
color_preset = 0x0B          # User; gains only make sense here
red_gain = 47
green_gain = 50
blue_gain = 36

[modes.lowblue]
display = "acer"
brightness = 75              # must match what LBL level 2 sets itself

[modes.lowblue.vcp]
"0xE8" = 2                   # raw VCP code: Acer Low Blue Light, level 2
```

Modes are listed in the order they appear in the file.

## Top level

| Key | Type | Required | Meaning |
|---|---|---|---|
| `version` | integer | yes | Format version. Must be `1`. |
| `displays` | table | no | One `[displays.<name>]` table per monitor. |
| `modes` | table | no | One `[modes.<name>]` table per mode. |

## `[displays.<name>]`

`<name>` is your own short name for the monitor; modes refer to it.

| Key | Type | Required | Meaning |
|---|---|---|---|
| `id` | string | yes | Stable display ID `MFG:MODEL:SERIAL`, built from the EDID. `MFG` is the three-letter EDID manufacturer ID (`A`–`Z`); `SERIAL` may be empty if the monitor has none. Each ID may appear only once. |
| `connector` | string | no | Connector name such as `DP-8`. Only a hint, since it changes with docks and ports. |
| `after_brightness` | array of strings | no | VCP codes (`"0xNN"`) that the monitor clears whenever brightness is written. They are written after brightness. Must not contain `0x10`. See [Write order](../../development/architecture/#write-order). |

## `[modes.<name>]`

`<name>` is what the tray and `pmmt apply <name>` show. A mode must name a
display and set at least one value.

| Key | Type | VCP code | VESA MCCS name |
|---|---|---|---|
| `display` | string | — | Name of a `[displays.<name>]` table. Required. |
| `brightness` | integer | `0x10` | Luminance |
| `contrast` | integer | `0x12` | Contrast |
| `color_preset` | integer | `0x14` | Select Color Preset |
| `red_gain` | integer | `0x16` | Video Gain (Drive): Red |
| `green_gain` | integer | `0x18` | Video Gain (Drive): Green |
| `blue_gain` | integer | `0x1A` | Video Gain (Drive): Blue |
| `vcp` | table | any | Raw codes, see below. |

### Raw VCP codes

Any other code goes into the mode's `vcp` table. Keys are strings of the form
`"0xNN"` (two hex digits, either case); values are integers:

```toml
[modes.lowblue.vcp]
"0xE8" = 2
"0x87" = 3
```

A code that has a named key above must use the name: `"0x10"` in `vcp` is an
error, write `brightness` instead. That way a mode can't set one code twice.

Which codes a monitor supports, and which are safe to write, is
monitor-specific. `ddcutil capabilities` lists them; for the Acer SA243Y see
[Acer SA243Y over DDC/CI](../../development/acer-sa243y/).

### Values

Values are integers from 0 to 65535, since DDC/CI carries a VCP value in two
bytes. TOML allows hex (`0x0B`), so preset codes can be written as in the
MCCS tables. Whether a value is valid for a given monitor (e.g. brightness up
to 100) is not checked here, because the maximum is only known once the
monitor is reached.

## Errors

pmmt rejects the whole file if anything is wrong, and keeps using the last
file that loaded. Each problem is reported as `file:line:column: message`:

```text
/home/me/.config/pmmt/modes.toml:8:1: unknown key `brightnes` in mode `day`
```

These are errors:

- TOML syntax errors;
- a missing or different `version`;
- unknown keys at any level, so typos never silently do nothing;
- values of the wrong type or outside 0…65535;
- raw codes not written as `"0xNN"`, or raw codes that have a named key;
- a display without `id`, a malformed or duplicate `id`;
- a mode without `display`, naming a display that doesn't exist, or setting
  no values;
- `after_brightness` entries that are not `"0xNN"`, repeat, or are `0x10`.
