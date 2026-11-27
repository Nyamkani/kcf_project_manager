# 05 — Codex Implementation Plan / Acceptance Tests

**Status: planning. No code has been implemented or tested by this document.**

## Collaboration and change policy

- Treat these docs as the initial contract for design review, not authority to invent unspecified APIs or delete developer files.
- Propose small, reviewable milestones. Keep application/project management separate from KCF Runtime, IPC, Network and Gateway implementations.
- Do **not** edit `kss_control_framework` or `kcf_tools` or create/merge their PRs without the user's explicit approval.
- For each milestone report changed files, commands, actual observed build/test results, regression scope, limitations and risks. Keep design decisions documented and auditable.
- Never claim physical UGV02/OPI5+ validation from an archive or mocked fixture.

## M0 — Read public KCF contracts and decide MVP details

1. Read Core v5.6.2 public `ProcessElement`, `ProcessRuntime`, `ElementSpec`, `Bringup::Setup/Run`, SDK packaging, installed CMake targets and existing CLI (`kcf status`, `kcf doctor`, etc).
2. Define versioned Project/Launch schema v1 proposal, field validations, relative paths and stable Element identity.
3. Resolve **`kcf` executable namespace conflict** with Core CLI; choose either extension/dispatch policy or separately named project CLI (names currently provisional).
4. Define file ownership and non-destructive generation; decide how Manifest runtime frequency reaches generated `main.cpp` without modifying user logic.
5. Select safe CMake target mapping (prefer codemodel query). Document any public API gaps instead of silently changing Core.

**Acceptance:** project owner reviews schema, CLI namespace, template CMake layout, launch semantics and overwrite safety before code is expanded.

## M1 — Generator / native CMake (highest priority)

- Build a Qt-free project library and generator for a root CMake project, at least two Element class/header sources, separate standard `main.cpp` runtime bootstrap and a versioned project definition.
- Support Add Element with consistent target/source metadata; avoid mandatory independent CMakeLists per Element.
- Build output depends on **compatible installed KCF SDK**, never on Project Manager running or a private Core source checkout.
- Generated project should use native CMake, recommend portable `CMakePresets.json` and resolve CMake target-to-executable without guessing.
- Refuse destructive overwrites and conflicting names. Repeat generation unchanged must yield zero diffs.

**Acceptance commands (examples):**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
cmake --build build --target ugv02_driver
```

Check build from plain terminal **and** normal VS Code CMake Tools; F7 is a configured shortcut, not a required extension. After modifying a user C++ implementation in VS Code, rerun normal build without regeneration. Do not falsely mark a test PASS without running it.

## M2 — Manifest / Headless CLI / Launch

- Validate schema, enumerate design-time project structure, implement full/target CMake build adapter.
- Implement standalone Element run and Supervisor launch using **public** KCF SDK/Bringup behavior; the same Element executable must serve both.
- Support argv arrays, env, workdir, startup/health/shutdown timeouts, working exit codes, diagnostics, and process stdout/stderr.
- Enforce explicit user launch and ownership-aware Stop. Never auto-own or kill the shared Gateway.
- Keep GUI and CLI-independent business logic in the shared project library.

**Acceptance:** missing target, malformed config, duplicate stable IDs/names, timeout/path failures and duplicate process ownership fail clearly; no unsafe cleanup.

## M3 — Separate Qt6 Project Manager

- New/Open Project, Add Element, configure runtime/launch settings, inspect Project tree, preview diff.
- Build All, Build Target, Run Element, Run Supervisor, Stop and log/error display via **shared backend**, not new Qt-only behavior.
- Display Project → Supervisor → Element → Class/Source → CMake Target/Executable before running. Label this as *design-time*, not observed state.
- Resolve GUI/CLI concurrent writes/builds and preserve user code.

**Acceptance:** Qt GUI, CLI and VS Code use identical build sources/targets; no dependency on Manager for native CMake.

## M4 — Refinement / legacy app import

- Non-destructive rename and remove, conflict-aware regeneration with rollback, existing project import and richer Design Graph.
- Test against complete five-Element UGV02 sources in controlled copy, without modifying unrelated repos.
- Physical OPI5+ / systemd / LAN test only after suitable environment exists.

## Minimum test matrix

| Area | Acceptance |
| --- | --- |
| IDE and CMake | Ordinary Configure/Build/F7 + standalone `cmake --build` PASS |
| Target | Full and selected executable target build |
| KCF versions | Contract-compatible Core SDKs build; no unintended EXACT pin |
| User edits | Source hash/content preserved after config/regeneration |
| Idempotency | No diff on repeated same settings |
| Config | Runtime Hz, argv, timeout changes do not rewrite user code |
| Launcher | Identical binary standalone/supervised; correct exit/log ownership |
| Safety | Explicit run, no arbitrary kill/root/Gateway ownership |
| Design/Runtime | Static declarations never masquerade as observed status |
| Failure | Invalid config/target/permissions produce actionable error |
| Parity | CLI/Qt read and write same project contract |

## Review/report template

For each slice: **Goal → files changed → exact commands → PASS/FAIL evidence → known limitations → next proposed decision**. No deployment or release claims without proof.
