# 구현 상태 및 검증 결과 — 2026-11-27

## 결과

v0.1.0 MVP를 구현했습니다. 공통 Qt-free C++17 라이브러리, `kcf-project` CLI, 독립 Qt6 Widgets GUI, 표준 CMake 프로젝트/Element 생성, 실행 설정, 전체/개별 빌드, standalone/Supervisor 실행, 설계 시점 트리, 충돌 검사와 사용자 코드 보존을 연결했습니다.

**최종 검증: 등록된 자동 테스트 3개 모두 PASS(각 최종 실행 기준), 미해결 제품 테스트 FAIL 0개.** 샌드박스 안에서 수행한 최초 Supervisor 테스트는 FAIL이었으며 아래에 원인과 재검증 범위를 명시합니다. VS Code UI와 실제 UGV02/OPI5+ 하드웨어는 미검증입니다.

## 구현 구성

| 파일/계층 | 역할 |
| --- | --- |
| `include/kcf_project/project.hpp` | Qt-independent public model/generator/build/run API |
| `src/project.cpp` | schema 검증, 파일 소유권/충돌/transaction, 템플릿, 프로젝트 트리, CMake/launch adapter |
| `src/process.cpp` | Linux posix_spawn, 구조화 argv, 로그 수집, 직접 자식 신호 전달과 waitpid |
| `src/cli.cpp` | headless 명령, 오류/종료 코드, foreground signal forwarding |
| `src/gui.cpp` | Qt 독립 창, 프로젝트/Element 설정 폼, 미리보기, 트리, 비동기 작업/로그/Stop |
| `templates/*.txt` | 공개 ProcessRuntime/Bringup을 사용하는 독립 앱 bootstrap; Manager 빌드 시 실행 파일 안에 내장 |
| `tests/contract.py` | SDK 없이 생성·설정·보존·충돌·오류·lock·rollback 검사 |
| `tests/integration.py` | 실제 설치 SDK와 3-Element fixture의 native/CLI build/run 검사 |
| `tests/gui.py` | 실제 Qt event loop/widgets 및 공통 backend를 사용하는 offscreen workflow |
| `cmake/KCFProjectManagerConfig.cmake.in` | 공통 라이브러리의 설치/소비용 CMake package |
| `.github/workflows/build.yml` | Qt on/off 빌드 및 generator 계약 CI 정의(원격 CI 실행은 미검증) |

생성물의 `CMakeLists.txt`/presets와 사용자 C++는 사용자 소유입니다. 관리 파일은 별도 추적하며 변경 감지 시 중단합니다. ID는 생성 후 유지합니다. 임의 rename/remove나 소스 삭제는 제공하지 않습니다. 이름 충돌을 피하기 위해 기존 Core `kcf` 실행 파일은 설치하거나 변경하지 않습니다.

## 검증 환경 / 공개 SDK 조사

- Linux x86_64, GCC 13.3.0, Qt 6.4.2, nlohmann-json 3.11.3.
- 인접 Core checkout의 **읽기 전용 조사**: `ProcessElement`, `ProcessRuntime`, `ElementSpec`, `Bringup`, CMake exported targets, 기존 `tools/cli/main.cpp`.
- SDK source: `Nyamkani/kss_control_framework`, commit `be72e74abe81560f8c61587781c4d5c365b164ab`, CMake version **5.6.2**.
- SDK는 이 저장소의 `.validation/sdk-build`에서 빌드하고 `.validation/sdk`에 설치했습니다. 다른 저장소 소스나 빌드 폴더는 수정하지 않았습니다. 종료 시 Core `git status --short`는 비어 있었습니다.
- 실제 설치 SDK의 `KCF::core`, `KCF::bringup`에 생성 앱을 링크했습니다. mock SDK나 private-header 우회는 사용하지 않았습니다. 다른 SDK 버전과 ABI 호환은 아직 검증하지 않았습니다.

## 실제 실행 명령

```bash
# Qt GUI + CLI + library
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4

# SDK: 외부 저장소는 소스 입력으로 읽고, 모든 출력은 현재 저장소 아래에 기록
cmake -S ../kss_control_framework -B .validation/sdk-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/.validation/sdk"
cmake --build .validation/sdk-build -j4
cmake --install .validation/sdk-build

cmake -S . -B build -DKCF_PM_TEST_SDK="$PWD/.validation/sdk"
cmake --build build -j4
ctest --test-dir build -R project_contract --output-on-failure
ctest --test-dir build -R sdk_integration --output-on-failure
ctest --test-dir build -R 'sdk_integration|qt_workflow' --output-on-failure

# GUI 없이 독립 빌드/테스트/설치
cmake -S . -B build-headless -DKCF_PM_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-headless -j2
ctest --test-dir build-headless --output-on-failure
cmake --install build-headless --prefix "$PWD/.validation/manager-install"
```

SDK/Qt runtime 테스트는 승인된 샌드박스 밖 실행으로 검증했습니다. 각 최종 결과: `project_contract` **PASS 0.88s**, `sdk_integration` **PASS 37.27s**, `qt_workflow` **PASS 15.36s**. Headless contract **PASS 0.51s**. Qt 테스트는 `QT_QPA_PLATFORM=offscreen`이며 클릭/설정/비동기 작업을 실제 위젯 이벤트 경로에서 수행합니다. New/Open 파일 선택 대화상자의 수동 조작과 화면 배치는 이 자동 테스트의 범위가 아닙니다.

설치된 공통 라이브러리를 별도 `.validation/consumer` 프로젝트에서 `find_package(KCFProjectManager)`와 `KCFProjectManager::kcf_project`로 링크하고 실행하여 `No changes` 출력을 확인했습니다. `ldd build-headless/kcf-project`에는 Qt/KCF 의존성이 없었습니다.

## 최소 요구사항별 결과

| 검증 | 실제 결과 / 근거 |
| --- | --- |
| 신규 Project 및 3 Elements | PASS: Driver/MotionController/SensorProcessor fixture 생성 |
| Native CMake configure/full/target build | PASS: Manager를 빌드 hook으로 호출하지 않는 표준 CMake |
| CLI 전체/개별 build | PASS: CMake 위임 및 존재하지 않는 target 오류 |
| 실제 실행 파일 위치 | PASS: 사용자 root CMake에서 OUTPUT_NAME=`custom_driver`, 출력 폴더=`custom bin`으로 변경한 target을 올바르게 실행 |
| Standalone Element Run | PASS: 실제 ProcessRuntime Setup/Loop/Shutdown, 정상 정지 |
| Supervisor Application Run | PASS: 3개 자식 READY 및 Supervisor RUNNING 확인 후 정지·회수 |
| 동일 Element 바이너리 | PASS: standalone/supervised 모두 `/proc/<pid>/exe`가 같은 custom_driver 경로 |
| 런타임 설정 | PASS: 공백/shell 문자를 포함한 argv, env, 상대 workdir를 사용자 C++에서 확인. 설정 변경 후 재빌드 없이 재실행 |
| Qt 생성/설정/전체·개별 빌드/단독·Supervisor 실행/정지 | PASS: 실제 Qt offscreen 자동화, 실제 SDK |
| 사용자 C++ 수정 후 재빌드 | PASS: 일반 CMake target rebuild, 재생성 불필요 |
| 설정/재생성 후 사용자 코드 보존 | PASS: 각 사용자 C++ SHA-256 유지; root CMake 사용자 설정 유지 |
| 반복 생성 | PASS: 기존 생성 결과와 ID 유지, zero diff |
| 잘못된 설정 | PASS: schema, ID 중복, Hz, argv 타입, env 예약 이름, timeout, unknown setting/target, 없는 workdir 거부 |
| 파일 안전 | PASS: 생성 파일 수동 수정, 새 파일 충돌, symlink 충돌에서 원본 유지 |
| 동시 작업/롤백 | PASS: flock 경합 거부; 읽기 전용 cmake 디렉터리로 실제 중간 적용 실패 유도 후 내용 복원; 중단된 transaction 쓰기 차단 |
| Headless 및 설치된 라이브러리 소비 | PASS: Qt OFF 빌드·테스트·설치, 별도 CMake consumer 실행 |
| Debug/Release | PASS: 별도 빌드 디렉터리에서 모두 실행 |
| VS Code CMake Tools UI / F7 | **미실시**: 동일 native CMake 경로는 통과했지만 IDE UI 조작을 PASS로 간주하지 않음 |
| 실제 Host Gateway 생명주기 | **미실시**: Gateway 프로세스를 생성/종료하는 코드·테스트가 없음. 운영 서비스 변경 없음 |
| 다른 SDK 버전 / multi-config generator | **미실시**: 구조적으로 config별 metadata 지원, 검증 범위는 단일 config Debug/Release |
| 실제 5-Element UGV02 / OPI5+ | **미실시**: 3-Element 합성 fixture는 실제 장치 검증이 아님 |

## 실패 이력과 해결

1. 최초 sandbox SDK/Qt 실행: standalone은 통과했으나 supervised 자식의 `ProcessRuntime::Run`이 `-71 (EPROTO)`를 반환했습니다. SDK Unix socket 검사가 실패하는 sandbox 환경과 관련됨을 확인했으며, 승인된 sandbox 밖 재실행에서 정상 동작했습니다. Core를 패치하거나 검사를 우회하지 않았습니다. 제한된 sandbox 내 Supervisor 실행은 현재 환경에서 여전히 지원되는 것으로 주장하지 않습니다.
2. 최초 sandbox 밖 SDK 테스트: Element Setup 로그만 기다린 뒤 즉시 정지하여 Supervisor가 아직 INITIALIZING인 상태에서 `-125 (ECANCELED)`를 반환했습니다. 공개 Setup 계약의 정상 취소 동작이므로 제품 동작을 변경하지 않고 테스트를 수정했습니다. 현재 테스트는 Supervisor RUNNING까지 확인한 뒤 정지하며 PASS입니다.
3. 최초 compiler warning: 로그/return의 misleading indentation 경고를 수정했습니다. 최종 Manager 빌드에 해당 경고가 없습니다. SDK 자체 빌드의 기존 경고는 다른 저장소 수정 없이 유지했습니다.

## 제한 사항 / 미구현

- 고급 import, rename/refactoring, source removal, 원격 실행, endpoint graph, DEB 및 배포/릴리스는 미구현입니다.
- per-file atomic rename과 예외 rollback을 제공합니다. 전원 손실 시 완전한 다중 파일 atomicity/fsync 보장 또는 자동 crash recovery는 없습니다. 중단 transaction은 쓰기를 차단하며 수동 복구 절차는 사용 설명서에 있습니다.
- 외부 편집기/native CMake는 Manager advisory lock에 참여하지 않습니다. 동시 수정/빌드는 피해야 합니다.
- foreground CLI/현재 GUI 작업의 직접 자식만 소유합니다. 프로세스 재접속/adoption, 강제 종료 deadline은 없습니다. SIGKILL/Manager crash 이후의 외부 프로세스 자동 정리는 제공하지 않습니다.
- Core가 INITIALIZING인 동안 정지하면 공개 SDK 취소 오류가 반환될 수 있습니다. Core ERROR 상태의 Supervisor는 종료 요청 전까지 대기하는 기존 계약을 유지합니다.
- native configure/build에 nlohmann-json 개발 패키지가 추가로 필요합니다. Core 특정 패치 버전에 고정하지 않지만, 테스트된 SDK는 5.6.2 한 버전입니다.
- 실행 설정 manifest 직접 편집은 읽기/검증에는 반영되지만 추적된 관리 파일 수정이므로 이후 regeneration은 충돌을 보고합니다. GUI/CLI `set`을 사용합니다.

사용법은 [사용 설명서](07_USER_GUIDE.md), 최종 계약 선택은 [설계 결정](06_OPEN_DECISIONS.md)을 참조하세요.
