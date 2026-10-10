"""Exercise untouched published Echo DLLs; no game process or module rebuild."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile


def digest(content):
    return hashlib.sha256(content).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--releases-root', type=Path, required=True)
    parser.add_argument('--host', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    receipt = []
    for version in ('3.5.1', '3.5.2', '3.5.3', '3.5.4'):
        archive_path = args.releases_root / version / 'BetterEndfield-Echo-1.0.0-Dual.zip'
        original = archive_path.read_bytes()
        with zipfile.ZipFile(archive_path) as archive:
            manifest = json.loads(archive.read('module.json'))
            assert manifest['format'] == 1 and manifest['abi'] == 1
            native = archive.read(manifest['libraries']['windows-x64'])
            assert b'BetterEndfield_GetThirdPartyModuleV1' in native
            assert b'BetterEndfieldNext_GetThirdPartyModuleV1' not in native
            android = archive.read(manifest['libraries']['android-arm64'])
            assert b'BetterEndfield_GetThirdPartyModuleV1' in android
            script = archive.read('ui/app.js')
            assert b'window.betterEndfield' in script
        library = output / version / 'example.echo.dll'
        library.parent.mkdir(parents=True, exist_ok=True)
        library.write_bytes(native)
        result = subprocess.run([sys.executable, str(root / 'native/tests/third_party_host_test.py'),
            '--host', str(args.host.resolve()), '--library', str(library)],
            text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(f'{version}: {result.stdout}\n{result.stderr}')
        assert library.read_bytes() == native
        assert archive_path.read_bytes() == original
        receipt.append({'release': version, 'archive_sha256': digest(original),
            'windows_dll_sha256': digest(native), 'android_so_sha256': digest(android),
            'windows_native_lifecycle_and_messages': 'passed',
            'android_execution': 'not_run', 'web_execution': 'not_run'})
        print(f'PASS {version}: published DLL loaded unchanged; config/messages/events/lifecycle regression passed.')
    (output / 'results.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
