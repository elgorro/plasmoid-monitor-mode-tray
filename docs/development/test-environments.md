---
title: "Test environments"
weight: 3
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T12:00:00+01:00
---

pmmt touches real hardware and a running Plasma session. Most testing must work
without either, so there are several environments, each for one kind of test.

## Overview

| ID | Environment | Hardware | Session bus | Plasma | Runs |
|---|---|---|---|---|---|
| E1 | Build-only container | no | no | no | build, unit tests |
| E2 | Container + private D-Bus | no (fake backend) | private | no | unit tests, smoke S1–S2 |
| E3 | Dev machine, fake backend | no (fake backend) | private | yes | smoke S1–S2, plasmoid S4 |
| E4 | Dev machine, real monitor | yes | real | yes | smoke S3–S5, manual |

Unit tests must pass in E1. Smoke tests S1–S2 must pass in E2. Only S3–S5
need real hardware.

## E1: build-only container

An Arch Linux container with the packages from
[Prerequisites](../prerequisites/). It is also the CI image (GitHub Actions,
`archlinux:base-devel`).

```sh
docker run --rm -it -v "$PWD":/src -w /src archlinux:base-devel bash
# inside: install packages, then
cmake -B build -G Ninja -DBUILD_TESTING=ON && cmake --build build
ctest --test-dir build --output-on-failure
```

No `/dev/i2c-*`, no D-Bus, no display. Tests that need any of these must not
run here.

### Continuous integration

`.github/workflows/pr-check.yml` runs E1 on every pull request against `main`
and on every push to `main`:

1. install the build dependencies with `pacman` (only what `CMakeLists.txt`
   needs so far);
2. configure (`-G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON`), build;
3. `ctest --output-on-failure`.

It only compiles and tests. It uploads no artifacts and has read-only
`contents` permission. Actions are pinned to a commit SHA. When a new
dependency goes into `CMakeLists.txt`, add its package to the workflow in the
same commit.

Reproduce a CI run locally:

```sh
docker run --rm -v "$PWD":/src:ro archlinux:base-devel bash -c '
  pacman -Syu --noconfirm --needed cmake ninja extra-cmake-modules qt6-base &&
  cmake -S /src -B /tmp/build -G Ninja -DBUILD_TESTING=ON &&
  cmake --build /tmp/build && ctest --test-dir /tmp/build --output-on-failure'
```

## E2: container with a private D-Bus

Same image, plus a throwaway session bus. `pmmtd` runs with the fake backend:

```sh
dbus-run-session -- tests/smoke/run.sh --backend=fake
```

`dbus-run-session` starts a fresh `dbus-daemon`, runs the command with
`DBUS_SESSION_BUS_ADDRESS` set, and tears the bus down afterwards. Nothing
leaks into the real session.

The fake backend is selected with:

| Variable | Meaning |
|---|---|
| `PMMT_BACKEND=fake` | use `FakeBackend` and `FakeBrightness` |
| `PMMT_FAKE_FIXTURE=<file>` | fake monitor(s) to load from `tests/fixtures/` |
| `PMMT_CONFIG_DIR=<dir>` | config directory instead of `$XDG_CONFIG_HOME/pmmt` |
| `PMMT_STATE_DIR=<dir>` | state directory instead of `$XDG_STATE_HOME/pmmt` |

These variables are for testing only and are documented here so they stay
stable.

## E3: dev machine with fake backend

Your normal Plasma session, but pmmt points at fake hardware and a private bus,
so the real monitor and real config are untouched:

```sh
export PMMT_BACKEND=fake PMMT_FAKE_FIXTURE=tests/fixtures/acer-sa243y.toml
export PMMT_CONFIG_DIR=$(mktemp -d) PMMT_STATE_DIR=$(mktemp -d)
dbus-run-session -- bash -c 'build/bin/pmmtd & sleep 1; plasmoidviewer -a package'
```

`plasmoidviewer` (from `plasma-sdk`) runs the applet in its own window, so the
real panel isn't touched. Use `plasmoidviewer -a package -f horizontal` to see
it as it looks in a horizontal panel.

## E4: dev machine with a real monitor

The full stack against real hardware. Before testing:

1. Check DDC/CI works: `ddcutil detect`.
2. **Save the monitor's current values** so you can restore them:
   `ddcutil getvcp 10 12 14 16 18 1A -t > /tmp/pmmt-before.txt`.
3. Install into a dev prefix, not `/usr`:
   `cmake --install build --prefix "$HOME/.local/pmmt-dev"`.
4. Use a separate config dir (`PMMT_CONFIG_DIR`) unless you're testing your
   own config.

Known hardware is listed below. Add your monitor when you test on it.

| Monitor | Connection | DDC/CI | Mode readable | Notes |
|---|---|---|---|---|
| Acer SA243Y | DisplayPort (DP-8) | yes | no | Blue light mode not readable; only brightness changes (100 → 75). |

## CI

CI (GitHub Actions) runs E1 and E2 on every push and pull request:

1. build with `-DBUILD_TESTING=ON`, warnings as errors
2. unit tests (`ctest`)
3. `qmllint` on `package/`
4. smoke tests S1–S2 under `dbus-run-session` with the fake backend

E3 and E4 are manual and listed in the PR checklist when relevant.
