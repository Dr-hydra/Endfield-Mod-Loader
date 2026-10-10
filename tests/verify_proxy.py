"""Validate installation, ownership and DLL startup only in an owned fixture."""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import socket
import subprocess
import tempfile
import time
import urllib.request

parser=argparse.ArgumentParser()
parser.add_argument('--native-bin',type=Path,required=True)
args=parser.parse_args()
binary=args.native_bin.resolve()
workspace=Path(__file__).resolve().parents[1]
test_directory=workspace/'build/tests';test_directory.mkdir(parents=True,exist_ok=True)
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.CreateEventW.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_wchar_p];kernel.CreateEventW.restype=ctypes.c_void_p
kernel.SetEvent.argtypes=[ctypes.c_void_p];kernel.CloseHandle.argtypes=[ctypes.c_void_p]
checks=0
def check(value,message):
    global checks
    if not value:raise AssertionError(message)
    checks+=1
with tempfile.TemporaryDirectory(prefix='eml-proxy-',dir=test_directory) as temporary:
    root=Path(temporary).resolve()
    assert root.is_relative_to(test_directory.resolve()) # Recursive cleanup is confined to this fixture.
    folder=root/'游戏 with spaces';folder.mkdir()
    game=folder/'Endfield.exe';game.write_bytes((binary/'EMLProxyControlledTarget.exe').read_bytes())
    with socket.socket() as listener:listener.bind(('127.0.0.1',0));port=listener.getsockname()[1]
    token='a'*64
    index=root/'index.json';index.write_text(json.dumps({'schema':1,'port':port,'token':token,'modules':[]}))
    host=binary/'EModLoader.Host.dll';proxy=binary/'EModLoader.XInputProxy.dll'
    launcher=binary/'EModLoader.Launcher.exe'
    def deploy(install=True):
        return subprocess.run([str(launcher),'--game',str(game),'--index',str(index),'--host',str(host),
            '--proxy',str(proxy),'--operation','install-proxy' if install else 'remove-proxy'],capture_output=True,text=True,timeout=20)
    result=deploy();check(result.returncode==0,'Proxy install failed: '+result.stderr)
    installed=folder/'xinput1_4.dll';receipt=folder/'eml-xinput.json'
    check(hashlib.sha256(installed.read_bytes()).digest()==hashlib.sha256(proxy.read_bytes()).digest(),'Installed proxy bytes changed')
    record=json.loads(receipt.read_text())
    check(record['proxy_sha256']==hashlib.sha256(proxy.read_bytes()).hexdigest() and record['index_path']==str(index),'Receipt identity/path differs')
    check(not (folder/'eml-xinput.lock').exists() and not list(folder.glob('*.pending')),'Deployment left temporary files')
    def start(through_launcher):
        name='Local\\EML.Proxy.Stop.'+str(os.getpid())+'.'+str(through_launcher)
        event=kernel.CreateEventW(None,True,False,name);assert event
        env=os.environ.copy();env['EML_TEST_STOP']=name
        process=None;pid=None
        try:
            if through_launcher:
                started=subprocess.run([str(launcher),'--game',str(game),'--index',str(index),'--host',str(host)],env=env,capture_output=True,text=True,timeout=30)
                check(started.returncode==0,'Owned proxy blocked EML startup: '+started.stderr)
                pid=int(started.stdout.strip())
            else:
                process=subprocess.Popen([str(game)],env=env,creationflags=subprocess.CREATE_NO_WINDOW);pid=process.pid
            connected=False
            for _ in range(70):
                try:
                    request=urllib.request.Request(f'http://127.0.0.1:{port}/status',headers={'Authorization':'Bearer '+token})
                    with urllib.request.urlopen(request,timeout=1) as response:status=json.load(response)
                    connected=status['mode']=='game' and status['host_product']=='Endfield Mod Loader'
                except (OSError,ValueError):pass
                if connected:break
                if process and process.poll() is not None:raise AssertionError('Proxy fixture exited: '+str(process.returncode))
                time.sleep(0.1)
            check(connected,'Proxy DLL did not load/authenticate the game Host')
            blocked=deploy(False)
            check(blocked.returncode!=0 and installed.exists(),'Uninstaller changed a running fixture')
        finally:
            kernel.SetEvent(event)
            if process:process.wait(timeout=10)
            elif pid is not None:
                kernel.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];kernel.OpenProcess.restype=ctypes.c_void_p
                handle=kernel.OpenProcess(0x100000,False,pid)
                if handle:
                    kernel.WaitForSingleObject.argtypes=[ctypes.c_void_p,ctypes.c_uint32]
                    kernel.WaitForSingleObject(handle,10000);kernel.CloseHandle(handle)
            kernel.CloseHandle(event)
    start(False);start(True)
    check(deploy().returncode==0,'Owned proxy update was rejected')
    installed.write_bytes(b'foreign or modified proxy');before=installed.read_bytes()
    check(deploy().returncode!=0 and installed.read_bytes()==before,'Installer overwrote modified proxy')
    check(deploy(False).returncode!=0 and installed.read_bytes()==before,'Uninstaller deleted modified proxy')
    installed.write_bytes(proxy.read_bytes())
    check(deploy(False).returncode==0 and not installed.exists() and not receipt.exists(),'Owned proxy did not uninstall cleanly')
    installed.write_bytes(b'foreign without receipt')
    check(deploy().returncode!=0 and installed.read_bytes()==b'foreign without receipt','Installer overwrote a foreign proxy')
    check(deploy(False).returncode!=0 and installed.exists(),'Uninstaller removed a foreign proxy')
    installed.unlink();receipt.write_text('{"product":"another tool","schema":1}')
    check(deploy().returncode!=0 and not installed.exists(),'Installer overwrote foreign configuration')
    print(f'PASS: {checks} proxy deployment, ownership, system XInput and controlled DLL startup checks. No real game or interactive UAC was used.')
