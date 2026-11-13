---
title: "Prerequisites"
weight: 1
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T12:00:00+01:00
---

## Target platform

| Component | Minimum | Why |
|---|---|---|
| KDE Plasma | 6.0 | Plasma 6 applet API (`PlasmoidItem`, `metadata.json`) |
| Qt | 6.6 | Qt 6 Core, D-Bus, QML; must match the Qt that Plasma uses |
| KDE Frameworks | 6.0 | KI18n, KCoreAddons; ECM for the build |
| ddcutil / libddcutil | 2.1 | DDC/CI access; cross-process locking of I²C buses |
| C++ compiler | GCC 13 / Clang 17 | C++20 |
| CMake | 3.22 | KDE Frameworks 6 baseline |

Only Linux is supported, because DDC/CI goes through `/dev/i2c-*`. Wayland and
X11 sessions are both meant to work.

## Packages

### Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
  qt6-base qt6-declarative qt6-tools \
  ki18n kcoreaddons libplasma \
  ddcutil tomlplusplus \
  plasma-sdk clang appstream desktop-file-utils hugo
```

| Package | Used for |
|---|---|
| `extra-cmake-modules` | KDE CMake modules (`ecm_add_test`, install dirs, clang-format) |
| `qt6-base` | Qt Core, Qt D-Bus, Qt Test, `qdbusxml2cpp` |
| `qt6-declarative` | QML, `qmllint`, `qmlls` |
| `ki18n`, `kcoreaddons` | translations, `KAboutData` |
| `libplasma` | Plasma applet QML modules |
| `ddcutil` | `libddcutil` and the `ddcutil` CLI (debugging, smoke tests) |
| `tomlplusplus` | TOML v1.0.0 parser (MIT, header-only) |
| `plasma-sdk` | `plasmoidviewer` for running the applet outside the panel |
| `clang` | `clang-format`, `clang-tidy` |
| `appstream`, `desktop-file-utils` | `appstreamcli validate`, `desktop-file-validate` |
| `hugo` | preview the docs (optional) |

On other distributions, install the equivalent `-devel`/`-dev` packages. Package
names are tracked in the issue tracker, not here, until someone verifies them.

## DDC/CI access

pmmt talks to the monitor over I²C. Check this before anything else.

1. **Enable DDC/CI in the monitor's on-screen menu.** Many monitors ship with it
   off.
2. **Load the `i2c-dev` kernel module** and make it permanent:

   ```sh
   sudo modprobe i2c-dev
   echo i2c-dev | sudo tee /etc/modules-load.d/i2c-dev.conf
   ```

3. **Give your user access to `/dev/i2c-*`.** ddcutil ships a udev rule for
   this. If `ddcutil detect` only works with `sudo`, add yourself to the `i2c`
   group and log in again:

   ```sh
   sudo groupadd --system i2c 2>/dev/null; sudo usermod -aG i2c "$USER"
   ```

4. **Check it works:**

   ```sh
   ddcutil detect            # lists the monitor with its I²C bus
   ddcutil getvcp 10         # brightness (VCP 0x10)
   ddcutil getvcp scan -t    # all readable VCP codes (slow)
   ```

### Known pitfalls

- Plasma's own power management (PowerDevil) also sets external monitor
  brightness over DDC/CI. pmmt must not fight it. See
  [Architecture: brightness](../architecture/#brightness-and-kde).
- DDC/CI writes are slow (about 50–100 ms each) and some monitors drop writes
  that come too quickly. Docks and USB-C/DisplayPort MST hubs often block
  DDC/CI completely.
- Some monitors' picture modes can't be read back over DDC/CI. This was the
  original Acer SA243Y problem described in the README.

## Building

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`CMakeLists.txt` uses ECM (`KDEInstallDirs`, `KDECMakeSettings`,
`KDECompilerSettings`) and builds as C++20. Build output goes to `build/bin/`
and `build/lib/`.

Format the C++ sources with ECM's KDE clang-format style before committing:

```sh
cmake --build build --target clang-format
```

Configuring copies ECM's `.clang-format` into the source directory. It is
generated, so it is in `.gitignore` and not committed (as the ECM
`KDEClangFormat` docs recommend).

Install into a separate prefix, so you don't touch the system:

```sh
cmake --install build --prefix "$HOME/.local/pmmt-dev"
```

Build and test details: [Unit tests](../unit-tests/), [Smoke tests](../smoke-tests/).
