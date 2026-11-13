# Contributing to plasmoid-monitor-mode-tray

Contributions are welcome — bug reports, ideas, docs and code.

## Free and open-source requirements

- All contributions are licensed under the project's [MIT License](LICENSE).
- Only submit work you have the right to submit. Sign off every commit
  (`git commit -s`) to certify the
  [Developer Certificate of Origin](https://developercertificate.org/).
- New dependencies, code snippets and assets (icons, images, fonts) must use an
  [OSI-approved](https://opensource.org/licenses) licence compatible with MIT
  (or a compatible free-content licence such as CC0 / CC BY for assets). Name
  the licence in the PR.
- No proprietary, closed-source or unclearly licensed material.

## Workflow

1. **Open an issue first** (or pick an existing one). Every issue gets a type
   label, a priority label and a milestone.
2. **Branch per topic**: `<type>/<issue-number>-<short-slug>`,
   e.g. `feat/12-capture-current-mode`.
3. **One topic per commit**, using
   [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/):
   `feat(ddc): read current VCP values` + `Refs #12`.
4. **Update the docs** under `docs/` (Hugo-style Markdown with front matter) in
   the same commit as the change.
5. **Follow the specs** (RFCs, freedesktop.org, VESA MCCS, KDE/Qt API docs) and
   cite them where relevant.
6. Open a **pull request** against `main` that links the issue (`Closes #12`)
   and carries the same labels and milestone.

## Code of conduct

Be respectful and constructive. This project follows the
[KDE Community Code of Conduct](https://kde.org/code-of-conduct/).
