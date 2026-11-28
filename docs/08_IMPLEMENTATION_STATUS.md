# 구현 상태 및 검증 결과 — 2026-11-27

> 최신 추가 변경은 하단 **CMake Build UX 개선 / Custom Module — 2026-11-28** 절을 참조하세요. 초기 MVP 결과는 당시 기록으로 유지합니다.

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

## 첫 실사용 UX 개선 — 2026-11-27

### 사용자 실사용 확인 및 이번 구현

사용자가 이번 요청에서 다음 결과를 보고했습니다: Qt GUI 프로젝트 생성과 2-Element Supervisor 실행 PASS, VS Code 일반 CMake Build PASS, VS Code에서 Loop 수정 후 재빌드 PASS, Project Manager에서 수정된 로그 출력 PASS. 이는 위 초기 MVP 당시의 IDE 미검증 기록 이후에 이루어진 **사용자 확인 결과**입니다. 이번 자동화 검증과 구분하여 기록합니다.

이번 변경은 다음 두 기능을 추가합니다.

1. **Supervisor 인자 없는 실행.** 생성된 `test1_supervisor`에 native CMake가 프로젝트 Manifest 경로와 현재 `$<CONFIG>`의 artifacts 경로를 기록합니다. `./build/test1_supervisor`로 실행하며 명시적인 `PROJECT_JSON CMAKE_ARTIFACTS` 방식도 유지합니다. cwd/부모 폴더/mtime/glob으로 후보를 추측하지 않습니다. Manifest 프로젝트 ID 및 artifacts의 현재 Supervisor 바이너리 매핑을 확인하고, 누락·중복·잘못된 경로는 Bringup 시작 전에 오류로 반환합니다. Core Setup/Run과 lifecycle 정책은 변경하지 않았습니다.
2. **Element별 CMake Build 설정.** 선택적인 schema-v1 `build` 객체로 추가 sources, include directories, structured find_package, link libraries/targets, compile definitions, compile/link options를 제공합니다. CLI `set`과 GUI **CMake Build** 탭이 동일 검증·생성기를 사용합니다. 패키지는 표에서 편집하고 나머지 항목은 한 줄에 하나씩 입력합니다. 표준 `find_package` 및 `target_* (... PRIVATE ...)` 명령만 관리 영역 `cmake/elements.cmake`에 생성하며 사용자 루트 CMakeLists는 보존합니다.

추가 구현 파일은 `src/build_settings.cpp` / `src/build_settings.hpp`이며, 기존 library/header, CLI, GUI, templates와 세 테스트를 확장했습니다. `CMakeLists.txt`는 새 library 소스와 template 변경 감지를 등록합니다.

### v1 호환 및 안전한 갱신

- 기존 manifest에서 build 필드가 없으면 빈 설정으로 처리합니다. 스키마 번호를 바꾸거나 Open 시 파일을 다시 쓰지 않습니다.
- 기존 템플릿 갱신은 CLI `regenerate --dry-run` → `regenerate`, 또는 GUI **Preview / Update Generated Files**의 Apply로 수행합니다. 갱신 후 native CMake configure/build가 필요합니다.
- 기존 관리 파일의 정확한 ownership baseline을 비교한 뒤에만 적용합니다. 사용자 C++와 CMakeLists, UUID를 보존합니다. 수동 수정된 관리 파일은 기존 충돌 정책에 따라 거부합니다.
- `build` patch는 생략한 하위 목록을 보존하고 지정한 목록만 교체합니다. `[]`는 목록만 비우며 source 파일은 삭제하지 않습니다.
- `tests/fixtures/schema1-project.json`에는 변경 전 v0.1.0 바이너리로 생성한 실제 프로젝트와 ownership 내용을 보관했습니다. 해당 fixture의 사용자 코드/CMake를 수정한 후 읽기·미리보기·갱신·빌드 설정 적용의 보존을 검증합니다.
- 새 build 필드를 이해하지 못하는 이전 Manager로의 재생성/downgrade는 지원하지 않습니다. 생성 결과의 native CMake 빌드는 계속 Manager 없이 가능합니다.

### 검증 명령과 결과

```bash
cmake -S . -B build -DKCF_PM_TEST_SDK="$PWD/.validation/sdk"
cmake --build build -j4
ctest --test-dir build --output-on-failure

cmake -S . -B build-headless -DKCF_PM_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-headless -j2
ctest --test-dir build-headless --output-on-failure
```

실제 SDK/Qt 실행은 앞서 확인한 sandbox Unix socket 제한 때문에 승인된 sandbox 밖에서 수행했습니다. 테스트는 기존 설치 KCF 5.6.2 SDK를 사용하며 Core/Tools를 수정하거나 다시 패키징하지 않았습니다.

| 테스트 | 결과 | 추가 검증 내용 |
| --- | --- | --- |
| `project_contract` | **PASS, 1.30s** | 실제 구버전 v1 fixture migration, ID/manifest 보존, 사용자 C++/CMake 보존, dry-run/idempotency, build 부분 갱신/비우기, 타입·표현식·예약 키워드·package conflict 거부. 기존 lock/rollback/conflict 회귀 유지 |
| `sdk_integration` | **PASS, 57.67s** | 아래 native build/run 시나리오와 기존 standalone/Supervisor/Release 회귀 |
| `qt_workflow` | **PASS, 20.48s** | 실제 GUI 필드에 source/include/package/link/definitions/options를 입력·저장하고 전체/개별 build 및 standalone/Supervisor run/stop. Offscreen 실제 Qt widgets/backend 사용 |
| 전체 CTest | **3/3 PASS, 0 FAIL, 79.47s** | 2026-11-27 23:06–23:07 KST 전체 실행 |
| Headless Release | **빌드 PASS, contract PASS 1.72s** | Qt OFF 상태에서 동일 build-settings/migration 계약 확인 |

전체 회귀 이후 package component가 `NO_CMAKE_PATH`/`OPTIONAL` 같은 find_package 옵션으로 해석되지 않도록 예약 키워드 검사를 보강했습니다. 최종 Manager 재빌드 PASS 및 해당 오류 사례를 추가한 targeted `project_contract` 재실행 **PASS 2.39s**를 확인했습니다. 정상 설정의 CMake 출력과 runtime 코드는 이 보강에서 변경하지 않았습니다.

SDK 통합 테스트의 추가 시나리오:

- 프로젝트 루트에서 인자 없는 Supervisor 실행 PASS; 다른 cwd에서 동일 바이너리 실행 PASS; 명시적인 두 경로 실행 PASS.
- Debug 및 Release 각각 native CMake가 기록한 기본 artifacts로 실행 PASS.
- Manifest 누락/잘못된 JSON/잘못된 schema/다른 프로젝트 ID, artifacts 누락/중복 target/다른 Supervisor 매핑, 인자 하나만 전달한 경우 오류 확인 PASS. 실패 경로에서 `[Bringup] started` 로그가 없는 것도 확인했습니다.
- 기본 Debug artifacts를 제거하고 같은 폴더에 Release 이름 파일만 둔 경우, 이를 추측해서 사용하지 않고 오류 반환 PASS.
- 테스트에서 실제 외부 정적 라이브러리를 CMake로 빌드·설치하고 `ExampleDependency::external` exported target으로 연결 PASS. SDK mock이나 루트 CMake 수동 의존성 추가를 사용하지 않았습니다.
- 공백을 포함한 추가 source/include 경로, 따옴표를 포함한 compile definition, compile option으로 전달한 macro, link option을 사용한 native CMake configure/full/target build PASS. Element Setup에서 외부 라이브러리를 호출하는 추가 source 함수의 결과까지 확인했습니다.
- CLI에서 compile definition만 부분 갱신한 뒤 **일반 `cmake --build`**로 재빌드·실행 PASS. CMake가 관리 include 변경을 감지하므로 별도 Manager build hook이 필요하지 않습니다.
- 위 변경 전후 사용자 root CMakeLists는 바이트 단위로 동일했고 사용자 C++ SHA-256도 유지됐습니다. 기존 OUTPUT_NAME/출력 디렉터리 사용자 설정 역시 유지됐습니다.

개발 중 CMake 인자 quoting helper가 C++ `std::quoted`와 이름이 겹쳐 첫 컴파일이 실패했습니다. helper 이름을 분리한 뒤 빌드 및 위 전체 회귀가 통과했습니다. 남아 있는 제품 테스트 FAIL은 없습니다.

### 범위 및 제한

- 이번 VS Code 호환성 검증은 같은 native CMake 경로의 자동화입니다. 이번 추가 UI의 VS Code 수동 조작을 새로 수행한 것으로 주장하지 않습니다. 초기 실제 VS Code 사용 결과는 위 사용자 보고와 구분합니다.
- no-argument 기본 경로는 build-tree 절대 경로입니다. 디렉터리 이동/배포 시 새 위치에서 configure/build하거나 올바른 명시적 경로·metadata를 사용해야 합니다. 자동 재배치 탐색은 제공하지 않습니다.
- 현재 자동화는 단일 config generator의 Debug/Release를 각각 검사합니다. multi-config generator 전용 실행 검증은 미실시입니다.
- 패키지는 REQUIRED/minimum-version/auto·config·module/components만 지원합니다. 동일 패키지의 서로 다른 요청은 거부합니다. Target 설정 scope는 PRIVATE이며 임의 CMake 명령, generator expression, 특수 search policy는 사용자 CMake에 남깁니다.
- 구조를 바꿀 수 있는 CMake 키워드 및 `;`, `$`, backslash, 줄바꿈을 설정 리터럴에서 거부합니다. 실제 source/package/target의 존재와 유효성은 CMake가 검사합니다.
- KCF Core/Tools 소스 변경, Git commit 또는 push는 수행하지 않았습니다.

사용 예시와 업그레이드 절차는 [사용 설명서의 UX 확장](07_USER_GUIDE.md#첫-실사용-ux-확장-기존-프로젝트-갱신)을 참조하세요.

## CMake Build UX 개선 / Custom Module — 2026-11-28

사용자는 Ubuntu VM에서 기존 프로젝트/Element 생성, 일반 CMake Build, Supervisor 실행 및 Eigen3 외부 라이브러리 연결을 실제 검증했다고 보고했습니다. 이 사용자 실사용 결과와 아래 자동화 결과는 구분합니다.

### 변경 사항

- CMake Build 입력 6개에 요청한 항목별 placeholder를 그대로 적용했습니다: `ex) test_module/module1/src/module1.cpp`, `ex) test_module/module1/include`, `ex) Eigen3::Eigen`, `ex) USE_IMU=1`, `ex) -Wall`, `ex) -pthread`. 항목 용도·프로젝트 루트 기준·절대경로·줄 단위 입력을 tooltip과 안내로 제공합니다.
- GUI **Add Custom Module** 대화상자를 추가했습니다. Module/Source directory browse, include directory browse, `.cpp` 복수 파일 browse, 직접 경로 입력, 검증된 파일 목록 미리보기와 오류 표시를 제공합니다.
- Source directory는 선택 범위입니다. 파일을 스캔/glob하거나 디렉터리 자체를 source로 등록하지 않습니다. 선택한 실제 `.cpp`만 기존 `target_sources()`에 반영합니다.
- 대화상자의 **Add to build settings**는 기존 CMake Build 초안에만 병합합니다. 기존 **Preview / Save Settings**에서 검토 후 적용합니다. 런타임 설정 초안과 기존 package/link/options는 유지합니다.
- Qt-free 공통 라이브러리의 `custom_module_settings`/`configure_custom_module`과 CLI `add-module PROJECT ELEMENT MODULE_JSON_FILE [--dry-run]`을 추가했습니다. GUI/CLI가 같은 path 검증과 schema-v1 build 목록을 사용합니다.
- 존재 여부, `.cpp` 파일 선택, 모듈/Source directory 경계, 기존 및 선택 목록 중복을 검사합니다. 상대·절대 표기 차이, symlink 및 hardlink 별칭으로 같은 파일을 재등록하는 것도 거부합니다. 상대경로는 프로젝트 내부에서만 허용하고 외부 경로는 명시적 절대경로를 요구합니다.
- Module 디렉터리 메타데이터를 manifest에 추가하지 않습니다. 기존 `build.sources`와 `build.include_directories`로 저장하여 schema v1을 유지합니다. 파일 복사·삭제, 사용자 C++ 또는 사용자 소유 root CMakeLists 덮어쓰기가 없습니다. 복잡한 CMake subproject는 사용자 root CMake에서 계속 관리합니다.

주요 변경: `src/custom_module.cpp`(공통 경로 검증), `src/custom_module_dialog.hpp`(Qt 대화상자), `src/gui.cpp`(입력 안내/초안 통합), `src/project.cpp` 및 `src/cli.cpp`(원자적 등록 경로), public header/CMake, `tests/modules.py` 및 기존 SDK/Qt 테스트 확장.

### 실행 검증

```bash
cmake -S . -B build -DKCF_PM_TEST_SDK="$PWD/.validation/sdk"
cmake --build build -j4
ctest --test-dir build --output-on-failure

cmake -S . -B build-headless -DKCF_PM_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-headless -j2
ctest --test-dir build-headless --output-on-failure
```

SDK 및 Qt runtime 테스트는 기존 KCF socket 제한에 대한 승인 범위에서 sandbox 밖으로 실행했습니다. Qt는 offscreen이며 실제 dialog/widgets, 공통 backend, 실제 설치된 KCF 5.6.2 SDK를 사용했습니다.

| 검증 | 실제 결과 |
| --- | --- |
| `module_paths` | **PASS 0.57s**: 내부 상대경로/외부 절대경로, dry-run 무변경, 누락 디렉터리/파일, directory-only·비 `.cpp`·빈 선택 거부, 중복·alias·생성 source 재등록 거부, 상대 외부 접근 차단, 사용자 코드/CMake 보존 |
| `project_contract` | **PASS 1.21s**: 기존 schema v1 migration, 생성·설정·충돌·lock·rollback 회귀 유지 |
| `sdk_integration` | **PASS 44.30s**: `add-module`로 등록한 내부 상대경로 source/include와 외부 절대경로 source를 실제 native CMake로 빌드. 추가 함수 호출 결과까지 확인. 기존 외부 패키지·정의·옵션·Debug/Release·Supervisor 회귀 유지 |
| `qt_workflow` | **PASS 14.94s**: Custom Module 대화상자에서 누락/중복 source 거부, 정상 목록 preview, 실제 Add 버튼 수락, 기존 Save → native build → standalone/Supervisor/Stop 흐름 |
| 전체 Qt 포함 구성 | **4/4 PASS, 0 FAIL, 61.03s** |
| Headless Release | **빌드 PASS, 2/2 PASS, 0 FAIL, 1.36s**: module_paths 0.38s / project_contract 0.97s |

선택되지 않은 `.cpp`에는 의도적으로 `#error`를 넣었습니다. SDK 테스트의 내부/외부 디렉터리와 Qt 테스트에서 빌드가 통과하여 디렉터리 glob 없이 실제 선택 파일만 등록됨을 확인했습니다. 신규 path 테스트는 사용자 root CMakeLists, Element 구현 및 module source의 바이트 보존도 검사합니다. 생성 결과는 일반 CMake include의 source/include 목록만 사용하며 Manager를 호출하는 build hook을 추가하지 않았습니다.

### 검증 범위 및 사용 안내

- 이번 테스트는 native CMake 자동화이며 새 기능으로 VS Code UI를 수동 조작한 결과로 주장하지 않습니다. 사용자가 확인한 Ubuntu VM/Eigen3 사용 결과는 본 절 첫 문단에 별도 기록했습니다.
- Qt 테스트는 실제 대화상자 검증·추가/저장 경로를 실행하지만 OS 파일 선택 창에서 사람이 파일을 고르는 동작은 자동화하지 않았습니다.
- Custom Module 등록 시 실제 파일 존재를 검사합니다. 이후 외부 편집/이동/삭제에 대한 감시·파일 복사 기능은 없으며, 변경된 파일은 CMake에서 다시 검사합니다. 일반 `set`의 기존 generated-source 사용 정책은 바꾸지 않았습니다.
- 대화상자에서 선택한 source/include는 해당 module/source 경계 안에 있어야 합니다. 독립된 외부 모듈은 별도로 명시적 절대경로로 등록합니다. header-only 또는 복잡한 빌드 구성은 기존 include 설정/사용자 CMake를 사용합니다.
- KCF Core API/Tools를 수정하지 않았고 Git commit/push를 수행하지 않았습니다. 이전 작업의 미커밋 변경도 유지했습니다.

[Custom Module 사용법 및 CLI JSON 예시](07_USER_GUIDE.md#custom-module-등록--2026-11-28)를 참조하세요.
