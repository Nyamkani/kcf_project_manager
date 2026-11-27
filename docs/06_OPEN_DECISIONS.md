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
