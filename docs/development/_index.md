---
title: "Development"
weight: 10
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T12:00:00+01:00
---

Documentation for people working on pmmt itself.

> **Status:** design phase (milestone v0.1.0). These pages describe the planned
> architecture and test strategy. So far only the build skeleton exists
> (`CMakeLists.txt`, a placeholder `libpmmt-core`, one unit test). Where code
> and docs disagree, fix whichever is wrong in the same commit.

1. [Prerequisites](prerequisites/): tools, libraries and DDC/CI access
2. [Architecture and API](architecture/): modules and the D-Bus API
3. [Test environments](test-environments/): where tests run
4. [Unit tests](unit-tests/): fast tests without hardware
5. [Smoke tests](smoke-tests/): checks that the built pieces work together
6. [Reviews and merging](reviews/): branch protection and who approves

Project rules (specs first, docs with every commit, branches, issues and
milestones) are in `CLAUDE.md` and `CONTRIBUTING.md` at the repository root.
