---
title: "Smoke tests"
weight: 5
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T00:00:00+01:00
---

Smoke tests check that the built and installed pieces start and talk to each
other: daemon, D-Bus API, CLI, plasmoid and, when available, the real monitor.
They are quick and shallow: if a smoke test fails, there is no point running
anything deeper.

## Levels

| Level | Checks | Environment | Automated |
|---|---|---|---|
| S1 | Build artefacts and metadata are valid | E1/E2 | yes (CI) |
| S2 | Daemon + D-Bus API + CLI with the fake backend | E2/E3 | yes (CI) |
| S3 | Real monitor: DDC/CI and KDE brightness | E4 | opt-in script |
| S4 | Plasmoid loads and works | E3/E4 | manual checklist |
| S5 | Resume and hotplug | E4 | manual checklist |

The script is `tests/smoke/run.sh`. It runs S1–S2 by default; `--hw` adds S3.
It exits non-zero on the first failure and prints which check failed.

## S1: artefacts and metadata

```sh
cmake --install build --prefix "$PREFIX"
qmllint package/contents/ui/*.qml
desktop-file-validate "$PREFIX"/share/applications/io.github.elgorro.monitormodetray.desktop
appstreamcli validate --no-net "$PREFIX"/share/metainfo/io.github.elgorro.monitormodetray.metainfo.xml
kpackagetool6 --type Plasma/Applet --show io.github.elgorro.monitormodetray -p "$PREFIX/share/plasma/plasmoids"
"$PREFIX"/bin/pmmt --version && "$PREFIX"/bin/pmmtd --version
```

Pass: every command exits 0.

## S2: daemon and API with the fake backend

Runs under `dbus-run-session` with `PMMT_BACKEND=fake` and the
`acer-sa243y.toml` fixture (see [Test environments](../test-environments/#e2-container-with-a-private-d-bus)).

| # | Step | Pass when |
|---|---|---|
| 1 | Start `pmmtd` | Service `io.github.elgorro.monitormodetray` appears on the bus within 2 s |
| 2 | `busctl --user introspect … /io/github/elgorro/monitormodetray` | Matches `src/dbus/*.xml` (methods, properties, signals) |
| 3 | `pmmt displays` | Lists the fake Acer as reachable |
| 4 | `pmmt modes` | Lists the modes from the test config |
| 5 | `pmmt apply evening` | Exits 0; fake backend got exactly the changed codes; fake brightness = 7500 |
| 6 | `pmmt status` | Shows `evening` |
| 7 | `pmmt apply evening` again | No writes (nothing changed) |
| 8 | `pmmt apply nosuchmode` | Exits non-zero with `ModeNotFound` |
| 9 | `pmmt capture <id> captured` then `pmmt modes` | `captured` listed with the fake's current values |
| 10 | Edit config, `pmmt reload` | `ConfigChanged` emitted; new mode visible |
| 11 | Stop `pmmtd`, run `pmmt status` | D-Bus activation restarts it (when the `.service` file is installed) |

## S3: real monitor (opt-in)

Runs only with `tests/smoke/run.sh --hw` on [E4](../test-environments/#e4-dev-machine-with-a-real-monitor).
It changes the monitor's settings, so it saves and restores them.

| # | Step | Pass when |
|---|---|---|
| 1 | `ddcutil detect` | The monitor is found |
| 2 | Save current VCP values | Written to a temp file |
| 3 | `pmmt displays` | The monitor is listed with its EDID-based ID |
| 4 | `pmmt get <id>` | Values match `ddcutil getvcp` |
| 5 | Apply a test mode with a different brightness | `ddcutil getvcp 10` and KDE's `Brightness` (`org.kde.ScreenBrightness`) both show the new value |
| 6 | Wait 10 s | KDE has not reset the brightness |
| 7 | Restore saved values | Monitor back to the start state |

## S4: plasmoid checklist

In `plasmoidviewer -a package` (E3), then in the real panel (E4):

- [ ] Icon shows in the tray, monochrome, matches the theme in light and dark
- [ ] Tooltip shows the app name and the active mode
- [ ] Popup lists all modes; the active one is marked
- [ ] Clicking a mode applies it; the tray stays responsive during the apply
- [ ] With `pmmtd` stopped: clear error state, no freeze, recovers when it's back
- [ ] With no monitor reachable: status becomes active and explains why
- [ ] Strings are translatable (no hard-coded text outside `i18n()`)

## S5: resume and hotplug checklist

On E4 only:

- [ ] Apply a non-default mode, suspend, resume: mode is re-applied
- [ ] Unplug and replug the monitor: mode is re-applied
- [ ] Change brightness in Plasma's brightness applet: `pmmt status` stays
      correct and nothing fights back

Record S3–S5 results in the PR description when a change touches
`area: ddc`, `area: kde-integration`, `area: daemon` or `area: plasmoid`.
