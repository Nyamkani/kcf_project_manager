"""Shared module path registration contract, no SDK or Qt required."""
import json
import pathlib
import subprocess
import sys
import tempfile
import hashlib
import os

cli = str(pathlib.Path(sys.argv[1]).resolve())
def invoke(*args, ok=True):
    result = subprocess.run([cli, *map(str, args)], capture_output=True, text=True)
    assert (result.returncode == 0) == ok, (args, result.stdout, result.stderr)
    return result.stdout + result.stderr

def snapshot(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file() and not p.name.endswith('.lock')}

with tempfile.TemporaryDirectory(prefix='kcf-module-') as tmp:
    base = pathlib.Path(tmp)
    root = base/'project with spaces'
    invoke('create', root, 'modules')
    invoke('add', root, 'driver')
    module = root/'test_module/module1'
    (module/'src').mkdir(parents=True)
    (module/'include').mkdir()
    (module/'src/module1.cpp').write_text('int module1() { return 1; }\n')
    (module/'src/unselected.cpp').write_text('#error Must not be compiled\n')
    (module/'include/module1.hpp').write_text('#pragma once\nint module1();\n')
    (root/'CMakeLists.txt').write_text((root/'CMakeLists.txt').read_text()+'\n# user cmake\n')
    (root/'src/driver/driver.cpp').write_text((root/'src/driver/driver.cpp').read_text()+'\n// user source\n')
    preserved = {p:p.read_bytes() for p in [root/'CMakeLists.txt',root/'src/driver/driver.cpp',module/'src/module1.cpp']}
    request = {'module_directory':'test_module/module1', 'source_directory':'test_module/module1/src',
               'include_directories':['test_module/module1/include'], 'sources':['test_module/module1/src/module1.cpp']}
    file = base/'request.json'
    def register(value, ok=True, dry=False):
        file.write_text(json.dumps(value))
        return invoke('add-module', root, 'driver', file, *(['--dry-run'] if dry else []), ok=ok)
    before = snapshot(root)
    assert 'target_sources' in register(request, dry=True)
    assert snapshot(root) == before
    for patch in [{'sources':[]}, {'sources':['test_module/module1/src']},
                  {'sources':['test_module/module1/src/missing.cpp']},
                  {'sources':['test_module/module1/include/module1.hpp']},
                  {'include_directories':['missing']}, {'source_directory':'missing'},
                  {'module_directory':'missing'}, {'sources':request['sources']*2},
                  {'include_directories':request['include_directories']*2}, {'typo':True},
                  {'source_directory':'.'}, {'sources':['src/driver/driver.cpp']}]:
        register({**request, **patch}, ok=False)
        assert snapshot(root) == before
    implicit = {'module_directory':'.', 'source_directory':'src/driver', 'include_directories':[],
                'sources':['src/driver/driver.cpp']}
    assert 'generated target' in register(implicit, ok=False)
    register(request)
    model = json.loads((root/'kcf.project.json').read_text())
    assert model['schema'] == 1
    assert model['elements'][0]['build']['sources'] == request['sources']
    assert model['elements'][0]['build']['include_directories'] == request['include_directories']
    assert 'module_directory' not in model['elements'][0]
    assert 'unselected.cpp' not in (root/'cmake/elements.cmake').read_text()
    registered = snapshot(root)
    assert 'already registered' in register(request, ok=False)
    register({**request, 'include_directories':[], 'sources':[str(module/'src/./module1.cpp')]}, ok=False)
    (module/'src/alias.cpp').symlink_to(module/'src/module1.cpp')
    register({**request, 'include_directories':[], 'sources':['test_module/module1/src/alias.cpp']}, ok=False)
    (module/'src/alias.cpp').unlink()
    os.link(module/'src/module1.cpp', module/'src/hardlink.cpp')
    register({**request, 'include_directories':[], 'sources':['test_module/module1/src/hardlink.cpp']}, ok=False)
    (module/'src/hardlink.cpp').unlink()
    assert snapshot(root) == registered
    external = base/'external module'
    (external/'src').mkdir(parents=True)
    (external/'include').mkdir()
    (external/'src/external.cpp').write_text('int external() { return 2; }\n')
    outside = {'module_directory':str(external), 'source_directory':str(external/'src'),
               'include_directories':[str(external/'include')], 'sources':[str(external/'src/external.cpp')]}
    assert 'explicitly absolute' in register({**outside,'module_directory':'../external module'}, ok=False)
    (root/'outside').symlink_to(external, target_is_directory=True)
    assert 'explicitly absolute' in register({**outside,'sources':['outside/src/external.cpp']}, ok=False)
    (root/'outside').unlink()
    register(outside)
    external_before = snapshot(external)
    for path, content in preserved.items(): assert path.read_bytes() == content
    model = json.loads((root/'kcf.project.json').read_text())
    assert model['elements'][0]['build']['sources'] == request['sources'] + outside['sources']
    assert 'No changes' in invoke('regenerate', root)
    assert snapshot(external) == external_before
print('PASS: explicit module sources, internal relative/external absolute paths, preview, aliases/duplicates, missing/outside paths, v1 and user file preservation')
