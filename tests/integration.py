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

def active(args, markers, callback=None):
    readiness = list(markers)
    if 'launch' in args or str(args[0]).endswith('_supervisor'):
        readiness.append('-> RUNNING')
    with tempfile.TemporaryFile(mode='w+') as log:
        p = subprocess.Popen(list(map(str, args)), stdout=log, stderr=subprocess.STDOUT)
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
    command('cmake', '-S', root, '-B', root/'build', '-DCMAKE_BUILD_TYPE=Debug', f'-DCMAKE_PREFIX_PATH={sdk}')
    command('cmake', '--build', root/'build', '--parallel', '2')
    command('cmake', '--build', root/'build', '--target', 'integration_driver')
    print('PASS: native configure, full build, target build, custom executable metadata', flush=True)
    command(cli, 'build', root, '--prefix', sdk)
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
    command(cli, 'build', root, '--config', 'Release', '--build-dir', 'build-release', '--prefix', sdk)
    active([cli, 'run', root, 'driver', '--config', 'Release', '--build-dir', 'build-release'], ['READY driver'])
    assert not (root/'work/SHOULD_NOT_EXIST').exists()
    settings.write_text(json.dumps({'workdir': 'missing'}))
    command(cli, 'set', root, 'driver', settings)
    assert 'Working directory' in command(cli, 'run', root, 'driver', ok=False)
    print('PASS: native direct Supervisor, external source edit/rebuild, source preservation, runtime-only setting update, Release, invalid workdir', flush=True)
