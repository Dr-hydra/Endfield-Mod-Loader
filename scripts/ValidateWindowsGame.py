"""Collect authenticated EML evidence from an opt-in real Windows game run.

Uses an isolated module index, never installs a proxy or changes game files.
The game remains open for scene checks; close it normally after validation.
Evidence excludes the local Host authentication token. No hashes are computed.
"""
import argparse
import ctypes
from ctypes import wintypes
import datetime as dt
import json
import secrets
import shutil
import socket
import subprocess
import time
import urllib.request
import uuid
from pathlib import Path


class ElevatedProcess:
    """Use the normal Windows UAC prompt and retain only this launcher's handle."""
    def __init__(self, command):
        class ShellExecuteInfo(ctypes.Structure):
            _fields_ = [('cbSize', wintypes.DWORD), ('fMask', wintypes.ULONG),
                ('hwnd', wintypes.HWND), ('lpVerb', wintypes.LPCWSTR),
                ('lpFile', wintypes.LPCWSTR), ('lpParameters', wintypes.LPCWSTR),
                ('lpDirectory', wintypes.LPCWSTR), ('nShow', ctypes.c_int),
                ('hInstApp', wintypes.HINSTANCE), ('lpIDList', ctypes.c_void_p),
                ('lpClass', wintypes.LPCWSTR), ('hkeyClass', wintypes.HKEY),
                ('dwHotKey', wintypes.DWORD), ('hIcon', wintypes.HANDLE),
                ('hProcess', wintypes.HANDLE)]
        shell = ctypes.WinDLL('shell32', use_last_error=True)
        shell.ShellExecuteExW.argtypes = [ctypes.POINTER(ShellExecuteInfo)]
        shell.ShellExecuteExW.restype = wintypes.BOOL
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.GetProcessId.argtypes = [wintypes.HANDLE]
        self.kernel.GetProcessId.restype = wintypes.DWORD
        self.kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        info = ShellExecuteInfo()
        info.cbSize = ctypes.sizeof(info)
        info.fMask = 0x40  # SEE_MASK_NOCLOSEPROCESS
        info.lpVerb = 'runas'
        info.lpFile = command[0]
        info.lpParameters = subprocess.list2cmdline(command[1:])
        info.lpDirectory = str(Path(command[0]).parent)
        info.nShow = 0
        if not shell.ShellExecuteExW(ctypes.byref(info)):
            raise ctypes.WinError(ctypes.get_last_error())
        self.handle = info.hProcess
        self.pid = self.kernel.GetProcessId(self.handle)

    def poll(self):
        code = wintypes.DWORD()
        if not self.kernel.GetExitCodeProcess(self.handle, ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        return None if code.value == 259 else code.value

    def close(self):
        self.kernel.CloseHandle(self.handle)


def summarize(snapshots):
    def ready(sample):
        return (sample.get('host', {}).get('mode') == 'game'
            and sample.get('echo', {}).get('result') == 0
            and sample.get('echo', {}).get('body', {}).get('runtime_helper_ready') is True
            and sample.get('first_person', {}).get('result') == 0
            and sample.get('first_person', {}).get('body', {}).get('phase') == 'ready')
    last = snapshots[-1] if snapshots else {}
    ready_count = sum(ready(sample) for sample in snapshots)
    exit_code = last.get('launcher_exit_code')
    # A clean, observed game exit ends the connection by design. A prior ready
    # sample does not excuse an unexplained disconnect or a nonzero game exit.
    passed = ready_count > 0 and (ready(last) if exit_code is None else exit_code == 0)
    return dict(initialization_passed=passed, ready_samples=ready_count,
        observed_launcher_exit_code=exit_code, scene_effects_validated=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--launcher', type=Path)
    parser.add_argument('--host', type=Path)
    parser.add_argument('--game', type=Path)
    parser.add_argument('--official-parent', type=Path)
    parser.add_argument('--package', type=Path, action='append', default=[])
    parser.add_argument('--attach', action='store_true')
    parser.add_argument('--elevate', action='store_true', help='Request normal Windows UAC for the launcher.')
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--first-person-action', choices=['status', 'toggle', 'exit'], default='status')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    index_path = output / 'modules' / 'index.json'
    launcher = None
    if args.attach:
        index = json.loads(index_path.read_text(encoding='utf-8'))
    else:
        if not all((args.launcher, args.host, args.game, args.package)):
            parser.error('A new run requires --launcher, --host, --game and --package.')
        if index_path.exists():
            parser.error('Use a new output directory or --attach to the existing run.')
        for file in (args.launcher, args.host, args.game):
            if not file.is_file():
                parser.error(f'Missing file: {file}')
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            port = listener.getsockname()[1]
        index = dict(schema=1, port=port, token=secrets.token_hex(32), modules=[])
        for package in args.package:
            manifest = json.loads((package / 'module.json').read_text(encoding='utf-8'))
            generation = str(uuid.uuid4())
            directory = index_path.parent / 'packages' / generation
            shutil.copytree(package, directory)
            configuration = manifest.get('default_configuration', {}).copy()
            if manifest['id'] == 'example.first-person':
                configuration.update(active=False, diagnostics=True)
            index['modules'].append(dict(id=manifest['id'], generation=generation,
                directory=str(directory), enabled=True, configuration=configuration))
        index_path.write_text(json.dumps(index, ensure_ascii=False), encoding='utf-8')
        command = [str(args.launcher.resolve()), '--game', str(args.game.resolve()),
            '--host', str(args.host.resolve()), '--index', str(index_path)]
        if args.official_parent:
            command += ['--official-parent', str(args.official_parent.resolve())]
        if args.elevate:
            launcher = ElevatedProcess(command)
        else:
            with (output / 'launcher.log').open('w', encoding='utf-8') as log:
                launcher = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                    creationflags=subprocess.CREATE_NO_WINDOW)
        (output / 'run.json').write_text(json.dumps(dict(started=dt.datetime.now().isoformat(),
            launcher_pid=launcher.pid, game=str(args.game.resolve()), command=command),
            indent=2), encoding='utf-8')
        print(f'Launched isolated validation; launcher PID {launcher.pid}', flush=True)

    def request(path, body=None):
        headers = {'Authorization': 'Bearer ' + index['token']}
        data = None
        if body is not None:
            headers['Content-Type'] = 'application/json'
            data = json.dumps(body).encode()
        req = urllib.request.Request(f'http://127.0.0.1:{index["port"]}{path}', data=data, headers=headers)
        with urllib.request.urlopen(req, timeout=3) as response:
            return json.load(response)

    def message(module, body):
        request_id = str(uuid.uuid4())
        request('/send', dict(module_id=module, request_id=request_id, body=body))
        until = time.monotonic() + 5
        while time.monotonic() < until:
            events = request('/poll', dict(module_id=module))['messages']
            for event in events:
                if event.get('request_id') == request_id:
                    return event
            time.sleep(.1)
        raise TimeoutError(f'No reply from {module}')

    started = time.monotonic()
    snapshots = []
    last_key = None
    action_sent = False
    while time.monotonic() - started < args.seconds:
        sample = dict(time=dt.datetime.now().isoformat())
        try:
            sample['host'] = request('/status')
            for module in sample['host']['modules']:
                if not module['enabled']:
                    continue
                if module['id'] == 'example.echo':
                    sample['echo'] = message(module['id'], dict(validation='Windows game runtime'))
                elif module['id'] == 'example.first-person':
                    action = args.first_person_action if not action_sent else 'status'
                    sample['first_person'] = message(module['id'], dict(action=action))
                    action_sent = True
        except Exception as error:
            sample['connection_error'] = str(error)
        if launcher is not None:
            sample['launcher_exit_code'] = launcher.poll()
        snapshots.append(sample)
        (output / 'runtime-evidence.json').write_text(json.dumps(snapshots, ensure_ascii=False, indent=2), encoding='utf-8')
        summary = dict(host=sample.get('host', {}).get('mode'),
            helper=sample.get('echo', {}).get('body', {}).get('runtime_helper_ready'),
            first_person=sample.get('first_person', {}).get('body'), error=sample.get('connection_error'))
        key = json.dumps(summary, sort_keys=True)
        if key != last_key:
            print(json.dumps(summary, ensure_ascii=False), flush=True)
            last_key = key
        if launcher is not None and launcher.poll() is not None and 'host' not in sample:
            break
        time.sleep(2)
    stamp = dt.datetime.now().strftime('%Y%m%d-%H%M%S')
    shutil.copyfile(output / 'runtime-evidence.json', output / f'runtime-evidence-{stamp}.json')
    summary = summarize(snapshots)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print('PASS: Host, Echo, engine helpers and first-person initialization.' if summary['initialization_passed']
        else 'INCOMPLETE: inspect runtime-evidence.json and modules/host.log.', flush=True)
    print('Scene effects require target_valid, active and increasing patched_frames evidence.', flush=True)
    if isinstance(launcher, ElevatedProcess):
        launcher.close()
    return 0 if summary['initialization_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
