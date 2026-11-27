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
