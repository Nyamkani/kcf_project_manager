# 06 — Open Decisions / Do Not Invent Contract Details

The items below are explicitly unresolved. Codex should propose options and ask for design approval instead of treating examples as final API.

| Topic | Options/questions | Required constraint |
| --- | --- | --- |
| CLI name and ownership | Extend existing Core `kcf` CLI via supported dispatch or separate `kcf-project` executable? | Core v5.6.2 already owns `kcf` diagnostics binary; avoid binary/package conflicts |
| Project/Launch format | YAML vs JSON; project/launch filenames; one vs two documents | Versioned schema; explicit validation, argv array and stable IDs |
| Runtime Hz | Generated main parses argv/env/manifest? | Configuration changes must not rewrite user `Setup/Loop/Shutdown` |
| Supervisor launch | Generic standalone launcher using public Bringup vs thin generated bootstrap | No change to Core internals/lifecycle |
| CMake edits | Owned generated include, root CMake safe rewrite, hand-managed area | Standard native CMake, no hidden Manager build-time hook, user edits preserved |
| CMake target mapping | File API codemodel or validated generated metadata | Never infer executable from target name alone |
| Compatibility | Minimum Core SDK version, feature checks or explicitly pinned SDK | No accidental patch-version EXACT dependency |
| Safe edits | File ownership fingerprints, file previews, backups, atomic apply and conflict policy | Preserve modified C++ and work without Git |
| Rename/remove | Rename display only vs target/class/filename and source relocation | Stable internal ID; source deletion separately authorized |
| Process ownership | CLI/GUI run and stop semantics | No external/systemd Host Gateway kills, root escalation or silent auto-start |
| GUI integration | Standalone Qt6 GUI with shared backend | Do not merge with KCF Tools runtime observer or build editor |
| Packaging | Native source development first, later independent DEB | Generated apps never need the Manager installed |

## Working defaults (subject to review)

- Linux C++17, Qt6 GUI optional, Qt-independent shared Project Management Library.
- One root `CMakeLists.txt` and CMake-friendly source folders; no mandatory per-Element CMakeLists.
- Native CMake + optional `CMakePresets.json`; VS Code CMake Tools builds unaffected.
- Generator → Headless CLI → GUI → advanced project import/rename.
- Separate Project Manager versioning; public KCF SDK contracts govern compatibility.
- Three-Element UGV02-like fixture before full project import.
- Existing KCF Core APIs remain unchanged; no IDE/extension.

## MVP decisions implemented — 2026-11-27

The user's first-integration instruction authorizes reasonable implementation decisions without per-feature approval. The historical planning questions above are retained as rationale; the following choices are now implemented in this repository.

| Topic | MVP decision |
| --- | --- |
| CLI namespace | Separate `kcf-project`; optional `kcf-project-manager` GUI. Never installs or dispatches `kcf`. |
| Model | `kcf.project.json`, schema integer 1; UUID project/Element IDs, explicit membership and typed runtime settings. nlohmann-json shared by Manager and generated apps. |
| Runtime Hz | Generated bootstrap reads manifest through reserved bootstrap argv; calls public `SetLoopFrequency`. No user source rewrite. Default direct executable run uses 10 Hz. |
| Supervisor | Thin generated C++ executable reads manifest and CMake artifact metadata; constructs public `ElementSpec`; uses `Bringup::Setup(name, specs)` and `Run`. |
| env/workdir public API gap | `ElementSpec` has no per-child environment/workdir fields. Generated Element bootstrap applies them before `ProcessRuntime::Run`; no Core API extension or private headers. Reserved `KCF_` env settings rejected. |
| CMake authority | User-owned root CMakeLists includes managed `cmake/elements.cmake`. CMake `file(GENERATE)` plus `$<TARGET_FILE:target>` produces per-configuration metadata; this is the documented equivalent to File API for generated known targets. |
| SDK compatibility | `find_package(KCF CONFIG REQUIRED)` and public exported targets; no version pin. Actually tested SDK versions are listed in implementation status, not inferred from source compatibility. |
| File ownership | Exact baseline contents (stronger comparison than a noncryptographic hash), user path index, preview, staged replacement/exception rollback, incomplete-transaction refusal, advisory project/build locks. No unconditional overwrite, force mode or source deletion. |
| Process ownership | Foreground CLI, asynchronous GUI worker, `posix_spawnp` structured argv. Stop signals only the direct owned PID; Bringup stops its own children. One Manager operation per project at a time. No background PID adoption. |
| Configuration update | Settings-only patch command; generated manifest is managed. Direct manual manifest edits are readable/validatable but conflict with regeneration until explicitly reconciled. |
| Native builds | Generator output needs installed KCF + nlohmann-json, never Qt or the Manager executable/library. CMake presets have no SDK/checkout-specific path. |
| Packaging | CMake install/export for source users. DEB/release pipeline deferred. |

### Remaining decisions / limits

- No arbitrary import, rename/refactoring, remove/exclude-with-source-removal command, remote deployment, or richer endpoint graph. `supervised=false` excludes an Element from Supervisor without deleting its source or build target.
- Crash recovery is fail-closed and documented, not a multi-file filesystem transaction or automatic post-crash repair. No fsync durability guarantee. External IDE/CMake processes do not participate in advisory Manager locks.
- Runtime Stop has no forced-kill deadline. A hung user Shutdown can delay termination; the Manager will not kill unrelated processes or claim it stopped an unresponsive process.
- Only generated/manifest-declared executable targets are exposed by Manager build selection. Arbitrary extra CMake targets remain available through native CMake.
- No claim of ABI/source compatibility across untested SDK releases. No changes to Core, Tools, IPC, Network, Supervisor lifecycle or Host Gateway are included.

## First-use UX extension — 2026-11-27

- Supervisor no-argument defaults are bound by native CMake to the exact source manifest and `$<CONFIG>` artifacts path. No cwd/ancestor/glob/mtime fallback. Explicit two-path invocation remains supported. Project ID and current Supervisor executable identity are checked before Bringup.
- Optional per-Element schema-v1 `build` object adds literal arrays for sources/includes/libraries/definitions/options and structured required package requests. Absent fields mean empty settings. Partial patches preserve omitted lists; arrays replace as a whole.
- Generated target settings use PRIVATE scope and the existing managed CMake include. Root CMakeLists and all user source files retain ownership. Package requests are shared in directory scope; incompatible requests for the same package are rejected.
- Upgrade is an explicit regenerate/settings preview and apply through existing conflict/rollback machinery. Opening old v1 projects does not rewrite them. Reusing an older generator after introducing new build settings is unsupported; native CMake remains independent.
- CMake expressions, arbitrary commands, advanced scopes and package search policies remain user CMake responsibilities. Default Supervisor paths are build-tree paths, not relocatable deployment metadata.

## Custom Module UX — 2026-11-28

Custom Module is a selection/validation operation, not a new persistent manifest entity. Shared `custom_module_settings` and `configure_custom_module` validate existing explicit paths and merge into schema-v1 build source/include lists. CLI `add-module` applies under the project lock; the Qt dialog adds to the existing draft and Preview / Save Settings remains the apply step. Relative inputs must resolve inside the project; external inputs must be explicit absolute paths. Module/source directories bound the selection but never trigger a scan/glob. User sources and root CMake remain untouched; subprojects stay in user CMake.
