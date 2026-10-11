"""Native guard and deployment regression checks. Never runs an actual game."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--normal-bin', type=Path, required=True)
parser.add_argument('--compat-bin', type=Path, required=True)
args = parser.parse_args()
normal, compat = args.normal_bin.resolve(), args.compat_bin.resolve()
checks = 0
with tempfile.TemporaryDirectory(prefix='eml-native-xinput-') as temporary:
    root = Path(temporary)
    game = root / 'Endfield.exe'
    shutil.copy2(compat / 'EMLControlledTarget.exe', game)
    foreign = root / 'xinput1_4.dll'
    shutil.copy2(compat / 'EMLForeignXInput.dll', foreign)
    before = foreign.read_bytes()
    index = root / 'index.json'
    index.write_text(json.dumps({'schema': 1, 'port': 55379, 'token': 'a' * 64, 'modules': []}))
    for binary in (normal, compat):
        command = [str(binary / 'EModLoader.Launcher.exe'), '--game', str(game),
                   '--index', str(index), '--host', str(binary / 'EModLoader.Host.dll')]
        result = subprocess.run(command, capture_output=True, timeout=10)
        assert result.returncode == 3, 'Native default conflict protection failed'
        checks += 1
        for operation in ('install-proxy', 'remove-proxy'):
            result = subprocess.run(command + ['--operation', operation, '--proxy',
                                   str(binary / 'EModLoader.XInputProxy.dll')] +
                                   (['--allow-xinput-conflict', 'true'] if binary == compat else []),
                                   capture_output=True, timeout=10)
            assert result.returncode == 10, 'Foreign DLL deployment protection failed'
            assert foreign.read_bytes() == before, 'Foreign DLL was changed'
            assert not (root / 'eml-xinput.json').exists(), 'Unexpected EML proxy receipt'
            checks += 1
    command = [str(normal / 'EModLoader.Launcher.exe'), '--game', str(game),
               '--index', str(index), '--host', str(normal / 'EModLoader.Host.dll')]
    result = subprocess.run(command + ['--allow-xinput-conflict', 'true'], capture_output=True, timeout=10)
    assert result.returncode == 3, 'Normal native build accepted compatibility override'
    checks += 1
    command[0] = str(compat / 'EModLoader.Launcher.exe')
    result = subprocess.run(command + ['--allow-xinput-conflict', 'false'], capture_output=True, timeout=10)
    assert result.returncode == 3, 'Disabled compatibility override was ignored'
    checks += 1
    result = subprocess.run(command + ['--allow-xinput-conflict', 'invalid'], capture_output=True, timeout=10)
    assert result.returncode == 2, 'Invalid compatibility flag was accepted'
    checks += 1
print(f'PASS: {checks} native default/normal guards, foreign DLL install/uninstall protections and option validation checks.')
