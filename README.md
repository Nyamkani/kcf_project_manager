# KCF Project Manager

**v0.1.0 MVP · Linux / C++17 / CMake / Qt6**

독립 Qt GUI와 headless CLI로 KCF 프로젝트를 생성하고, Element 실행 설정과 CMake 빌드·실행을 관리합니다. 두 UI는 Qt에 의존하지 않는 `kcf_project` 라이브러리를 사용합니다. 사용자 C++ 편집은 VS Code 등 기존 IDE에서 합니다.

## 빌드

필수: CMake 3.20+, C++17 컴파일러, Threads, nlohmann-json 3.2+. GUI는 Qt6 Widgets가 필요합니다. Ubuntu 패키지 이름은 `cmake g++ nlohmann-json3-dev qt6-base-dev`입니다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/kcf-project-manager
# GUI 없는 Linux: configure에 -DKCF_PM_GUI=OFF 추가
```

Manager 자체의 빌드·생성·구조 조회에는 KCF SDK가 필요하지 않습니다. **생성된 애플리케이션**의 빌드에는 `KCF::core`와 `KCF::bringup`을 제공하는 설치된 공개 SDK 및 nlohmann-json이 필요합니다. Core 버전을 EXACT로 고정하지 않습니다.

## 첫 프로젝트

```bash
./build/kcf-project create /tmp/ugv02 ugv02
./build/kcf-project add /tmp/ugv02 driver
./build/kcf-project add /tmp/ugv02 motion_controller
./build/kcf-project add /tmp/ugv02 sensor_processor
./build/kcf-project tree /tmp/ugv02
./build/kcf-project build /tmp/ugv02 --prefix /path/to/installed/kcf-sdk
./build/kcf-project build /tmp/ugv02 --target ugv02_driver
./build/kcf-project run /tmp/ugv02 driver
# Ctrl+C로 위 실행을 정지한 다음:
./build/kcf-project launch /tmp/ugv02
```

`kcf`는 기존 Core 진단 도구 이름입니다. 이 저장소는 `kcf-project`와 `kcf-project-manager`만 제공합니다. Host Gateway를 생성하거나 시작·종료하지 않습니다.

생성 이후에는 Manager 없이 일반 CMake 또는 VS Code CMake Tools로 빌드할 수 있습니다.

```bash
cmake -S /tmp/ugv02 -B /tmp/ugv02/build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/path/to/installed/kcf-sdk
cmake --build /tmp/ugv02/build -j2
cmake --build /tmp/ugv02/build --target ugv02_driver
```

`src/driver/driver.cpp`와 `include/ugv02/driver.hpp`에서 구현합니다. 런타임 및 Element별 source/include/package/link/compile 설정은 GUI의 Runtime / CMake Build 탭 또는 CLI `set`으로 변경합니다. 생성 관리 파일에 수동 수정이 있으면 덮어쓰지 않고 중단합니다.

## 검증과 문서

실제 SDK 통합 및 Qt offscreen 테스트를 포함하려면:

```bash
cmake -S . -B build -DKCF_PM_TEST_SDK=/path/to/installed/kcf-sdk
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

- [사용 설명서](docs/07_USER_GUIDE.md)
- [설계 결정 / 공개 계약](docs/06_OPEN_DECISIONS.md)
- [구현 상태와 실제 검증 결과](docs/08_IMPLEMENTATION_STATUS.md)
- 초기 요구사항: [기획 요약](docs/00_기획_요약_KO.md), [범위](docs/01_PROJECT_SCOPE.md), [아키텍처](docs/02_ARCHITECTURE_AND_COMPATIBILITY.md), [빌드/실행 계약](docs/03_BUILD_AND_LAUNCH_CONTRACT.md), [UGV02 사례](docs/04_UGV02_CASE_STUDY.md), [구현 계획](docs/05_CODEX_IMPLEMENTATION_PLAN.md).

현재 고급 rename/remove/import, 패키지 배포, 원격 실행은 포함하지 않습니다. VS Code 실제 UI 조작과 OPI5+/UGV02 장치 검증은 자동 CMake/Qt 테스트와 별도로 구분합니다.

생성된 Supervisor는 프로젝트 루트에서 `./build/ugv02_supervisor`로 직접 실행할 수 있습니다. 명시적 manifest/artifacts 인자도 지원합니다. 기존 프로젝트는 `kcf-project regenerate PROJECT --dry-run`으로 검토한 뒤 `regenerate`와 native CMake 재빌드를 수행하세요. [기존 v1 프로젝트 갱신과 빌드 설정](docs/07_USER_GUIDE.md#첫-실사용-ux-확장-기존-프로젝트-갱신)을 참조하세요.

사용자 C++ 모듈은 GUI **CMake Build → Add Custom Module** 또는 CLI `add-module`로 등록합니다. 폴더를 자동 스캔하지 않고 선택한 실제 `.cpp`와 include 경로만 연결합니다. [Custom Module 사용법과 경로 정책](docs/07_USER_GUIDE.md#custom-module-등록--2026-11-28)을 참조하세요.
