# plasmoid-monitor-mode-tray (pmmt)

Idea: a KDE Plasma system tray icon for switching an external monitor's picture
modes over DDC/CI. Example modes: day, evening/blue light, gaming.

## Background

The Acer SA243Y's own blue light mode got reset whenever KDE sent its saved
brightness (100%) over DDC/CI. The mode itself can't be read over DDC/CI. Only
brightness (VCP 0x10) changed when the mode was turned on (100 → 75). Matching
KDE's saved brightness (`kscreen-doctor output.DP-8.brightness.75`) fixed the
reset.

A tray switcher would make this explicit. Each mode is a set of DDC/CI values,
and the tray applies a mode while keeping KDE's brightness in sync, so the two
don't fight.

## Plan

- Modes are defined in `$XDG_CONFIG_HOME/pmmt/modes.toml`. Each mode holds VCP
  values: brightness (0x10), contrast (0x12), colour preset (0x14), RGB gains
  (0x16/0x18/0x1A) or raw codes.
- A small session daemon (`pmmtd`) applies modes over DDC/CI (libddcutil) and
  sets brightness through KDE's PowerDevil, so KDE remembers the same value.
- A Plasma 6 plasmoid (QML) and a CLI (`pmmt`) talk to the daemon over D-Bus.
- "Capture current" saves the monitor's current values as a new mode.
- The active mode is re-applied after resume and hotplug.
- Optional: switch modes on a schedule (e.g. evening mode at sunset).

Stack: C++20, Qt 6, KDE Frameworks 6, CMake + ECM. Details:
[architecture and API](docs/development/architecture.md); planning happens in
GitHub issues and milestones.

## Notes

- Install `ddcutil`. Its udev rule (`60-ddcutil-i2c.rules`, `TAG+="uaccess"`)
  gives the active local session access to `/dev/i2c-*`; no `i2c` group or
  sudo needed. This does not apply over SSH or outside the seat.
- Writes over DDC/CI are slow (~50–100 ms each), so only send values that changed.
