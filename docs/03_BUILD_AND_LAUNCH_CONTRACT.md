# 03 — CMake / Manifest / Build / Launch Contract

> Update 2026-11-27: MVP implementation is now present. This document retains the original requirements; see [implementation status](08_IMPLEMENTATION_STATUS.md), [user guide](07_USER_GUIDE.md), and [resolved MVP decisions](06_OPEN_DECISIONS.md#mvp-decisions-implemented--2026-11-27). Historical “planning” statements below describe the baseline.

## The most important acceptance condition

A generated application must be a **normal independent CMake project**. After generation, it **MUST build with VS Code's ordinary CMake Tools Configure/Build** (including the user's configured F7 shortcut) and native `cmake --build`, without installing, launching, or invoking Project Manager. No IDE extension, hidden build daemon, proprietary build graph or mandatory regenerate-before-build hook.

## Responsibility matrix

| Data / action | Authority |
| --- | --- |
| Sources, target definitions, include/link libraries, compile flags | Standard `CMakeLists.txt` |
| Configure profiles, build directory, Debug/Release | CMake / optional `CMakePresets.json` |
| Project/Element stable IDs, Supervisor membership, argv, timeout, workdir/env | Versioned project/launch manifest |
| Actual executable path per target/configuration | Validated CMake File API codemodel or equivalent generated metadata |
| Build execution | CMake; invoked by native CLI, IDE, KCF CLI or GUI |
| Standalone/Supervised launch | Common launcher using public KCF Runtime/Bringup APIs |

A Manifest must never silently supersede hand-authored CMake definitions. CMake remains readable and independently editable. The project generator must detect conflicts rather than overwrite manual edits.

## CLI examples — provisional (names not finalized)

```bash
kcf project create ugv02
kcf project add-element driver
kcf project add-element motion_controller
kcf build
kcf build --target ugv02_driver
kcf build --config Debug
kcf run ugv02_driver
kcf launch ugv02
kcf project list
```

These commands illustrate user experience only. **KCF Core v5.6.2 already includes a `kcf` diagnostics executable**, so Codex must explicitly resolve CLI package/binary ownership and extension strategy before implementing these spellings. A separate `kcf-project` prototype is acceptable.

## Proposed generated project — not fixed naming or schema

```text
ugv02/
├── CMakeLists.txt
├── CMakePresets.json
├── kcf.project.yaml                # provisional file format
├── include/ugv02/
│   ├── driver.hpp                  # user-owned
│   └── motion_controller.hpp       # user-owned
├── src/
│   ├── driver/
│   │   ├── main.cpp                # generated bootstrap
│   │   └── driver.cpp              # user-owned
│   └── motion_controller/
│       ├── main.cpp
│       └── motion_controller.cpp
└── tests/
```

Element classes expose `Setup/Loop/Shutdown`, while the generated `main.cpp` calls public `ProcessRuntime`. Runtime frequency and parameters should be configurable without rewriting user implementation. At launch, the exact same Element binary is reused standalone or as a Supervisor child.

## Native CMake minimum acceptance

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
cmake --build build --target ugv02_driver
```

Compatible installed KCF SDK/CMake targets may be prerequisites for the **generated application**. No Project Manager service, Qt6 library, source-tree checkout or CLI is required at build time.

`CMakePresets.json` is recommended to align GUI, CLI, VS Code and direct CMake builds; portable settings should not embed local absolute compiler/Qt/SDK paths. CMake File API codemodel should be investigated for actual target→executable mapping, especially multi-configuration builds.

## Manifest and launcher requirements

- Schema version; stable Project/Element IDs distinct from visible names.
- Explicit Application/Supervisor assignment, CMake target or executable reference, argv as **array**, environment, working directory, runtime Hz, startup/health/shutdown timeout.
- Clear validation for missing/ambiguous target, incompatible schema, duplicate IDs/names, invalid paths/argv and permission failures.
- Defined relative-path resolution. No guessed paths based on target names.
- No implicit process launch from opening a project, viewing Design Graph or refreshing the GUI.
- Do not join user arguments into shell command strings; use structured argv and safe process spawning.
- Stop only processes the relevant launcher explicitly owns. Avoid killing external Element, Supervisor or systemd Gateway.

## Non-destructive regeneration policy

- Separate generated-managed and user-authored files with tracked ownership/content fingerprints.
- Preview file-by-file diff; fail or request explicit approval for collisions and modified generated files.
- Stage/validate changes before atomic apply or rollback. Idempotent regen produces no diff.
- Keep stable internal Element ID; distinguish rename of visible name, target, executable, C++ class and directory.
- Default Delete removes registration/configuration, not user source. Source removal requires independent explicit approval.
- Work without Git installed; coordinate concurrent GUI/CLI edits and builds.

## Tests

Verify IDE-native build, native CMake, CLI, GUI, selected target, Debug/Release, no-regeneration rebuild after a VS Code edit, manifest argv, user-code SHA retention, invalid target/schema, exact process ownership, same binary standalone/supervised, and independent Host Gateway lifecycle. Do not represent static code inspection as an executed PASS.
