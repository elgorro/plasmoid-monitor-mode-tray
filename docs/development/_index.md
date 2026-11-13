---
title: "Development"
weight: 10
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T20:16:44+01:00
---

Documentation for people working on pmmt itself.

> **Status:** design phase (milestone v0.1.0). These pages describe the planned
> architecture and test strategy. So far `libpmmt-core` has the config model
> (`Mode`, `ModeStore`, see [Configuration](../usage/configuration/)); the
> rest is milestone v0.2.0. Where code and docs disagree, fix whichever is
> wrong in the same commit.

1. [Prerequisites](prerequisites/): tools, libraries and DDC/CI access
2. [Architecture and API](architecture/): modules and the D-Bus API
3. [Test environments](test-environments/): where tests run
4. [Unit tests](unit-tests/): fast tests without hardware
5. [Smoke tests](smoke-tests/): checks that the built pieces work together
6. [Reviews and merging](reviews/): branch protection and who approves
7. [Acer SA243Y over DDC/CI](acer-sa243y/): what the reference monitor accepts

Project rules (specs first, docs with every commit, branches, issues and
milestones) are in `CLAUDE.md` and `CONTRIBUTING.md` at the repository root.
