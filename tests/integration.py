#!/usr/bin/env python3
"""Real installed-SDK integration. No mocks; no Gateway is started."""
import hashlib
import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

cli = str(pathlib.Path(sys.argv[1]).resolve())
sdk = str(pathlib.Path(sys.argv[2]).resolve())

def command(*args, ok=True):
    p = subprocess.run(list(map(str, args)), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (p.returncode == 0) != ok:
        raise AssertionError((args, p.returncode, p.stdout))
    return p.stdout

def active(args, markers, callback=None, cwd=None):
    readiness = list(markers)
    if 'launch' in args or str(args[0]).endswith('_supervisor'):
        readiness.append('-> RUNNING')
    with tempfile.TemporaryFile(mode='w+') as log:
        p = subprocess.Popen(list(map(str, args)), stdout=log, stderr=subprocess.STDOUT, cwd=cwd)
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                log.seek(0)
                text = log.read()
                if all(marker in text for marker in readiness):
                    break
                if p.poll() is not None:
                    raise AssertionError(('Exited before ready', p.returncode, text))
                time.sleep(.05)
            else:
                raise AssertionError(('Readiness timeout', text))
            if callback:
                callback(p, text)
            p.send_signal(signal.SIGTERM)
            rc = p.wait(timeout=15)
            log.seek(0)
            text = log.read()
            assert rc == 0, (rc, text)
            for marker in markers:
                assert marker.replace('READY', 'STOP') in text, text
            return text
        finally:
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()

with tempfile.TemporaryDirectory(prefix='kcf-pm-sdk-') as tmp:
    root = pathlib.Path(tmp) / 'space fixture'
    command(cli, 'create', root, 'integration')
    names = ['driver', 'motion_controller', 'sensor_processor']
    for name in names:
        command(cli, 'add', root, name)
        source = root / f'src/{name}/{name}.cpp'
        source.write_text(f'''#include "integration/{name}.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
Element_{name}::Element_{name}(int argc, char** argv) {{
    if (argc != 2 || std::string(argv[0]) != "space argument" || std::string(argv[1]) != "$(touch SHOULD_NOT_EXIST)") throw std::runtime_error("argv mismatch");
}}
int Element_{name}::Setup() {{
    const char* value = std::getenv("PM_TEST_VALUE");
    if (!value || std::string(value) != "expected" || std::filesystem::current_path().filename() != "work") return -1;
    std::cout << "READY {name} pid=" << getpid() << std::endl; return 0;
}}
int Element_{name}::Loop() {{ return 0; }}
void Element_{name}::Shutdown() {{ std::cout << "STOP {name}" << std::endl; }}
''')
    (root / 'work').mkdir()
    settings = pathlib.Path(tmp) / 'settings.json'
    settings.write_text(json.dumps({'hz': 30, 'argv': ['space argument', '$(touch SHOULD_NOT_EXIST)'],
                                    'env': {'PM_TEST_VALUE': 'expected'}, 'workdir': 'work'}))
    hashes = {n: hashlib.sha256((root / f'src/{n}/{n}.cpp').read_bytes()).hexdigest() for n in names}
    for name in names:
        command(cli, 'set', root, name, settings)
    # Native target properties must remain authoritative for executable location/name.
    with (root / 'CMakeLists.txt').open('a') as f:
        f.write('\nset_target_properties(integration_driver PROPERTIES OUTPUT_NAME custom_driver RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/custom bin")\n')
    # Build and install a real external CMake package, outside the generated app.
    dependency = pathlib.Path(tmp)/'external library'
    dependency.mkdir()
    (dependency/'external.hpp').write_text('#pragma once\nint external_value();\n')
    (dependency/'external.cpp').write_text('#include "external.hpp"\nint external_value() { return 42; }\n')
    (dependency/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(ExampleDependency LANGUAGES CXX)
add_library(external STATIC external.cpp)
target_include_directories(external PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)
install(TARGETS external EXPORT ExampleDependencyTargets ARCHIVE DESTINATION lib)
install(FILES external.hpp DESTINATION include)
install(EXPORT ExampleDependencyTargets NAMESPACE ExampleDependency:: DESTINATION lib/cmake/ExampleDependency)
install(FILES ExampleDependencyConfig.cmake DESTINATION lib/cmake/ExampleDependency)
''')
    (dependency/'ExampleDependencyConfig.cmake').write_text('include("${CMAKE_CURRENT_LIST_DIR}/ExampleDependencyTargets.cmake")\n')
    dependency_prefix = pathlib.Path(tmp)/'external prefix'
    command('cmake', '-S', dependency, '-B', dependency/'build', f'-DCMAKE_INSTALL_PREFIX={dependency_prefix}')
    command('cmake', '--build', dependency/'build')
    command('cmake', '--install', dependency/'build')
    prefix = f'{sdk};{dependency_prefix}'
    extras = root/'extra sources'
    extras.mkdir()
    (extras/'local.hpp').write_text('#pragma once\n#define LOCAL_VALUE 3\n')
    (extras/'probe.cpp').write_text('''#include <external.hpp>
#include <local.hpp>
#ifndef BUILD_VALUE
#error Missing compile definition
#endif
#ifndef OPTION_VALUE
#error Missing compile option
#endif
static_assert(sizeof(MESSAGE) > 1, "Missing quoted definition");
int build_probe() { return external_value() + LOCAL_VALUE + BUILD_VALUE + OPTION_VALUE; }
''')
    (extras/'unselected.cpp').write_text('#error Directory must not be globbed\n')
    module_request = pathlib.Path(tmp)/'module.json'
    module_request.write_text(json.dumps({'module_directory':'extra sources', 'source_directory':'extra sources',
                                         'include_directories':['extra sources'], 'sources':['extra sources/probe.cpp']}))
    command(cli, 'add-module', root, 'driver', module_request)
    # An explicitly selected absolute external module also builds without copying.
    external_module = pathlib.Path(tmp)/'custom external module'
    external_module.mkdir()
    (external_module/'external_module.cpp').write_text('int custom_external_module() { return 8; }\n')
    (external_module/'unselected.cpp').write_text('#error External directory must not be globbed\n')
    module_request.write_text(json.dumps({'module_directory':str(external_module), 'source_directory':str(external_module),
                                         'include_directories':[], 'sources':[str(external_module/'external_module.cpp')]}))
    command(cli, 'add-module', root, 'driver', module_request)
    driver_source = root/'src/driver/driver.cpp'
    driver_source.write_text(driver_source.read_text().replace('int Element_driver::Setup() {',
        'int build_probe();\nint custom_external_module();\nint Element_driver::Setup() {\n    if (custom_external_module() != 8 || build_probe() != 42 + 3 + BUILD_VALUE + 5) return -1;'))
    hashes['driver'] = hashlib.sha256(driver_source.read_bytes()).hexdigest()
    build_settings = {'packages': [{'name': 'ExampleDependency', 'mode': 'config'}],
                      'link_libraries': ['ExampleDependency::external'],
                      'compile_definitions': ['BUILD_VALUE=7', 'MESSAGE="hello world"'],
                      'compile_options': ['-DOPTION_VALUE=5', '-Wall'],
                      'link_options': ['-Wl,--no-undefined']}
    settings.write_text(json.dumps({'build': build_settings}))
    user_cmake = (root/'CMakeLists.txt').read_bytes()
    command(cli, 'set', root, 'driver', settings)
    assert (root/'CMakeLists.txt').read_bytes() == user_cmake
    command('cmake', '-S', root, '-B', root/'build', '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={prefix}')
    command('cmake', '--build', root/'build', '--parallel', '2')
    command('cmake', '--build', root/'build', '--target', 'integration_driver')
    print('PASS: native configure, full build, target build, custom executable metadata', flush=True)
    command(cli, 'build', root, '--prefix', prefix)
    command(cli, 'build', root, '--target', 'integration_driver')
    command(cli, 'build', root, '--target', 'not_a_target', ok=False)
    print('PASS: CLI full/selected build and unknown-target error', flush=True)
    def while_running(p, text):
        assert 'Project busy' in command(cli, 'run', root, 'driver', ok=False)
        import re
        pid = int(re.search(r'READY driver pid=(\d+)', text).group(1))
        assert pathlib.Path(f'/proc/{pid}/exe').resolve() == (root/'build/custom bin/custom_driver').resolve()
    active([cli, 'run', root, 'driver'], ['READY driver'], while_running)
    active([cli, 'launch', root], ['READY '+n for n in names], while_running)
    print('PASS: real ProcessRuntime, Supervisor, identical executable, argv/env/workdir, owned Stop, duplicate launch rejection', flush=True)
    # Direct SDK program invocation works without the Manager process.
    active([root/'build/integration_supervisor', root/'kcf.project.json', root/'build/kcf-artifacts-Debug.txt'], ['READY '+n for n in names])
    active([root/'build/integration_supervisor'], ['READY '+n for n in names], cwd=root)
    # Cwd is irrelevant; do not select a nearby manifest or another build config.
    active([root/'build/integration_supervisor'], ['READY '+n for n in names], cwd=dependency)
    supervisor = root/'build/integration_supervisor'
    manifest = root/'kcf.project.json'
    artifacts = root/'build/kcf-artifacts-Debug.txt'
    def invalid(*args):
        p = subprocess.run(list(map(str, [supervisor, *args])), text=True, capture_output=True, timeout=5, cwd=root)
        assert p.returncode == 2, (p.returncode, p.stdout, p.stderr)
        assert '[Bringup] started' not in p.stdout + p.stderr
        return p.stderr
    assert 'Cannot read' in invalid(root/'missing.json', artifacts)
    assert 'Cannot read CMake artifact' in invalid(manifest, root/'missing.txt')
    assert 'both paths or neither' in invalid(manifest)
    wrong = pathlib.Path(tmp)/'wrong.json'
    wrong.write_text('{broken json')
    assert 'Invalid JSON' in invalid(wrong, artifacts)
    model = json.loads(manifest.read_text())
    model['schema'] = 99
    wrong.write_text(json.dumps(model))
    assert 'Unsupported project schema' in invalid(wrong, artifacts)
    model['schema'] = 1
    model['id'] = '00000000-0000-0000-0000-000000000000'
    wrong.write_text(json.dumps(model))
    assert 'different project' in invalid(wrong, artifacts)
    wrong_artifacts = pathlib.Path(tmp)/'wrong.txt'
    wrong_artifacts.write_text(artifacts.read_text().replace(str(supervisor), str(root/'build/custom bin/custom_driver')))
    assert 'do not identify this Supervisor' in invalid(manifest, wrong_artifacts)
    wrong_artifacts.write_text(artifacts.read_text() + artifacts.read_text())
    assert 'Duplicate target' in invalid(manifest, wrong_artifacts)
    manifest.rename(root/'saved-manifest.json')
    try:
        assert 'Cannot read' in invalid()
    finally:
        (root/'saved-manifest.json').rename(manifest)
    artifacts.rename(root/'build/kcf-artifacts-Release.txt')
    try:
        assert 'Cannot read CMake artifact' in invalid()
    finally:
        (root/'build/kcf-artifacts-Release.txt').rename(artifacts)
    print('PASS: no-argument/explicit Supervisor, cwd-independent configured defaults, missing/malformed/wrong-project/wrong-artifacts errors without launch', flush=True)
    # A partial Manager build-settings update must survive a plain IDE-style build.
    settings.write_text(json.dumps({'build': {'compile_definitions': ['BUILD_VALUE=11', 'MESSAGE="updated value"']}}))
    command(cli, 'set', root, 'driver', settings)
    command('cmake', '--build', root/'build', '--target', 'integration_driver')
    assert (root/'CMakeLists.txt').read_bytes() == user_cmake
    for name in names:
        assert hashlib.sha256((root/f'src/{name}/{name}.cpp').read_bytes()).hexdigest() == hashes[name]
    active([cli, 'run', root, 'driver'], ['READY driver'])
    print('PASS: external installed package, extra sources/includes, definitions/options/link options, native rebuild after Manager settings, user CMake/C++ preservation', flush=True)
    source = root/'src/driver/driver.cpp'
    source.write_text(source.read_text()+'\n// edited by external IDE; no regeneration\n')
    command('cmake', '--build', root/'build', '--target', 'integration_driver')
    hashes['driver'] = hashlib.sha256(source.read_bytes()).hexdigest()
    settings.write_text(json.dumps({'hz': 50, 'health_timeout_ms': 3000}))
    command(cli, 'set', root, 'driver', settings)
    command(cli, 'regenerate', root)
    for name in names:
        assert hashlib.sha256((root/f'src/{name}/{name}.cpp').read_bytes()).hexdigest() == hashes[name]
    active([cli, 'run', root, 'driver'], ['READY driver'])
    command(cli, 'build', root, '--config', 'Release', '--build-dir', 'build-release', '--prefix', prefix)
    active([cli, 'run', root, 'driver', '--config', 'Release', '--build-dir', 'build-release'], ['READY driver'])
    active([root/'build-release/integration_supervisor'], ['READY '+n for n in names], cwd=root)
    assert not (root/'work/SHOULD_NOT_EXIST').exists()
    settings.write_text(json.dumps({'workdir': 'missing'}))
    command(cli, 'set', root, 'driver', settings)
    assert 'Working directory' in command(cli, 'run', root, 'driver', ok=False)
    print('PASS: native direct Supervisor, external source edit/rebuild, source preservation, runtime-only setting update, Release, invalid workdir', flush=True)
