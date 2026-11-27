# 04 — Source-grounded UGV02 Case Study

**Reference:** User-supplied `ugv02.zip` inspected on 2026-11-27. The ZIP is **not distributed in this repository**. Findings below describe source code, not a performed build, device test or complete application acceptance.

## Relevant observed files

- `CMakeLists.txt`
- `src/main.cpp`, `src/driver.cpp`, `include/ugv02/driver.hpp`
- `src/motion_controller.cpp`, `src/sensor_processor.cpp`, `src/joystick_controller.cpp`
- `src/gateway_element.cpp`, `src/bringup_main.cpp`

## Concrete observations

1. **Entry-point inconsistency.** Driver implementation and `src/main.cpp` are split. `src/motion_controller.cpp` contains both the `MotionController` Element class and its own `main()` (49 lines overall); Sensor Processor, Joystick and Gateway also embed their own entry points. All valid C++, but no single discoverable scaffold.
2. **Per-Element runtime boilerplate.** Driver `src/main.cpp` parses `--port` and `--boot-wait-ms`, detects execution mode, creates Driver, sets `ProcessRuntime` to 50 Hz, and calls `Run(element)`. Motion Controller repeats `ProcessRuntime`/50 Hz setup. Changing runtime Hz often means altering compiled source.
3. **Hand-written Bringup composition.** `src/bringup_main.cpp` builds a five-entry `std::vector<bringup::ElementSpec>` for `ugv02_driver`, `ugv02_motion_controller`, `ugv02_sensor_processor`, `ugv02_gateway_element`, `ugv02_joystick_controller`; sets Driver startup timeout 20000 ms and Gateway Element shutdown timeout 4000 ms. Paths, argv and timeouts are configured in C++.
4. **Manual build-target linkage.** Root `CMakeLists.txt` defines executable targets and has to relate the Bringup target to its child executables. Element additions can require edits in CMake and Bringup in addition to application source.
5. **Custom Gateway Element.** `src/gateway_element.cpp` (118 lines) wraps a host Gateway with UID checks, `fork/execv`, `waitpid`, signals and TERM/KILL child reaping. It is source evidence of historical application-level orchestration, **not** a template to replicate. Future default policy is externally managed singleton Host Gateway.
6. **SDK exact-version pin.** The archived CMake requests `find_package(KCF 5.6.0 EXACT CONFIG REQUIRED)`. Newly generated apps should use a compatible version/SDK contract rather than unintentionally hardcoding a single patch release.
7. **Archive limitations.** CMake test targets refer to `tests/` inputs not present in the supplied ZIP. Thus the archive alone cannot substantiate a complete build/CTest run.

## UX problem vs application complexity

UGV02 Serial driver, sensor processing, state machines and safe motor handling are legitimate application responsibilities. **Creating/duplicating runtime entry points, binding CMake targets, maintaining manual ElementSpec lists and forwarding argv** are repetitive developer-experience burdens that should be managed by optional tooling.

| Workflow | Archive | Proposed new experience |
| --- | --- | --- |
| New Element | Write Element class and bootstrap, edit CMake + Bringup | Generate class/main/target/config with safe diff |
| Runtime Hz / argv / timeout | Edit and rebuild configuration C++ | Edit manifest/config without rewriting user code |
| Build | Native CMake already possible but manually maintained | Native CMake still works; Manager, GUI and CLI are optional |
| Structure before launch | Read CMake and Bringup code | Declared Project/Supervisor/Element/source/target view |
| Gateway ownership | Custom managed wrapper | Shared systemd Host Gateway not auto-owned |

## Comparison boundary

ROS2 mecanum and colcon inspired the desired ergonomics, but this document did **not inspect that other repository**, so it makes no file-by-file ROS2 superiority claim.

## Acceptance fixture

First generate a **three-Element UGV02-like fixture** (Driver, MotionController, SensorProcessor). Verify native CMake + VS Code build, target-only build, same binary standalone/supervised, and source preservation. Then validate a controlled complete copy of the actual five-Element UGV02 structure. Separate virtual automation from OPI5+ real-device checks.
