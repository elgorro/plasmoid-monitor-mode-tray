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

## Rough plan

- Modes defined in a config file (e.g. `~/.config/monitor-mode-tray/modes.toml`),
  each with VCP values: brightness (0x10), contrast (0x12), colour preset (0x14),
  RGB gains (0x16/0x18/0x1A).
- Applying a mode:
  - send the VCP values with `ddcutil setvcp` (or libddcutil);
  - set KDE's brightness to the same value with
    `kscreen-doctor output.<name>.brightness.<n>` or the KWin D-Bus API.
- "Capture current" action: read the monitor's current values
  (`ddcutil getvcp scan -t`) and save them as a new mode.
- Re-apply the active mode after resume and hotplug.
- Tray UI: a Plasma 6 plasmoid (QML), or a small Python + KStatusNotifierItem
  app as a quicker prototype.
- Optional: switch modes on a schedule (e.g. evening mode at sunset), as an
  alternative to Night Light.

## Notes

- Install `ddcutil`; your user needs to be in the `i2c` group (or have ACL access
  to `/dev/i2c-*`).
- Writes over DDC/CI are slow (~50–100 ms each), so only send values that changed.
