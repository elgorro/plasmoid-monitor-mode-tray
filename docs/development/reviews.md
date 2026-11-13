---
title: "Reviews and merging"
weight: 6
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T00:00:00+01:00
---

`main` is protected. Every change reaches it through a pull request.

## Who does what

| Role | Can |
|---|---|
| Everyone | open issues, fork, open pull requests, review and comment |
| Code owner (`.github/CODEOWNERS`, currently `@elgorro`) | approve, request changes, merge |
| Repository admin | merge a pull request without approval (bypass, PR only) |

Reviews from anyone are welcome and count as feedback. The approval that
unblocks the merge has to come from a code owner.

## Rules on `main`

Set as the repository ruleset `protect-main` (Settings → Rules → Rulesets):

- Changes only through pull requests, no direct pushes
- 1 approving review, which must be from a code owner
- New commits dismiss earlier approvals, and the last push needs approval from
  someone other than its author
- All review conversations resolved before merge
- No force-pushes, no deleting `main`
- Admins may bypass, but only through a pull request
- Merged branches are deleted automatically

Required status checks (build, unit tests, smoke S1–S2) will be added to the
ruleset once CI exists. See [Test environments](../test-environments/#ci).

## Pull request checklist

- [ ] Linked issue (`Closes #n`) with type and priority labels and a milestone
- [ ] Base branch is `main`. Don't stack PRs on other branches: a stacked PR
      merged before its base gets merged into the wrong branch
- [ ] Commits are one topic each, Conventional Commits, signed off (`-s`)
- [ ] Docs under `docs/` updated in the same commits
- [ ] Unit tests added or updated; smoke results for hardware/UI areas (see
      [Smoke tests](../smoke-tests/))
