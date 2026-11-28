import os
import pathlib
import subprocess
import sys
import tempfile
import json
with tempfile.TemporaryDirectory(prefix='kcf-pm-qt-') as tmp:
    root = pathlib.Path(tmp)/'gui'
    p = subprocess.run([sys.argv[1], '--self-test', str(root), sys.argv[2]],
                       env={**os.environ, 'QT_QPA_PLATFORM': 'offscreen'}, timeout=170)
    log = (root/'gui-test.log').read_text() if (root/'gui-test.log').exists() else 'No GUI log'
    print(log)
    assert p.returncode == 0, p.returncode
    assert 'GUI self-test PASS' in log
    model = json.loads((root/'kcf.project.json').read_text())
    assert model['elements'][0]['hz'] == 25
    assert model['elements'][0]['argv'] == ['gui argument']
    build = model['elements'][0]['build']
    assert build['sources'] == ['extras/gui.cpp']
    assert build['include_directories'] == ['extras']
    assert build['link_libraries'] == ['Threads::Threads']
    assert build['compile_definitions'] == ['GUI_BUILD_SETTING=7']
    assert build['compile_options'] == ['-Wall']
    assert build['link_options'] == ['-Wl,--as-needed']
    assert build['packages'][0]['name'] == 'Threads'
