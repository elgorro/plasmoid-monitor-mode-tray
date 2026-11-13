---
title: "Acer SA243Y over DDC/CI"
weight: 7
date: 2026-11-13T19:30:00+01:00
lastmod: 2026-11-13T20:15:00+01:00
---

What the Acer SA243Y G0 (the monitor this project started with) accepts over
DDC/CI, and what that means for the mode model. Investigated in issues #12
(picture mode, LBL, RGB gains) and #38 (the other Picture and Performance
OSD options) on 2026-11-13 with `ddcutil` 3.0.2, Plasma 6.8.2, monitor on
DisplayPort. Results were checked against the monitor's own on-screen menu
(OSD).

The fake monitor definition with these values and side effects is
`tests/fixtures/monitors/acer-sa243y.toml`.

## Summary

- The **Low Blue Light (LBL)** mode *can* be set over DDC/CI: vendor code
  `0xE8`, values 0–4.
- Every write to **brightness (`0x10`) switches LBL back to Standard**, even
  when the value doesn't change. This is the original bug from the README.
- Each LBL level sets a fixed brightness of its own. So an LBL mode has to
  set brightness *before* `0xE8`, and nothing may write `0x10` afterwards.
- An evening mode from RGB gains also works (`0x16`/`0x18`/`0x1A`), but a
  gain write switches the colour preset to User.
- Every write to **contrast (`0x12`)** also resets LBL to Standard.
- **Black Boost** is `0xE5` (0–10), readable and writable, and doesn't
  touch LBL.
- **Gamma, ACM, Super Sharpness, Over Drive, Adaptive-Sync, VRB and
  Refresh rate num** are not reachable over DDC/CI. The current refresh
  rate can be read from `0xAE`.
- `0xE1` = 1 **locks the OSD**. It must never be written.
- `0xDC` (Display Mode) is not supported.

## Capabilities

`ddcutil --bus <n> capabilities` reports MCCS 2.2 and these features
(VESA MCCS 2.2a names):

| VCP | Name | Values / range | Notes |
|---|---|---|---|
| `0x04` | Restore factory defaults | write only | never write |
| `0x10` | Luminance | 0–100 | see [LBL](#low-blue-light-0xe8) |
| `0x12` | Contrast | 0–100 | resets LBL, see [Other OSD options](#other-osd-options) |
| `0x14` | Select Color Preset | `05`, `06`, `08`, `0B` | OSD names differ, see below |
| `0x16`, `0x18`, `0x1A` | Video Gain R/G/B | 0–100, default 50 | |
| `0x59`–`0x5E` | 6-axis saturation | 0–100 | |
| `0x60` | Input Source | `01` VGA, `11` HDMI | never write |
| `0x6C`, `0x6E`, `0x70` | Video Black Level R/G/B | 0–100 | |
| `0x87` | Sharpness | 0–4 | found by `getvcp known`, not in caps |
| `0x9B`–`0xA0` | 6-axis hue | 0–100 | |
| `0xCC` | OSD Language | list | |
| `0xD6` | Power Mode | `01`, `02`, `04`, `05` | never write |
| `0xE0`, `0xE1`, `0xE2`, `0xE3`, `0xE4`, `0xE5`, `0xE7`, `0xE8` | manufacturer specific | | see below |

`getvcp scan` also finds codes that are not in the capabilities string:
`0xAC`/`0xAE` (horizontal/vertical frequency, read only), `0xE6`, `0xE9` and
`0xEA`.

`0xDC` (Display Mode), which the architecture used as an example, reads
"Unsupported feature code".

## Colour preset (`0x14`)

The OSD calls this item **Picture mode**. The names ddcutil prints (from the
MCCS value table) don't match the OSD:

| Value | MCCS name | OSD name |
|---|---|---|
| `0x05` | 6500 K | Warm |
| `0x06` | 7500 K | Normal |
| `0x08` | 9300 K | Cool |
| `0x0B` | User 1 | User |
| — | — | LBL (only via `0xE8`) |

Changing Picture mode in the OSD can also change brightness (Normal set from
the OSD moved it 75 → 80) without telling KDE.

## Low Blue Light (`0xE8`)

The OSD item LBL has the options Standard and Level 1–4. `0xE8` maps 1:1:

| `0xE8` | OSD | Brightness the monitor sets itself |
|---|---|---|
| `0` | LBL Standard, Picture mode Warm (`0x14` = `05`) | 80 |
| `1` | LBL Level 1 | 80 |
| `2` | LBL Level 2 | 75 |
| `3` | LBL Level 3 | 65 |
| `4` | LBL Level 4 | 60 |

Writing `0xE8` = 1–4 also selects Picture mode "LBL".

Reading:

- `0xE8` reads back the last value *written over DDC/CI*. A level set in the
  OSD isn't visible: `0xE8` read `0xFF` with LBL level 1 or 2 set by hand. So
  the active LBL state cannot be read reliably.
- No other VCP code changed when the level was set in the OSD, except
  brightness.

**Brightness resets LBL.** With LBL at level 2, writing `0x10` (to 70, and
also to the unchanged 75) set LBL back to Standard; `0xE8` then reads `0`.
Picture mode stays "LBL". KDE/PowerDevil writes `0x10` whenever its stored
brightness differs, which is why the mode kept getting lost. Setting KDE's
brightness to 75 (the level-2 value) "fixed" it only because PowerDevil
then had nothing to write.

## Evening mode from RGB gains

With `0x14` = User and gains R 47 / G 50 / B 36 the picture turns clearly
warm. A slight purple tint remained, so the exact values are a matter of
taste and belong in the user's config. Lowering green below red quickly
gives a purple tint.

- Writing any gain sets `0x14` to `0x0B` (User) by itself.
- Switching `0x14` back to a K preset keeps the gain values (they read the
  same) and they still affect the picture.
- RGB gains alone don't lower brightness; an evening mode also needs a lower
  `0x10`.

## Other OSD options

Each option was changed in the OSD and all candidate codes were read before
and after (`0x10`–`0x1A`, `0x87`, `0xAC`, `0xAE`, `0xE0`–`0xEA`). As a
control, a Black Boost change made in the OSD showed up in `0xE5` at once,
so the method does see OSD changes on codes the monitor reports. Then the
unidentified vendor codes were written and the OSD checked.

| Menu | OSD option | OSD values | VCP | Finding |
|---|---|---|---|---|
| Picture | Contrast | 0–100 | `0x12` | read/write; **any write resets LBL to Standard** |
| Picture | Black Boost | 0–10 | `0xE5` | read/write, OSD changes visible; doesn't touch LBL |
| Picture | Gamma | 1.8, 2.0, 2.2, … | — | not reachable |
| Picture | ACM | Off / On | — | not reachable |
| Picture | Super Sharpness | Off / On | — | not reachable |
| Performance | Over Drive | Off / Normal / … | — | not reachable |
| Performance | Adaptive-Sync | Off / On | — | not reachable |
| Performance | VRB | Off / Normal / Extreme | — | not reachable |
| Performance | Refresh rate num | Off / On | — | not reachable; `0xE0` = 5 hides the counter until the OSD is opened |
| — | current refresh rate | — | `0xAE` | read only (e.g. 120.10 Hz) |

**Contrast resets LBL.** With LBL at level 2, writing `0x12` = 50 (the
unchanged value) set LBL back to Standard. Picture mode stays "LBL" in the
OSD, while `0x14` then reads `0x05`. This is the same behaviour as a
brightness write. A Black Boost write in the same situation kept LBL at
level 2.

## Other vendor codes

| VCP | Allowed (caps) | Read | Finding |
|---|---|---|---|
| `0xE0` | 00 04 05 | `0xFF` initially, max 6 | OSD/overlay state, not a setting. `4`: no visible effect; `5`: hides the refresh-rate counter until the OSD is opened. Writing it changes what `0xE1` reports (max 1 → 2, value 0 → 1). **Don't use** |
| `0xE1` | 00 01 02 | 0, max 1 | **OSD lock**: `1` locks the OSD menu, `0` unlocks it again (#12 saw "verify failed" on `0`, here it worked). **Never write** |
| `0xE2` | 00–07, 10–12 | 0, max 27 | 0–4 accepted, 5+ rejected. Each value sets its own brightness/contrast (0: 75 or 80, 1: 80, 2: 44, 3: 97/60, 4: 77/56) and a warm tint; the OSD shows no matching item. Meaning unknown, **don't use** |
| `0xE3` | — | `0x13`, max `0x0FFF` | not written (looks like a counter or service value) |
| `0xE4` | — | 2, max 3 | writes `1` and `3` fail verification and change nothing; not Gamma (an OSD gamma change doesn't show up). Read only |
| `0xE5` | — | 0, max 10 | **Black Boost** |
| `0xE6` | not in caps | 0, max 0 | not written |
| `0xE7` | 00 01 02 | `0xFF` initially, max 3 | `0`–`2` accepted, no visible effect on any OSD option (with Adaptive-Sync on or off). Each write changes what `0xE8` reads (`1` → `0xE8` reads 2, `2` → 1) without changing LBL. **Don't use** |
| `0xE9`, `0xEA` | not in caps | `0xFF`, max 6 | not written |

Codes that read `0xFF` before any write keep the written value afterwards;
the original `0xFF` can't be written back. The `0xE7` → `0xE8` effect is a
second reason why `0xE8` can't be trusted as a read of the LBL state.

## Consequences for pmmt

- A blue-light mode for this monitor is `vcp."0xE8" = <level>` plus the
  brightness that level sets. Config validation should warn when a mode's
  brightness doesn't match the level (80/75/65/60).
- **Write order:** codes that reset or set brightness on the monitor side
  (here `0xE8`) come *after* brightness, and nothing writes `0x10` after
  them. See [Architecture: Write order](../architecture/#write-order).
- After applying `0xE8`, KDE's stored brightness must already equal the
  level's brightness, otherwise PowerDevil writes `0x10` later and LBL is gone.
- Re-apply after resume/hotplug must use the same order.
- `CaptureMode` can't capture LBL from the monitor (OSD changes don't show up
  in `0xE8`), so LBL modes have to be written by hand in `modes.toml`.
- Contrast belongs to the same group as brightness: `0x12` must be written
  *before* `0xE8`, too.
- Black Boost (`0xE5`) is a good mode option: order doesn't matter, and it
  can be captured from the monitor.
- Gamma, ACM, Super Sharpness, Over Drive, Adaptive-Sync and VRB can't be
  part of a mode on this monitor. (Adaptive-Sync and refresh rate are
  better handled by KScreen on the GPU side anyway.)
- `0x04`, `0x60`, `0xD6`, `0xE1` (OSD lock) and the unknown vendor codes
  (`0xE0`, `0xE2`–`0xE4`, `0xE6`, `0xE7`, `0xE9`, `0xEA`) belong on the
  "never write" side of the capture allow-list (#19).

## How to reproduce

Use `--bus` from `ddcutil detect`; take a `getvcp known` snapshot before
you start and write the original values back afterwards.

```sh
ddcutil --bus 20 capabilities
ddcutil --bus 20 getvcp known
ddcutil --bus 20 setvcp e8 2      # LBL level 2, brightness becomes 75
ddcutil --bus 20 getvcp 10 e8     # 75, 2
ddcutil --bus 20 setvcp 10 75     # same brightness ...
ddcutil --bus 20 getvcp e8        # ... but LBL is back to 0 (Standard)
```

Check the OSD after each write; it is the only reliable view of the LBL
state.
