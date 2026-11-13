# CLAUDE.md — plasmoid-monitor-mode-tray (pmmt)

KDE Plasma 6 system tray tool for switching an external monitor's picture modes
over DDC/CI, while keeping KDE's brightness in sync. See `README.md` for the idea
and background, and `docs/development/` for architecture and testing.

Machine-specific details (OS, versions, monitors, devices) are in
`CLAUDE.local.md`. That file is gitignored and must never be committed.

## Project rules

### 1. Check the specs first

- Before implementing or changing any protocol, format or interface, read the
  relevant spec (IETF RFCs, freedesktop.org specs, VESA MCCS/DDC/CI, KDE/Qt API
  docs) and follow it.
- Cite the spec and section in code comments and docs where it matters, e.g.
  `RFC 3339 §5.6` or `D-Bus spec, "Valid Names"`.
- If the implementation knowingly differs from a spec, document why.

Specs that are likely relevant:

| Topic | Spec |
|---|---|
| IPC / API | D-Bus Specification (naming, introspection XML) |
| Tray icon | freedesktop.org StatusNotifierItem, Plasma applets (KPackage) |
| Config/data paths | XDG Base Directory Specification |
| Desktop entry / autostart | XDG Desktop Entry Spec, XDG Autostart |
| Icons | XDG Icon Theme / Icon Naming Specification |
| App metadata | AppStream metainfo |
| Config file format | TOML v1.0.0 |
| Monitor control | VESA MCCS (VCP codes), DDC/CI |
| Timestamps | RFC 3339 |
| Requirement keywords in docs | RFC 2119 / RFC 8174 |
| Versioning | Semantic Versioning 2.0.0 |
| Commit messages | Conventional Commits 1.0.0 |

### 2. Documentation in `docs/` — updated with every commit

- `docs/` holds the project documentation as Markdown, structured as a Hugo
  content directory (it can be mounted with `contentDir = "docs"`).
- Every page has YAML front matter with at least `title`, `weight`, `date` and
  `lastmod` (RFC 3339). Bump `lastmod` when you change a page.
- Sections use `_index.md`; a page with assets is a page bundle
  (`docs/<section>/<page>/index.md` plus its files).
- Use relative links or Hugo `ref`/`relref` shortcodes, no absolute site URLs.
- Every commit that changes behaviour, API, config, build or usage MUST create
  or update the matching page under `docs/` in the same commit. If a commit
  really needs no docs change, say so in the commit body
  (`Docs: none needed — <reason>`).

### 3. Git workflow — one topic per branch, one topic per commit

- Never commit straight to `main`. Work on a topic branch, then merge through a
  pull request.
- Branch names: `<type>/<issue-number>-<short-slug>`, e.g.
  `feat/12-capture-current-mode`, `fix/17-resume-reapply`, `docs/2-dev-docs`.
- Each commit covers one topic. Don't mix unrelated changes.
- Commit messages follow Conventional Commits:
  `<type>(<scope>): <summary>` and reference the issue (`Refs #12` / `Closes #12`).
  Sign off commits (`git commit -s`, DCO).
- `CLAUDE.local.md` and other local files stay out of commits.

### 4. Planning with GitHub issues, labels and milestones — mandatory

- All work starts from a GitHub issue. No branch, commit or PR without an issue.
  If none exists, create it first (`gh issue create`).
- Every issue has at least one **type** label and one **priority** label, and a
  milestone.
  - Type: `type: feature`, `type: bug`, `type: docs`, `type: chore`,
    `type: refactor`, `type: test`
  - Priority: `prio: high`, `prio: medium`, `prio: low`
  - Area (optional): `area: plasmoid`, `area: daemon`, `area: core`,
    `area: ddc`, `area: kde-integration`, `area: config`, `area: packaging`
  - Status (optional): `status: blocked`, `status: needs-info`
  - Community: `good first issue`, `help wanted`
- Milestones track releases (`v0.1.0`, `v0.2.0`, …) per SemVer. Keep them up to
  date: assign issues and PRs, close milestones when released.
- PRs link their issue (`Closes #n`) and carry the same labels and milestone.

### 5. License and contributions

- License: MIT (`LICENSE`), copyright elgorro.
- Public repository: https://github.com/elgorro/plasmoid-monitor-mode-tray
- Contributions are welcome under the rules in `CONTRIBUTING.md`: they must be
  compatible with MIT and with free/open-source standards (OSI-approved
  licences, no proprietary code or assets, DCO sign-off).
- Any new dependency must have an OSI-approved licence compatible with MIT.
  Check it before adding.

## KDE / freedesktop conventions

### Names and IDs

| What | Value |
|---|---|
| Repository | `plasmoid-monitor-mode-tray` |
| Short name (CLI, service, logs) | `pmmt` |
| App ID (reverse-DNS) | `io.github.elgorro.monitormodetray` |
| Plasmoid ID (`KPlugin.Id`, package dir) | `io.github.elgorro.monitormodetray` |
| D-Bus service | `io.github.elgorro.monitormodetray` |
| D-Bus object path | `/io/github/elgorro/monitormodetray` |
| D-Bus interface | `io.github.elgorro.monitormodetray.Modes1` (major version in the name) |
| `.desktop` / metainfo | `io.github.elgorro.monitormodetray.desktop` / `.metainfo.xml` |
| Config file | `$XDG_CONFIG_HOME/pmmt/modes.toml` |
| State file | `$XDG_STATE_HOME/pmmt/state.toml` |

- Never use the `org.kde.*` namespace; it is reserved for KDE projects.
- No hyphens in IDs (D-Bus object paths and Flathub reject them).
- D-Bus methods/properties/signals are `PascalCase` (`ApplyMode`, `ActiveMode`,
  `ModeChanged`); interface XML lives in the repo and is the API source of truth.

### Plasmoid

- Plasma 6 applet as a KPackage: `package/metadata.json` + `package/contents/ui/main.qml`.
- `metadata.json`: `KPackageStructure: "Plasma/Applet"`,
  `X-Plasma-API-Minimum-Version: "6.0"`, category `System Information`,
  licence `MIT`, authors with name only.
- Shown in the system tray via `X-Plasma-NotificationArea: "true"` and
  `X-Plasma-NotificationAreaCategory: "Hardware"`.
- Use `PlasmoidItem`, `compactRepresentation` (tray icon) and
  `fullRepresentation` (popup); use Kirigami / PlasmaComponents 3, not Qt Quick
  Controls directly.
- Status: `PassiveStatus` normally, `ActiveStatus` only when something needs
  attention (e.g. monitor not reachable).
- Tooltip: `toolTipMainText` = app name, `toolTipSubText` = active mode.

### Icons, i18n, code style

- Tray icon: monochrome, use existing names from the Icon Naming Spec / Breeze
  first (`video-display`, `brightness-high`); custom icons named after the app
  ID, with a `-symbolic` variant.
- All user-visible strings through `i18n()` (QML) / `i18n()` / `ki18n()` (C++),
  translation domain `plasma_applet_io.github.elgorro.monitormodetray`.
- Follow the KDE Human Interface Guidelines for wording and layout, and KDE
  Frameworks coding style for C++ (clang-format with ECM's KDE config).
- Build with CMake + Extra CMake Modules (ECM), tests via `ecm_add_test` / CTest.
