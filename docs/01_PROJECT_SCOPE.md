# 01 — Product Scope and User Experience

**Status: planning, 2026-11-27.** Requirements here are agreed product intent; command names and file formats are examples, not final design.

## Motivation

The real UGV02 application mixes user-defined Element logic with recurring configuration work: KCF Runtime `main()`, per-target CMake, C++ `Bringup::Setup()` / `ElementSpec`, forwarded argv, timeouts and optional Gateway wrapper. Some files contain Element class plus `main()`; others split them. This is valid C++ but fails to provide a consistent first-use developer experience.

## Goal

A developer implements `Setup()`, `Loop()`, `Shutdown()`, device drivers, control algorithms and application logic. The Manager generates a standard C++17/CMake project and safely manages project configuration, structure, builds and launches. Think **STM32CubeMX for configuration**, **VS Code for editing**, and **colcon-like one-command CLI UX**.

## Required workflow

1. Create an application project through the Qt GUI or headless CLI.
2. Add Elements such as Driver, MotionController and SensorProcessor.
3. Produce standard `main.cpp` runtime bootstrap files, Element classes/headers, root CMake, project/launch configuration and minimal tests.
4. Edit user-owned code with VS Code (or another editor). No extension or embedded code editor required.
5. Build using ordinary VS Code CMake Tools (including a configured F7 shortcut), native CMake, headless CLI or Qt GUI. All use the same actual CMake source/targets.
6. Run the exact same Element executable standalone or within a Supervisor, with configuration-driven argv, frequency and timeouts.
7. Inspect design-time Project → Supervisor → Element → source/class → CMake Target/Executable without starting the processes; use KCF Tools to observe actual runtime behavior.

## Functional requirements

- **Project/Element generator:** Repeatable standard files and directories; no mandatory per-Element CMakeLists.
- **Manifest/config:** Versioned, validated, clear error semantics, explicit Supervisor membership and execution settings.
- **Build:** Full project and selected target, Debug/Release, CMake-native, no hidden proprietary build engine.
- **Launch:** Single Element or complete Supervisor, explicit user action and safe process ownership.
- **Code preservation:** Stable Element identity distinct from names; no unconditional overwrite or delete; preview/diff/conflict/atomic application; idempotent regeneration; works without Git.
- **Design structure:** Declarative source and target relationships distinguished from observed Runtime/PID and Topic topology.
- **Deployment:** Manager and its Qt GUI optional on SBC; generated project fully usable without them.
- **Compatibility:** Core and Tools independently versioned; rely only on compatible public KCF SDK/Bringup interfaces.

## Out of scope

- VS Code extension, language server, code editor or debugger.
- ROS2 runtime or recreation of colcon's multi-package dependency solver.
- Auto-deploying or owning the Host Gateway, remote arbitrary execution, or automatic control permission changes.
- Changes to existing KCF Runtime/Supervisor/IPC/Network semantics.
- Arbitrary legacy project import and complex rename refactoring in the initial MVP; these remain later work.

## Related work

- [Core #9](https://github.com/Nyamkani/kss_control_framework/issues/9): Core project contract and development convenience scope
- [Core #18](https://github.com/Nyamkani/kss_control_framework/issues/18): parent UX tracker
- [Tools #3](https://github.com/Nyamkani/kcf_tools/issues/3): GUI and design-time project view

The uploaded UGV02 ZIP was reviewed, but must not be represented as an executed build or physical validation.
