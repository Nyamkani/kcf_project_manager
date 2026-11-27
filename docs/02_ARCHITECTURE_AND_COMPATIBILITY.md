# 02 — Architecture and Compatibility

> Update 2026-11-27: MVP implementation is now present. This document retains the original requirements; see [implementation status](08_IMPLEMENTATION_STATUS.md), [user guide](07_USER_GUIDE.md), and [resolved MVP decisions](06_OPEN_DECISIONS.md#mvp-decisions-implemented--2026-11-27). Historical “planning” statements below describe the baseline.

## Proposed system

```text
KCF Project Manager (separate repository and version)
├── Qt6 GUI: project creation, settings, structure, build/run
└── Headless CLI: generate, build, run, launch, stop
           │
           ▼
Shared Qt-independent Project Management Library
├── Project model / versioned manifest parser / validation
├── Safe generator and file ownership manager
├── CMake configure/build adapter & target mapping
├── Standalone / Supervisor launcher adapter
└── Design-time structure provider
           │
           ▼
Generated C++17 / CMake / project manifest
           │
           ▼
Installed compatible public KCF SDK
(ProcessElement, ProcessRuntime, Bringup/Supervisor, IPC)
```

VS Code/any IDE edits source and performs normal CMake builds independently. KCF Tools is a separate Runtime observer, not the Project Manager.

## Core boundary — do not change existing behavior

- Retain public `ProcessElement` (`Setup/Loop/Shutdown`), `ProcessRuntime`, `Bringup::Setup/Run`, `ElementSpec`, Supervisor lifecycle/error/ownership and existing IPC/Network policies.
- Do not rely on private KCF headers, internal SHM layouts, internal Gateway APIs or a hardcoded KCF checkout path.
- Project Manager's creation/inspection path must function without a KCF SDK installed; **generated application builds against a compatible SDK**.
- If a required operation cannot be done with the public SDK/launcher, document the blocker and seek a separate decision before any Core API change.
- ABI compatibility is not automatically ensured by source compatibility: validate installed SDK, exported CMake targets, dependencies and link behavior.

## Independent version policy

| Product | Independent version |
| --- | --- |
| KCF Core | e.g. v5.6.2 |
| KCF Tools | e.g. v0.16.0 |
| KCF Project Manager | future v0.1.0 |
| Manifest schema | separately versioned, proposed v1 |

The Manager must **not** need an upgrade merely because Core patch/minor version changes if the public Runtime/Bringup/SDK contracts it uses remain compatible. Avoid unintentional `find_package(KCF 5.6.2 EXACT)` pinning. Choose validated minimum/feature/compatibility checks; allow an explicit project pin when necessary.

## Gateway and process ownership

A Host Gateway is a shared singleton service, typically operated via systemd. A Project Manager-generated Supervisor **must not** implicitly create, own, kill or duplicate it. Launcher controls only explicitly started processes under its own ownership. Do not privilege-escalate, launch arbitrary remote programs, or enable Network Control/Topic permissions without user authorization.

## Design-time vs runtime

- **Design-time:** Project, manifest-declared Supervisor, Element, source/header, CMake target, executable mapping; optional explicitly declared endpoint relationship.
- **Runtime:** Actual PID, ONLINE/ERROR, discovered Publisher/Subscriber, observed Network route, data delivery. These are observed through public KCF diagnostics and KCF Tools.
- Never infer a real Topic route or RUNNING status from a CMake target or static manifest.
