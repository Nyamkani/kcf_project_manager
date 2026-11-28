# MVP 사용 설명서

## 설치 및 구성

README의 CMake 절차로 빌드합니다. `cmake --install build --prefix /your/prefix`는 CLI, 선택적 GUI, 공통 라이브러리와 헤더, CMake 패키지를 설치합니다. 라이브러리 사용자는 `find_package(KCFProjectManager CONFIG REQUIRED)`와 `KCFProjectManager::kcf_project`를 사용합니다. 이 패키지는 Qt나 KCF Core에 링크하지 않습니다.

생성된 앱은 설치된 KCF SDK와 nlohmann-json을 찾을 수 있어야 합니다. SDK 위치는 `CMAKE_PREFIX_PATH`, GUI의 SDK prefix 또는 CLI `build --prefix`로 전달합니다. 프로젝트에 로컬 SDK 절대 경로를 저장하지 않습니다.

## GUI 흐름

1. **New Project** → 부모 디렉터리와 소문자 이름 → 파일 변경 미리보기 → Apply.
2. **Add Element** → 이름 → 미리보기 → Apply. 기존 프로젝트는 **Open Project**로 엽니다.
3. Element를 선택하고 Hz, Supervisor 포함 여부, startup/health/shutdown timeout, 작업 디렉터리, JSON argv 배열 및 env 객체를 설정합니다. **Preview / Save Settings**로 검토하고 적용합니다.
4. **Build All / Build Element**는 공통 CMake backend를 실행합니다. 빌드 디렉터리와 Debug/Release를 선택할 수 있습니다.
5. **Run Element / Run Supervisor**로 명시적으로 실행합니다. **Stop owned process**는 이 창이 시작한 직접 자식에게 SIGTERM을 보냅니다. Supervisor의 자식 정리는 공개 Bringup 계약에 맡깁니다.
6. 실행 중 창을 닫으면 먼저 정지를 요청하며 종료 완료 후 닫을 수 있습니다. 작업 중 설정·빌드 컨트롤은 잠깁니다. 로그에 PID, stdout/stderr, 종료 코드가 표시됩니다.

왼쪽 트리는 실행 전 선언 구조입니다. 실제 ONLINE 상태나 Topic 연결을 뜻하지 않습니다. 실제 관측에는 KCF Tools를 사용합니다. GUI는 코드 편집기를 제공하지 않습니다.

## CLI와 설정

`kcf-project --help`가 전체 명령을 표시합니다. 모든 명령은 프로젝트 경로를 명시적으로 받습니다. `create`, `add`, `set`, `regenerate`는 `--dry-run`을 지원합니다. 생성 결과 변경이 없으면 `No changes`를 출력합니다.

예를 들어 `/tmp/driver-settings.json`:

```json
{
  "hz": 50,
  "argv": ["--port", "/dev/ttyUSB0", "argument with spaces"],
  "env": {"DEVICE_MODE": "simulation"},
  "workdir": ".",
  "supervised": true,
  "startup_timeout_ms": 20000,
  "health_timeout_ms": 2000,
  "shutdown_timeout_ms": 1500
}
```

```bash
kcf-project set /tmp/ugv02 driver /tmp/driver-settings.json --dry-run
kcf-project set /tmp/ugv02 driver /tmp/driver-settings.json
kcf-project validate /tmp/ugv02
kcf-project regenerate /tmp/ugv02 --dry-run
```

argv는 shell 문자열이 아닙니다. 공백, `$()` 및 세미콜론은 인자로 그대로 전달합니다. 생성된 클래스 생성자는 사용자 인자만 `argc, argv`로 받습니다(프로그램명 제외). 환경 변수는 프로세스 환경에 추가/덮어쓰기하며 `KCF_`로 시작하는 내부 계약 변수는 설정할 수 없습니다. 상대 workdir는 프로젝트 루트 기준입니다. 절대 workdir도 지원합니다. startup timeout에는 JSON 읽기 및 환경·작업 디렉터리 초기화 시간이 포함됩니다.

이름은 `[a-z][a-z0-9_]{0,47}`이며 `supervisor`는 예약어입니다. Hz는 0 초과 100000 이하, timeout은 정수 1..3600000 ms입니다. 버전 1 manifest는 프로젝트와 Element UUID를 가지며 이름 변경은 MVP 명령으로 제공하지 않습니다. 잘못된 schema, 중복 이름/ID/target, 잘못된 인자 타입과 설정을 거부합니다.

CLI는 foreground 방식입니다. Ctrl+C/SIGTERM을 받은 CLI는 자신이 시작한 자식에게 SIGTERM을 전달하고 회수합니다. 외부 PID를 받는 `stop`이나 강제 kill은 없습니다. 멈추지 않는 사용자 Shutdown/컴파일러 프로세스에 대해 강제 종료를 보장하지 않습니다. 프로세스 종료 코드는 그대로 전달하며 설정·입출력 오류는 2입니다. SDK의 음수 오류는 생성된 앱이 stderr로 보고하고 1로 반환합니다.

## 일반 CMake / VS Code 개발

프로젝트 폴더를 VS Code에서 열고 CMake Tools로 debug 또는 release configure preset을 선택합니다. SDK prefix는 로컬 환경 또는 IDE 설정에서 제공합니다. Configure/Build와 target 선택은 일반 CMake 기능입니다. F7 동작은 사용자의 키 설정에 따릅니다.

사용자 소유 파일:

- `include/<project>/<element>.hpp`, `src/<element>/<element>.cpp`
- 루트 `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`

관리 파일:

- `kcf.project.json`, `cmake/elements.cmake`
- `src/<element>/main.cpp`, `generated/runtime.hpp`, `generated/supervisor.cpp`
- `.kcf/ownership.json`: 관리 파일의 정확한 이전 내용과 사용자 파일 목록

CMake 확장은 사용자 소유 루트 CMakeLists에 작성합니다. 예를 들어 `include(cmake/elements.cmake)` 뒤에 `target_sources`, `target_link_libraries`, `set_target_properties`를 사용할 수 있습니다. 실행 파일 위치는 `$<TARGET_FILE:...>` 기반 `build/kcf-artifacts-<Config>.txt`에서 확인하므로 OUTPUT_NAME이나 출력 디렉터리를 바꿔도 동작합니다. `build/kcf-source.txt`로 다른 프로젝트의 빌드 폴더 혼용을 거부합니다. 새 Element 추가 후에는 다시 configure/build해야 합니다. SDK 및 CMake target 변경에 대한 최종 판단은 CMake가 합니다.

직접 실행도 가능합니다(Manager 불필요):

```bash
./build/ugv02_driver  # 기본 10Hz, 사용자 인자 없음
./build/ugv02_supervisor  # 갱신된 템플릿: 빌드 시 기록한 기본 경로
./build/ugv02_supervisor "$PWD/kcf.project.json" "$PWD/build/kcf-artifacts-Debug.txt"
```

manifest 설정을 적용한 직접 Element 실행에는 다음 bootstrap 인자를 사용합니다. `ELEMENT_UUID`는 manifest에서 확인합니다.

```bash
./build/ugv02_driver --kcf-project "$PWD/kcf.project.json" --kcf-element ELEMENT_UUID --port /dev/ttyUSB0
```

CLI/GUI는 이 인자와 manifest의 argv를 자동 구성합니다. 실제 Element 바이너리는 standalone과 supervised에서 같습니다. 설정만 바꾸면 재컴파일 없이 다음 실행에 적용됩니다.

## 보존, 충돌 및 복구

관리 파일에 수동 수정이 있거나 새 파일 경로가 이미 존재하면 적용 전체를 중단합니다. 사용자 C++ 파일은 재생성 대상에서 제외하며, 누락된 사용자 파일도 자동 복원하지 않습니다. 관리 파일 수정은 별도 복사본으로 보존한 뒤 원래 생성 내용과 직접 조정해야 합니다. manifest는 GUI/CLI set으로 수정하세요. 직접 수정한 manifest를 강제로 채택하는 명령은 없습니다.

각 변경은 먼저 검증·충돌 검사 후 `.kcf/transaction/new`에 준비하고 이전 파일을 `old`에 보존합니다. 파일별 rename으로 적용하고 예외 발생 시 이미 적용한 파일을 복원합니다. 프로젝트별 advisory flock은 협력하는 GUI/CLI의 변경·빌드·실행을 직렬화합니다. 실행 중 설정 변경도 거부하여 Supervisor 자식 모두 같은 설정을 읽게 합니다. 빌드 디렉터리 lock도 사용합니다. IDE나 외부 편집기는 이 lock에 참여하지 않으므로 동시에 편집/빌드하지 마세요.

**여러 파일의 교체 전체가 파일시스템 수준의 단일 원자적 연산은 아닙니다.** 전원 단절/SIGKILL 중간에 남은 `.kcf/transaction`은 다음 쓰기를 차단합니다. 자동 복구나 강제 덮어쓰기 대신 프로젝트 전체를 백업한 뒤 `journal.json`의 각 `path`와 `existed`를 확인합니다. `existed=true` 파일은 `old/<path>`를 원위치로 복원하고, `false` 파일은 이번 transaction이 새로 만든 내용인지 `new` 및 현 내용을 확인한 뒤 제거 여부를 판단합니다. 복구 확인 후 transaction 디렉터리를 별도 보관하고 `regenerate --dry-run`으로 검증하세요. 복구 도중 수정한 사용자 파일을 무조건 삭제해서는 안 됩니다. fsync를 통한 전원 손실 내구성은 보장하지 않습니다.

`.kcf/ownership.json`은 버전 관리에 포함해야 안전한 재생성을 다른 환경에서도 사용할 수 있습니다. lock 파일과 transaction은 제외합니다. 관리 경로의 symlink 및 `..` 경로를 거부합니다. Git 없이도 보존 정책이 작동합니다.

## 첫 실사용 UX 확장: 기존 프로젝트 갱신

이 확장도 **schema v1**을 사용합니다. 기존 manifest에 `build`가 없으면 빈 빌드 설정으로 읽습니다. Open/validate/tree는 파일을 변경하지 않습니다. 기존 프로젝트에서 새 Supervisor 기본 실행 기능을 사용하려면 먼저 생성 관리 파일을 갱신하고 native CMake로 재빌드하세요.

```bash
kcf-project regenerate /path/to/test1 --dry-run
kcf-project regenerate /path/to/test1
cmake -S /path/to/test1 -B /path/to/test1/build -DCMAKE_BUILD_TYPE=Debug
cmake --build /path/to/test1/build
cd /path/to/test1
./build/test1_supervisor
```

GUI에서는 프로젝트를 연 뒤 **Preview / Update Generated Files → Apply**를 사용합니다. UUID와 기존 manifest 필드는 유지되며 사용자 C++와 루트 CMakeLists.txt는 덮어쓰지 않습니다. 관리 파일에 수동 수정이 있으면 기존처럼 충돌로 중단합니다. 변경 목록에서 확인 후 수동 조정해야 하며 강제 덮어쓰기는 제공하지 않습니다.

이전 Manager는 새 `build` 필드의 의미를 알지 못하므로 **새 빌드 설정을 사용한 프로젝트를 이전 Manager로 재생성하지 마세요.** 기존 v1 프로젝트를 현재 버전으로 여는 것은 지원하지만, 오래된 generator로의 downgrade는 지원하지 않습니다. 생성된 프로젝트 자체의 native CMake 빌드는 Manager 버전과 관계없이 가능합니다.

### Supervisor의 기본 경로

`./build/test1_supervisor`처럼 인자 없이 실행하면 CMake가 해당 Supervisor를 빌드할 때 기록한 다음 두 경로를 사용합니다.

- 해당 프로젝트의 `kcf.project.json`
- 해당 빌드 디렉터리·구성의 `kcf-artifacts-<Config>.txt`

`$<CONFIG>`를 CMake가 평가하므로 Debug/Release를 파일 날짜나 이름 패턴으로 추측하지 않습니다. 현재 디렉터리, 부모 디렉터리, 다른 빌드 디렉터리를 검색하지 않습니다. 프로젝트 루트 밖에서 실행해도 같은 명확한 기본 경로를 사용합니다. 다른 구성의 metadata 파일이 있어도 기본 파일이 없으면 오류입니다.

명시적 경로 방식도 유지합니다.

```bash
./build/test1_supervisor "$PWD/kcf.project.json" "$PWD/build/kcf-artifacts-Debug.txt"
```

인자는 둘 다 지정하거나 모두 생략해야 합니다. Manifest의 프로젝트 ID가 컴파일된 프로젝트 ID와 일치해야 하고, artifacts의 Supervisor target이 현재 실행 중인 실제 바이너리를 가리켜야 합니다. 누락·잘못된 JSON·schema·다른 프로젝트 ID·중복 target·누락 실행 파일·다른 Supervisor를 가리키는 metadata는 실행 전에 오류로 처리합니다. 정상 config 해석 이후의 Bringup Setup/Run과 정지 정책은 바뀌지 않았습니다.

기본 경로는 **빌드 시점의 절대 경로**입니다. 프로젝트나 build 디렉터리를 옮기면 새 위치에서 다시 configure/build하세요. 이 기능은 설치/배포용 재배치 탐색기가 아닙니다. 재배치한 바이너리에 명시적 경로를 사용할 때도 metadata가 현재 실행 파일들을 올바르게 가리켜야 합니다.

### Element별 CMake Build 설정

GUI의 Element 선택 → **CMake Build** 탭에서 다음 항목을 편집합니다. 문자열 목록은 한 줄에 하나씩 입력하며, 패키지는 표에서 추가/삭제합니다. **Preview / Save Settings**로 생성 CMake 변경을 검토하고 적용합니다.

| 설정 | JSON key | 생성 CMake |
| --- | --- | --- |
| 추가 C++ 파일 | `sources` | `target_sources(... PRIVATE ...)` |
| Include 디렉터리 | `include_directories` | `target_include_directories(... PRIVATE ...)` |
| 필수 패키지 | `packages` | `find_package(... REQUIRED ...)` |
| 라이브러리 / CMake target | `link_libraries` | `target_link_libraries(... PRIVATE ...)` |
| 컴파일 정의 | `compile_definitions` | `target_compile_definitions(... PRIVATE ...)` |
| 컴파일 옵션 | `compile_options` | `target_compile_options(... PRIVATE ...)` |
| 링크 옵션 | `link_options` | `target_link_options(... PRIVATE ...)` |

CLI에서는 기존 `set` 명령의 설정 JSON에 선택적인 `build` 객체를 추가합니다.

```json
{
  "build": {
    "sources": ["src/support/device.cpp"],
    "include_directories": ["include", "third_party/device/include"],
    "packages": [
      {"name": "Threads", "mode": "module"},
      {"name": "ExampleDependency", "mode": "config", "version": "1.2", "components": []}
    ],
    "link_libraries": ["Threads::Threads", "ExampleDependency::external"],
    "compile_definitions": ["DEVICE_ENABLED=1"],
    "compile_options": ["-Wall", "-Wextra"],
    "link_options": ["-Wl,--as-needed"]
  }
}
```

`ExampleDependency`는 예시 패키지입니다. 실제 설치한 패키지 이름과 exported target으로 바꿉니다. 검색 위치는 기존 `CMAKE_PREFIX_PATH` 또는 GUI/CLI SDK prefix 입력으로 지정합니다(여러 prefix는 세미콜론으로 구분). 지원하는 패키지 필드는 `name`, 최소 `version`, `mode`(`auto`/`config`/`module`), `components` 배열입니다. 모든 패키지는 REQUIRED이며 EXACT/임의 find_package 옵션은 노출하지 않습니다. 프로젝트가 기본 제공하는 KCF/nlohmann_json은 중복 등록하지 않습니다.

```bash
kcf-project set /path/to/test1 driver /tmp/build-settings.json --dry-run
kcf-project set /path/to/test1 driver /tmp/build-settings.json
# Manager를 실행하지 않는 VS Code CMake Tools와 같은 native build 경로
cmake --build /path/to/test1/build
```

build 하위 필드는 **부분 갱신**입니다. 생략한 목록은 유지하고, `[]`를 지정하면 해당 목록만 비웁니다. 예를 들어 `{"build":{"compile_options":[]}}`는 source/package/link 설정을 보존합니다. 목록에서 source를 제외해도 실제 파일을 삭제하지 않습니다.

경로는 프로젝트 루트 기준 상대 경로 또는 절대 경로입니다. `link_libraries`에서 `/`가 들어간 값은 파일 경로로 처리하고, `m`, `Threads::Threads` 같은 값은 CMake의 라이브러리/target 이름으로 전달합니다. 추가 파일은 사용자 소유이며 생성기가 작성하거나 삭제하지 않습니다. 파일/패키지/target의 실제 존재 및 컴파일 가능 여부는 CMake가 검사합니다.

문자열은 CMake 코드가 아닌 **리터럴 인자**입니다. 공백과 따옴표는 보존합니다. 리스트 구분자 `;`, 변수/생성기 표현식 `$`, backslash 및 줄바꿈, 구조를 바꾸는 CMake 예약 키워드는 거부합니다. 컴파일 정의는 `NAME` 또는 `NAME=value`이며 `-D`를 붙이지 않습니다. 모든 target 설정은 PRIVATE입니다. 동일 패키지를 여러 Element에서 요청하면 같은 요청은 한 번만 생성하고 다른 version/mode/components 요청은 충돌로 거부합니다.

복잡한 generator expression, 조건부·공개 scope, custom commands, 패키지별 특수 옵션은 사용자 소유 루트 CMakeLists.txt에서 계속 편집합니다. 이 설정 UI는 임의 CMake 언어 편집기가 아닙니다. 모든 지원 설정은 `cmake/elements.cmake` 관리 영역에 반영되며, native CMake/VS Code 빌드 시 Manager나 별도 재생성 hook이 필요하지 않습니다.

## Custom Module 등록 — 2026-11-28

CMake Build 탭의 입력 칸에는 항목별 실제 예제를 표시합니다. 마우스를 올리면 용도와 경로 기준을 볼 수 있습니다. 여러 값은 기존처럼 한 줄에 하나씩 입력합니다.

| 항목 | Placeholder 예제 |
| --- | --- |
| Additional C++ sources | `ex) test_module/module1/src/module1.cpp` |
| Include directories | `ex) test_module/module1/include` |
| Link libraries / CMake targets | `ex) Eigen3::Eigen` |
| Compile definitions | `ex) USE_IMU=1` |
| Compile options | `ex) -Wall` |
| Link options | `ex) -pthread` |

GUI에서 Element를 선택하고 **CMake Build → Add Custom Module**을 누릅니다.

1. Module directory를 입력하거나 **Browse directory…**로 선택합니다.
2. Source directory를 지정합니다. 이 값은 파일 선택·검증 범위이며, 폴더의 파일을 자동 등록하지 않습니다.
3. Header include directories를 직접 입력하거나 **Browse include directory…**로 추가합니다.
4. **Browse .cpp files…**로 빌드할 실제 파일을 여러 개 선택하거나 한 줄에 하나씩 입력합니다. 선택한 파일만 `target_sources()`에 등록됩니다.
5. **Validate / Preview**에서 정규화된 실제 source/include 목록을 확인합니다. 잘못된 경로와 중복은 대화상자 안에 오류를 표시하며 추가 버튼을 활성화하지 않습니다.
6. **Add to build settings**는 메인 창의 초안 목록에만 병합합니다. 아직 파일을 쓰지 않습니다. 취소하면 초안을 바꾸지 않습니다.
7. 메인 창의 **Preview / Save Settings → Apply**로 기존 CMake 변경 미리보기를 확인하고 적용합니다. 파일이 등록 이후 이동/삭제되면 CMake 빌드 단계에서 오류가 발생하므로 설정도 함께 갱신하세요.

예시 모듈 구조:

```text
test_module/module1/
├── include/module1.hpp
└── src/
    ├── module1.cpp       # 명시적으로 선택
    └── experimental.cpp # 선택하지 않으면 빌드하지 않음
```

모든 상대경로는 **프로젝트 루트 기준**입니다. Module/Source directory를 입력했더라도 후속 파일 경로의 기준이 그 디렉터리로 바뀌지 않습니다. 프로젝트 내부를 Browse로 선택하면 상대경로로 표시하고, 외부를 선택하면 실제 절대경로로 표시합니다. 직접 입력한 절대경로도 지원하며 절대경로 선택은 저장 시에도 절대경로로 유지합니다.

경로 검증 정책:

- Module directory, Source directory 및 include 디렉터리는 실제로 존재해야 합니다. 적어도 하나의 존재하는 일반 `.cpp` 파일을 명시적으로 선택해야 합니다.
- Source directory와 include 디렉터리는 선택한 Module directory 안에 있어야 하고, source 파일은 선택한 Source directory 안에 있어야 합니다.
- 외부 모듈은 **명시적인 절대경로**를 사용합니다. `../outside` 또는 내부 symlink를 이용한 상대경로의 프로젝트 밖 접근은 거부합니다. 외부 symlink를 Browse로 선택하면 실제 외부 절대경로로 표시됩니다.
- 같은 선택 목록의 중복과 기존 source/include 등록과의 중복을 거부합니다. `.`/`..` 정규화, 절대·상대 표기 차이, symlink 및 동일 파일 hardlink도 비교합니다. 생성 target에 이미 포함된 Element 구현/main.cpp를 다시 추가하는 것도 거부합니다.
- 폴더 검색, recursive glob, 파일 복사, 사용자 코드 수정·삭제를 수행하지 않습니다. 선택하지 않은 `.cpp`는 나중에 폴더에 추가되어도 자동 빌드되지 않습니다.

CLI도 동일한 공통 라이브러리 검증을 사용합니다. `/tmp/module.json` 예시:

```json
{
  "module_directory": "test_module/module1",
  "source_directory": "test_module/module1/src",
  "include_directories": ["test_module/module1/include"],
  "sources": ["test_module/module1/src/module1.cpp"]
}
```

```bash
kcf-project add-module /path/to/project driver /tmp/module.json --dry-run
kcf-project add-module /path/to/project driver /tmp/module.json
cmake --build /path/to/project/build
```

외부 모듈이라면 위 네 항목의 경로를 `/absolute/path/module1/...`로 명시합니다. CLI의 apply는 lock을 잡고 최신 Element 설정에 병합합니다. `--dry-run`은 실제 파일을 바꾸지 않습니다. 같은 모듈을 두 번 등록하면 중복 오류를 반환하므로 자동으로 덮어쓰거나 조용히 무시하지 않습니다. 동일 include 디렉터리가 이미 등록된 모듈에 source만 추가하려면 `include_directories: []`로 지정할 수 있습니다.

Manifest에는 새 스키마나 module 엔터티를 추가하지 않습니다. 결과는 기존 schema v1의 `build.sources`와 `build.include_directories` 목록으로 저장하며, 기존 package/link/options 설정은 유지합니다. GUI에서 초안 병합 후 저장하거나 CLI로 등록한 결과는 모두 표준 관리 CMake에 반영됩니다. 일반 `set`/CMake Build 텍스트 칸도 계속 사용할 수 있으며, 존재·모듈 경계 검증은 이 Custom Module 등록 흐름에서 수행합니다. 고급 generated source나 복잡한 CMake 서브프로젝트는 기존 사용자 소유 루트 CMakeLists.txt에서 직접 관리하세요.
