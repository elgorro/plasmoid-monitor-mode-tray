---
title: "Architecture and API"
weight: 2
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T14:30:00+01:00
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

[modes.day]
display = "acer"
brightness = 100
contrast = 50

[modes.evening]
display = "acer"
brightness = 75
red_gain = 100
green_gain = 85
blue_gain = 60

[modes.evening.vcp]
"0xDC" = 3                   # raw VCP code, e.g. a vendor display mode
```

Unknown keys are an error, not a warning, so typos don't silently do nothing.
Runtime state (active mode per display) lives in
`$XDG_STATE_HOME/pmmt/state.toml`.

## Applying a mode

1. Look up the mode and the display; fail early if either is missing.
2. Read the current values the backend knows (cached since the last apply/read).
3. **Diff:** keep only values that differ.
4. **Brightness (`0x10`):** if present, set it through `BrightnessSync`, not
   over DDC directly (see below).
5. **Other codes:** write them one by one through `DisplayBackend`, in MCCS order
   (colour preset before gains, because a preset can reset gains).
6. Optionally read back and verify (config switch, off by default because it
   doubles the bus traffic).
7. Store the active mode in state, emit `ModeApplied`.

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

## Open questions

Track each as an issue before deciding:

- Schedules (evening mode at sunset): in `pmmtd`, or leave it to Night Light?
- Multiple modes per display active at once (e.g. "gaming" only changing
  response time)?
- Does `CaptureMode` need a VCP allow-list, since `getvcp scan` returns
  codes that are unsafe to write back?
