---
title: "Architecture and API"
weight: 2
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T20:16:44+01:00
---

> **Status:** proposed. Changes to this page go through an issue labelled
> `type: docs` + the affected `area:` label.

## Goals

- Apply a **mode** (a named set of VCP values) to an external monitor over
  DDC/CI.
- Keep **KDE's brightness** in sync, so Plasma doesn't reset the monitor later
  (the original Acer SA243Y problem, see README).
- **Capture** the monitor's current values as a new mode.
- **Re-apply** the active mode after resume and monitor hotplug.
- Offer one **API** that the tray, a CLI and scripts all use.

## Overview

```text
 ┌──────────────────────┐   ┌──────────────┐   ┌───────────────┐
 │ Plasmoid (QML)       │   │ pmmt (CLI)   │   │ scripts, cron │
 │ area: plasmoid       │   │              │   │ busctl, qdbus │
 └──────────┬───────────┘   └──────┬───────┘   └───────┬───────┘
            │      session D-Bus: io.github.elgorro.monitormodetray
            └──────────────────────┼───────────────────┘
                        ┌──────────▼───────────┐
                        │ pmmtd (daemon)       │  area: daemon
                        │ D-Bus adaptor, state │
                        │ resume/hotplug hooks │
                        └──────────┬───────────┘
                        ┌──────────▼───────────┐
                        │ libpmmt-core         │  area: core, area: config
                        │ Mode, ModeStore,     │
                        │ ModeApplier, diffing │
                        └───┬──────────────┬───┘
               DisplayBackend│              │BrightnessSync   (interfaces)
          ┌─────────────────┴─┐          ┌─┴──────────────────────┐
          │ DdcutilBackend    │          │ KdeScreenBrightness    │
          │ (libddcutil)      │          │ (org.kde.ScreenBrightness)
          │ FakeBackend       │          │ FakeBrightness         │
          └───────────────────┘          └────────────────────────┘
             area: ddc                      area: kde-integration
```

Every arrow points down. Lower modules never know about higher ones.

## Modules

| Module | Kind | Depends on | Responsibility |
|---|---|---|---|
| `libpmmt-core` | static library | Qt Core, toml++ | Mode model, config parsing/validation, diffing, apply logic. No I/O except config files. |
| `DisplayBackend` | interface in core | — | List displays; read/write VCP codes. |
| `DdcutilBackend` | library | core, libddcutil | `DisplayBackend` over DDC/CI. |
| `FakeBackend` | library | core | In-memory monitor for tests; can load a fixture and inject errors and delays. |
| `BrightnessSync` | interface in core | — | Read/set the brightness KDE stores for a display. |
| `KdeScreenBrightness` | library | core, Qt D-Bus | `BrightnessSync` via PowerDevil's `org.kde.ScreenBrightness`. |
| `pmmtd` | executable | all of the above, Qt D-Bus | Session service: exports the API, owns backends, keeps state, handles resume/hotplug. |
| `pmmt` | executable | Qt D-Bus | CLI. Thin client of the D-Bus API, no backend code. |
| plasmoid | KPackage (QML) + small QML plugin | Qt D-Bus | Tray UI. Thin client of the D-Bus API. |

### Why a daemon

- The plasmoid only lives inside `plasmashell`. Resume/hotplug re-apply, the CLI
  and schedules need a process that runs without the tray.
- One process owns the I²C bus, so writes are serialised and never overlap.
- DDC/CI writes take 50–100 ms each; doing them in the daemon keeps the tray UI
  responsive.

`pmmtd` runs as a systemd user service and is D-Bus activated
(`io.github.elgorro.monitormodetray.service`), so it starts on first use.

### Language and build

C++20, Qt 6, KDE Frameworks 6, CMake + ECM: the KDE standard toolchain, and
`libddcutil` is a C library. The plasmoid UI is QML. An early Python
prototype idea is dropped in favour of a single stack.

### Planned repository layout

```text
src/core/            libpmmt-core (Mode, ModeStore, ModeApplier, interfaces)
src/backends/ddc/    DdcutilBackend
src/backends/fake/   FakeBackend
src/kde/             KdeScreenBrightness
src/daemon/          pmmtd, D-Bus adaptor
src/cli/             pmmt
src/dbus/            D-Bus interface XML (API source of truth)
package/             plasmoid KPackage (metadata.json, contents/ui/*.qml)
src/qmlplugin/       QML module wrapping the D-Bus proxy
tests/unit/          Qt Test unit tests
tests/smoke/         smoke test scripts
tests/fixtures/      TOML configs, fake monitor definitions
docs/                this documentation (Hugo content)
```

## Data model

A **display** is identified by a stable ID built from its EDID: manufacturer
code, model name and serial, e.g. `ACR:SA243Y:1234ABCD`. Connector names
(`DP-8`) change with docks and ports, so they are only a hint.

A **mode** is a name plus a map of VCP code → value. Codes and meanings follow
VESA MCCS:

| VCP | MCCS name | Config key |
|---|---|---|
| `0x10` | Luminance (brightness) | `brightness` |
| `0x12` | Contrast | `contrast` |
| `0x14` | Select Color Preset | `color_preset` |
| `0x16` | Video Gain (Drive): Red | `red_gain` |
| `0x18` | Video Gain (Drive): Green | `green_gain` |
| `0x1A` | Video Gain (Drive): Blue | `blue_gain` |
| other | — | `vcp."0xNN"` |

### Config file

`$XDG_CONFIG_HOME/pmmt/modes.toml` (XDG Base Directory spec), TOML v1.0.0:

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

`after_brightness` lists codes that the monitor resets whenever brightness
(or, on some monitors, another code such as contrast) is written; see [Write order](#write-order). Which codes need it is
monitor-specific. For the Acer SA243Y see
[Acer SA243Y over DDC/CI](../acer-sa243y/).

Unknown keys are an error, not a warning, so typos don't silently do nothing.
The full format and every validation rule are in
[Configuration](../../usage/configuration/); `ModeStore` in `src/core/`
implements it.
Runtime state (active mode per display) lives in
`$XDG_STATE_HOME/pmmt/state.toml`.

## Applying a mode

1. Look up the mode and the display; fail early if either is missing.
2. Read the current values the backend knows (cached since the last apply/read).
3. **Diff:** keep only values that differ.
4. **Write the other codes** one by one through `DisplayBackend`, in this
   order:
   1. colour preset (`0x14`) and vendor picture modes (e.g. `0xDC`);
   2. RGB gains (`0x16`, `0x18`, `0x1A`);
   3. contrast (`0x12`) and any remaining codes, ascending.

   Codes listed in the display's `after_brightness` are skipped here.
5. **Brightness (`0x10`):** if present, set it through `BrightnessSync`,
   not over DDC directly (see below).
6. **`after_brightness` codes** of the mode, ascending. They are written even
   when the diff says they are unchanged, because the brightness write in
   step 5 or a contrast write in step 4 may have reset them on the monitor
   side. Then read `0x10` once: if
   it differs from the mode's brightness, report it (the mode's brightness
   doesn't match what the monitor sets for that code). Don't write `0x10`
   again, that would undo step 6.
7. Optionally read back and verify (config switch, off by default because it
   doubles the bus traffic).
8. Store the active mode in state, emit `ModeApplied`.

### Write order

VESA MCCS defines the codes but no write order, and monitors react differently
to a preset change. Selecting a colour preset or picture mode can reset other
values on the monitor side:

- a colour preset (`0x14`) can reset the RGB gains, so gains come after it;
  on the Acer SA243Y, writing a gain switches `0x14` to User by itself;
- on the Acer SA243Y, each Low Blue Light level (`0xE8`) sets its own
  brightness (level 2 → 75), and **any** `0x10` or `0x12` (contrast) write,
  even of the same value, switches LBL back off.

So everything that selects a preset or mode goes first, fine-tuning values
follow, and brightness is written last. That way the value from the mode is
the one that sticks, and the value KDE remembers matches the monitor.

The exception is a mode code that brightness writes clear, like the Acer's
`0xE8`. It has to come after brightness (and after contrast, which clears it
too on the Acer), and nothing may write `0x10` or `0x12` after it. The display lists such codes in `after_brightness`. For this to stay
stable, the mode's brightness must equal what the monitor sets for that code,
so that KDE's stored brightness already matches and PowerDevil has no reason
to write `0x10` later. Details: [Acer SA243Y over DDC/CI](../acer-sa243y/).

A failed write stops the apply and returns an error that names the VCP code.
Values already written stay written; the error says so.

### Brightness and KDE

PowerDevil already controls external monitor brightness over DDC/CI and exposes
it on the session bus:

- `org.kde.ScreenBrightness` at `/org/kde/ScreenBrightness`:
  property `DisplaysDBusNames` (`as`), signals `DisplayAdded`,
  `DisplayRemoved`, `BrightnessChanged`.
- `org.kde.ScreenBrightness.Display` at `/org/kde/ScreenBrightness/displayN`:
  properties `Label` (`s`), `IsInternal` (`b`), `Brightness` (`i`),
  `MaxBrightness` (`i`); method `SetBrightness(i value, u flags)`.

(Checked by introspection on Plasma 6.8.2. On the dev machine the Acer shows up
as `display0` with `MaxBrightness` 10000 and `Brightness` 7500, i.e. 75 %.)

So pmmt sets brightness **through PowerDevil**, scaled from the monitor's VCP
range to `0…MaxBrightness`. PowerDevil then writes the DDC value itself and
remembers it, which removes the original reset problem. The display is matched
by `Label` and `IsInternal == false`. If no match is found, pmmt falls back to
writing `0x10` over DDC and warns that KDE may overwrite it.

## D-Bus API

The API is the contract between `pmmtd` and every client. The XML introspection
file in `src/dbus/` is the source of truth; C++ adaptor and proxy code is
generated from it with `qdbusxml2cpp`. Names follow the D-Bus specification
("Valid Names") and the KDE conventions in `CLAUDE.md`.

| | |
|---|---|
| Bus | session |
| Service | `io.github.elgorro.monitormodetray` |
| Object | `/io/github/elgorro/monitormodetray` |
| Interface | `io.github.elgorro.monitormodetray.Modes1` |

The `1` in the interface name is the API major version. A breaking change adds
`Modes2` next to `Modes1`; additions within a version are allowed.

### Methods

| Method | Signature | Description |
|---|---|---|
| `ListDisplays()` | `→ aa{sv}` | Displays with `id`, `label`, `connector`, `reachable`. |
| `ListModes()` | `→ aa{sv}` | Modes with `name`, `display`, `values` (`a{yq}`). |
| `ApplyMode(s mode)` | `→ ()` | Apply a mode to its display. |
| `CaptureMode(s display, s name)` | `→ ()` | Read current values and save them as a new mode. |
| `GetValues(s display)` | `→ a{yq}` | Current VCP values (code `y` → value `q`). |
| `ReloadConfig()` | `→ ()` | Re-read `modes.toml`. |

### Properties

| Property | Type | Access | Description |
|---|---|---|---|
| `ActiveModes` | `a{ss}` | read | display ID → active mode name. |
| `ApiVersion` | `u` | read | Minor API version within `Modes1`. |

Properties emit `org.freedesktop.DBus.Properties.PropertiesChanged`.

### Signals

| Signal | Signature | When |
|---|---|---|
| `ModeApplied` | `(s display, s mode)` | A mode was applied successfully. |
| `DisplaysChanged` | `()` | A display appeared, disappeared or became (un)reachable. |
| `ConfigChanged` | `()` | `modes.toml` was reloaded. |

### Errors

Errors are D-Bus error replies named
`io.github.elgorro.monitormodetray.Error.<Name>`:

| Name | Meaning |
|---|---|
| `ModeNotFound` | No mode with that name. |
| `DisplayNotFound` | Display ID unknown or not connected. |
| `DdcFailed` | A DDC/CI read or write failed; message names the VCP code. |
| `InvalidConfig` | `modes.toml` failed validation; message has file:line. |
| `Busy` | Another apply is running on that display. |

### CLI mapping

```text
pmmt displays              → ListDisplays
pmmt modes                 → ListModes
pmmt apply <mode>          → ApplyMode
pmmt capture <display> <name> → CaptureMode
pmmt get <display>         → GetValues
pmmt status                → ActiveModes
pmmt reload                → ReloadConfig
```

## Resume and hotplug

- **Resume:** `pmmtd` listens to `PrepareForSleep(b)` on
  `org.freedesktop.login1.Manager` (system bus). On `false` (resumed) it waits
  for the display to become reachable, then re-applies the active mode.
- **Hotplug:** `DisplayAdded` from `org.kde.ScreenBrightness` triggers the same
  re-apply for that display.
- Re-apply only writes differing values, so it is cheap when nothing changed.
  `after_brightness` codes are the exception (see [Applying a mode](#applying-a-mode)).

## Open questions

Track each as an issue before deciding:

- Schedules (evening mode at sunset): in `pmmtd`, or leave it to Night Light?
- Multiple modes per display active at once (e.g. "gaming" only changing
  response time)?
- Does `CaptureMode` need a VCP allow-list, since `getvcp scan` returns
  codes that are unsafe to write back?
