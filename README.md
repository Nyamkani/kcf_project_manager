# KCF Project Manager

> Planning baseline: 2026-11-27 · **Planning only — implementation not started**

KCF Project Manager is an **independent Qt6 project configurator and headless CLI** for standard **Linux C++17 / CMake / KCF** applications. It is not a code editor, VS Code extension, new build engine, or replacement for KCF Core.

- **Qt Project Manager:** Create projects/Elements, edit configuration, preview project structure, build and launch (STM32CubeMX-like workflow).
- **VS Code / any IDE:** Edit generated C++ sources. Ordinary CMake Tools Configure / Build (including the configured F7 shortcut) **must work without Project Manager**.
- **Headless CLI:** Colcon-like one-command build, target build and launch convenience, delegating to native CMake and public KCF contracts.
- **KCF Tools:** Remains a separate runtime observation/control application.

## Documentation

1. [한국어 기획 요약](docs/00_기획_요약_KO.md)
2. [01 — Product scope](docs/01_PROJECT_SCOPE.md)
3. [02 — Architecture and compatibility](docs/02_ARCHITECTURE_AND_COMPATIBILITY.md)
4. [03 — CMake, build, manifest and launch contract](docs/03_BUILD_AND_LAUNCH_CONTRACT.md)
5. [04 — UGV02 case study](docs/04_UGV02_CASE_STUDY.md)
6. [05 — Codex implementation plan and acceptance](docs/05_CODEX_IMPLEMENTATION_PLAN.md)
7. [06 — Open design decisions](docs/06_OPEN_DECISIONS.md)

## Core requirements

1. Generated projects remain **standalone, standard CMake projects**, buildable in VS Code or using `cmake --build` without the Project Manager.
2. Existing public `ProcessElement`, `ProcessRuntime`, `Bringup/Supervisor`, IPC and Network contracts are preserved.
3. GUI and CLI share one Qt-independent project-management library and manifest contract.
4. User C++ code is **never silently overwritten** by a generator or project rename.
5. The Manager is independently versioned; compatible Core upgrades alone do not require a Manager update.
6. One Element binary may execute standalone or supervised, with the same user implementation.
7. The shared Host Gateway is never implicitly spawned, owned or killed by the Manager.

## Source issues

- [KCF Core #9 — Workspace / Manifest / Build / Launch](https://github.com/Nyamkani/kss_control_framework/issues/9)
- [KCF Core #18 — UX roadmap](https://github.com/Nyamkani/kss_control_framework/issues/18)
- [KCF Tools #3 — Project Manager GUI](https://github.com/Nyamkani/kcf_tools/issues/3)

No executable, build workflow or CI has been implemented in this documentation-only baseline.
