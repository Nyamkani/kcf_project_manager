#!/usr/bin/env python3
"""Generator safety contract; no SDK or Qt required."""
import json
import pathlib
import subprocess
import sys
import tempfile
import hashlib
import fcntl

cli = str(pathlib.Path(sys.argv[1]).resolve())

def invoke(*args, ok=True):
    p = subprocess.run([cli, *map(str, args)], text=True, capture_output=True)
    assert (p.returncode == 0) == ok, (args, p.returncode, p.stdout, p.stderr)
    return p.stdout + p.stderr

def hashes(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and not p.name.endswith('.lock')}

with tempfile.TemporaryDirectory(prefix='kcf-pm-contract-') as tmp:
    base = pathlib.Path(tmp)
    root = base / 'space project'
    invoke('create', root, 'fixture', '--dry-run')
    assert not root.exists()
    invoke('create', root, 'fixture')
    invoke('add', root, 'driver')
    invoke('add', root, 'motion_controller')
    invoke('add', root, 'sensor_processor')
    before = hashes(root)
    invoke('create', root, 'fixture')
    invoke('add', root, 'driver')
    assert 'No changes' in invoke('regenerate', root)
    assert hashes(root) == before
    assert 'design-time' in invoke('tree', root)
    source = root / 'src/driver/driver.cpp'
    source.write_text(source.read_text() + '\n// user modification retained\n')
    contents = source.read_bytes()
    settings = base / 'settings.json'
    settings.write_text(json.dumps({'hz': 40, 'argv': ['space argument', '$(touch NO)', ';'],
                                    'env': {'PM_TEST': 'value'}, 'workdir': '.', 'supervised': True}))
    before = hashes(root)
    invoke('set', root, 'driver', settings, '--dry-run')
    assert before == hashes(root)
    invoke('set', root, 'driver', settings)
    invoke('regenerate', root)
    assert source.read_bytes() == contents
    for invalid in [{'hz': 0}, {'argv': 'bad'}, {'env': {'KCF_PROCESS_MODE': 'bad'}},
                    {'startup_timeout_ms': -1}, {'health_timeout_ms': 1.5}, {'unknown': 3}]:
        before = hashes(root)
        settings.write_text(json.dumps(invalid))
        invoke('set', root, 'driver', settings, ok=False)
        assert hashes(root) == before
    invoke('add', root, '../escape', ok=False)
    invoke('add', root, 'supervisor', ok=False)
    invoke('run', root, 'driver', ok=False)
    invoke('tree', root, '--dry-run', ok=False)
    invoke('build', root, '--unknown', 'x', ok=False)
    managed = root / 'cmake/elements.cmake'
    original = managed.read_text()
    managed.write_text(original + '# user edit\n')
    before = hashes(root)
    assert 'Modified managed file' in invoke('add', root, 'extra', ok=False)
    assert hashes(root) == before
    managed.write_text(original)
    collision = root / 'src/extra/extra.cpp'
    collision.parent.mkdir(parents=True)
    collision.write_text('precious user code')
    invoke('add', root, 'extra', ok=False)
    assert collision.read_text() == 'precious user code'
    collision.unlink()
    outside = base / 'outside.cpp'
    outside.write_text('external file')
    collision.symlink_to(outside)
    invoke('add', root, 'extra', ok=False)
    assert outside.read_text() == 'external file'
    collision.unlink()
    with (root / '.kcf-project.lock').open('r+') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        assert 'busy' in invoke('add', root, 'extra', ok=False)
    # Inject a real mid-apply filesystem failure after new user files were staged.
    # Existing managed files and newly created files must roll back together.
    before = hashes(root)
    (root/'cmake').chmod(0o555)
    try:
        invoke('add', root, 'rollback_probe', ok=False)
    finally:
        (root/'cmake').chmod(0o755)
    assert hashes(root) == before
    assert not (root/'.kcf/transaction').exists()
    manifest = root / 'kcf.project.json'
    saved = manifest.read_text()
    model = json.loads(saved)
    model['schema'] = 9
    manifest.write_text(json.dumps(model))
    invoke('validate', root, ok=False)
    model['schema'] = 1
    model['elements'][1]['id'] = model['elements'][0]['id']
    manifest.write_text(json.dumps(model))
    invoke('validate', root, ok=False)
    manifest.write_text(saved)
    # Optional v1 build settings: typed validation, merge semantics, safe literals.
    original_root = (root/'CMakeLists.txt').read_bytes()
    build_settings = {
        'sources': ['extra source.cpp'], 'include_directories': ['include dir'],
        'packages': [{'name': 'Threads', 'mode': 'module'}],
        'link_libraries': ['Threads::Threads'], 'compile_definitions': ['MESSAGE="hello world"'],
        'compile_options': ['-Wall'], 'link_options': ['-Wl,--as-needed']}
    settings.write_text(json.dumps({'build': build_settings}))
    before = hashes(root)
    invoke('set', root, 'driver', settings, '--dry-run')
    assert hashes(root) == before
    invoke('set', root, 'driver', settings)
    assert (root/'CMakeLists.txt').read_bytes() == original_root
    assert source.read_bytes() == contents
    cmake = (root/'cmake/elements.cmake').read_text()
    assert 'target_sources(fixture_driver PRIVATE' in cmake
    assert 'find_package([[Threads]] MODULE REQUIRED)' in cmake
    assert 'target_link_options(fixture_driver PRIVATE' in cmake
    settings.write_text(json.dumps({'build': {'compile_options': []}}))
    invoke('set', root, 'driver', settings)
    configured = json.loads(manifest.read_text())['elements'][0]['build']
    assert configured['sources'] == build_settings['sources'] and configured['compile_options'] == []
    assert 'No changes' in invoke('regenerate', root)
    for bad in [None, [], {'sources': 'source.cpp'}, {'sources': [4]},
                {'sources': ['${CMAKE_SOURCE_DIR}/x.cpp']}, {'link_libraries': ['a;b']},
                {'compile_definitions': ['-DVALUE=1']}, {'compile_options': ['PRIVATE']},
                {'packages': ['Threads']}, {'packages': [{'name': 'Threads', 'typo': True}]},
                {'packages': [{'name': 'Threads', 'version': '1;2'}]},
                {'packages': [{'name': 'Threads', 'components': ['CONFIG']}]},
                {'packages': [{'name': 'Threads', 'components': ['NO_CMAKE_PATH']}]},
                {'packages': [{'name': 'Threads', 'components': ['OPTIONAL']}]},
                {'packages': [{'name': 'Threads', 'mode': 'invalid'}]}, {'typo': []}]:
        before = hashes(root)
        settings.write_text(json.dumps({'build': bad}))
        invoke('set', root, 'driver', settings, ok=False)
        assert hashes(root) == before
    settings.write_text(json.dumps({'build': {'packages': [{'name': 'Threads', 'mode': 'config'}]}}))
    before = hashes(root)
    assert 'conflicting find_package' in invoke('set', root, 'motion_controller', settings, ok=False)
    assert hashes(root) == before

    # Load a genuine old generator snapshot, including its exact ownership baseline.
    legacy = base/'legacy'
    fixture = json.loads((pathlib.Path(__file__).parent/'fixtures/schema1-project.json').read_text())
    for path, content in fixture['files'].items():
        dest = legacy/path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(content)
    user_cpp = legacy/'src/driver/driver.cpp'
    user_cpp.write_text(user_cpp.read_text() + '\n// legacy user edit\n')
    user_cmake = legacy/'CMakeLists.txt'
    user_cmake.write_text(user_cmake.read_text() + '\n# legacy CMake customization\n')
    old_user = (user_cpp.read_bytes(), user_cmake.read_bytes())
    old_manifest = (legacy/'kcf.project.json').read_bytes()
    before = hashes(legacy)
    invoke('validate', legacy)
    invoke('tree', legacy)
    invoke('regenerate', legacy, '--dry-run')
    assert hashes(legacy) == before
    invoke('regenerate', legacy)
    assert (legacy/'kcf.project.json').read_bytes() == old_manifest
    assert (user_cpp.read_bytes(), user_cmake.read_bytes()) == old_user
    assert 'No changes' in invoke('regenerate', legacy)
    settings.write_text(json.dumps({'build': {'compile_definitions': ['MIGRATED=1']}}))
    invoke('set', legacy, 'driver', settings)
    assert json.loads((legacy/'kcf.project.json').read_text())['schema'] == 1
    assert (user_cpp.read_bytes(), user_cmake.read_bytes()) == old_user
    transaction = root / '.kcf/transaction'
    transaction.mkdir()
    assert 'Incomplete transaction' in invoke('regenerate', root, ok=False)
print('PASS: legacy v1 migration, build settings, creation, 3 Elements, idempotency, preview, preservation, validation, collisions, symlinks, locks, incomplete transaction')
