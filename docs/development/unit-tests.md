---
title: "Unit tests"
weight: 4
date: 2026-11-13T00:00:00+01:00
lastmod: 2026-11-13T20:22:48+01:00
---

Unit tests check one module at a time, with the hardware and KDE replaced by
fakes. They are the main safety net and must stay fast.

## Rules

- **No hardware, no session bus, no Plasma.** Unit tests run in
  [E1](../test-environments/#e1-build-only-container). Anything that needs a bus
  is a smoke test.
- **Deterministic.** No sleeps, no wall-clock time, no network. Inject a clock
  or timer where timing matters.
- **Fast.** Each test executable finishes in well under a second.
- **Isolated.** Use `QTemporaryDir` for files; never read or write the real
  `$XDG_CONFIG_HOME`.
- **New code comes with tests.** A bug fix starts with a test that reproduces
  the bug.

## Framework and layout

Qt Test, registered with ECM's `ecm_add_test` so CTest runs them:

```cmake
include(ECMAddTests)
ecm_add_test(test_modestore.cpp
    TEST_NAME test_modestore
    LINK_LIBRARIES pmmt-core Qt6::Test)
```

```text
tests/unit/test_<module>.cpp     one executable per module
tests/fixtures/config/*.toml     config inputs (valid and invalid)
tests/fixtures/monitors/*.toml   fake monitor definitions
```

`tests/unit/test_version.cpp` is the smallest working example. Tests that read
fixtures get the path from the `PMMT_FIXTURES_DIR` compile definition, see
`test_modestore` in `tests/unit/CMakeLists.txt`.

ECM's `KDECMakeSettings` also registers an `appstreamtest` in CTest. It passes
trivially until the AppStream metainfo file exists.

Test class: `Test<Module>`; test functions describe the behaviour,
e.g. `applySkipsUnchangedValues()`. Use `_data()` functions for table-driven
cases.

## What to test

| Module | Test file | Cases |
|---|---|---|
| Config parsing | `test_modestore.cpp` | valid config; each MCCS key maps to the right VCP code; raw `vcp."0xNN"` keys; unknown key → `InvalidConfig` with line number; out-of-range value; missing display reference; `version` mismatch |
| Mode diff | `test_modediff.cpp` | nothing changed → empty; only changed codes; unknown current values count as changed |
| Mode applier | `test_modeapplier.cpp` | brightness goes to `BrightnessSync`, not DDC; colour preset written before gains; a failing write stops and names the code; read-back verify on/off; fallback to DDC brightness when no KDE display matches |
| Brightness scaling | `test_brightnessscale.cpp` | VCP max 100 ↔ KDE max 10000; rounding at both ends; non-100 VCP max |
| Display IDs | `test_displayid.cpp` | ID built from EDID fields; matching ignores connector changes |
| Fake backend | `test_fakebackend.cpp` | fixture load; injected errors and delays work, so other tests can trust it |
| State | `test_statestore.cpp` | active mode saved and restored; missing/corrupt state file → empty state, no crash |
| D-Bus types | `test_dbustypes.cpp` | `a{yq}` / `aa{sv}` marshalling round-trip (no bus needed) |

`DdcutilBackend` and `KdeScreenBrightness` talk to real systems, so they are
covered by [smoke tests](../smoke-tests/), not unit tests. Keep their code thin.

## Fakes

`FakeBackend` and `FakeBrightness` live in `src/backends/fake/` (library
`pmmt-fake`) and are used by unit tests and smoke tests alike. They:

- hold VCP values per display in memory, loaded from a fixture
  (`loadFixture()`) or set up in code (`addDisplay()`, `setFeature()`);
- reject writes the monitor would reject: read-only codes, values above the
  maximum or outside the allowed list, unsupported codes;
- reproduce the monitor's side effects from the fixture, so a wrong write
  order shows up as a wrong final value;
- record every successful write in order (`writes()`) and count reads
  (`readCount()`), so tests can assert on order and bus traffic;
- can fail a given VCP code (`failOn(0x16)`) or mark a display unreachable
  (`setReachable()`);
- never sleep: `setDelayMs()` only advances a simulated clock
  (`elapsedMs()`).

`FakeBrightness` records each `setBrightness()` call and, like PowerDevil,
writes `0x10` to the `FakeBackend` it was given. `setResult()` makes it
report `NoMatch` or `Failed` instead.

### Monitor fixtures

`tests/fixtures/monitors/*.toml`, one monitor per file, TOML v1.0.0. Unknown
keys are errors.

```toml
version = 1

[display]                 # the ID is "MFG:MODEL:SERIAL"
manufacturer = "ACR"      # required
model = "SA243Y G0"       # required; also the label
serial = "FAKE0001"
connector = "DP-8"
product_code = 0x0E1C     # informational
mccs_version = "2.2"      # informational

[vcp.0x10]                # continuous feature
value = 75
max = 100

[vcp.0x14]                # non-continuous feature
value = 0x06
allowed = [0x05, 0x06, 0x08, 0x0B]
names = { 0x05 = "Warm" } # informational
writable = true           # default; false makes writes fail

[[effects]]               # writing on_write (with value, if given) ...
on_write = 0xE8
value = 2
sets = { 0x10 = 75 }      # ... makes the monitor set these codes
```

`acer-sa243y.toml` is the reference; its effects come from the real monitor
(see [Acer SA243Y over DDC/CI](../acer-sa243y/)).

## Running

```sh
cmake -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure        # all tests
ctest --test-dir build -R modestore               # one module
build/bin/test_modeapplier applySkipsUnchangedValues   # one function
```

With sanitizers (recommended before a PR):

```sh
cmake -B build-asan -G Ninja -DBUILD_TESTING=ON -DECM_ENABLE_SANITIZERS='address;undefined'
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```
