from pathlib import Path
import argparse
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time
import urllib.request
import ctypes

parser=argparse.ArgumentParser()
parser.add_argument('--native-bin',type=Path,required=True)
args=parser.parse_args()
binary=args.native_bin.resolve()
controlled_launcher=binary/'EMLControlledLauncher.exe'
if not controlled_launcher.is_file():
    parser.error('Build the test-only EMLControlledLauncher CMake target before running controlled launch tests.')
with tempfile.TemporaryDirectory(prefix='eml-launch-') as temp:
    root=Path(temp)
    runtime=root/'controlled runtime';runtime.mkdir()
    # Only this disposable fixture uses the unsigned-parent test build. The
    # product launcher always discovers and verifies the official launcher.
    launcher=runtime/'EModLoader.Launcher.exe'
    shutil.copy2(controlled_launcher,launcher)
    with socket.socket() as listener:listener.bind(('127.0.0.1',0));port=listener.getsockname()[1]
    token='controlled_test_private_token_123456789'
    index=root/'index.json';index.write_text(json.dumps({'schema':1,'port':port,'token':token,'modules':[]}))
    event_name='Local\\EML.Test.Stop.'+str(os.getpid())
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateEventW.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_wchar_p];kernel.CreateEventW.restype=ctypes.c_void_p
    kernel.SetEvent.argtypes=[ctypes.c_void_p];kernel.CloseHandle.argtypes=[ctypes.c_void_p]
    event=kernel.CreateEventW(None,True,False,event_name)
    assert event
    env=os.environ.copy();env['EML_TEST_STOP']=event_name
    try:
        result=subprocess.run([str(launcher),'--game',str(binary/'EMLControlledTarget.exe'),'--index',str(index),'--host',str(binary/'EModLoader.Host.dll')],env=env,text=True,capture_output=True,timeout=45)
        assert result.returncode==0,(result.stdout,result.stderr)
        for attempt in range(50):
            try:
                request=urllib.request.Request(f'http://127.0.0.1:{port}/status',headers={'Authorization':'Bearer '+token})
                with urllib.request.urlopen(request,timeout=2) as response:status=json.load(response)
                break
            except Exception:
                if attempt==49:raise
                time.sleep(0.1)
        assert status['host_product']=='Endfield Mod Loader' and status['mode']=='game'
        print('PASS: test-only explicit launcher loaded the product Host into a controlled test process and authenticated handshake succeeded. No game was used.')
    finally:kernel.SetEvent(event);kernel.CloseHandle(event)
