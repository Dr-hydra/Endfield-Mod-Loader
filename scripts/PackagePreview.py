from pathlib import Path
import argparse
import hashlib
import json
import shutil
import re
import subprocess
import zipfile

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--version',default='0.1.1-dev')
args=parser.parse_args()
if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9_.-]+)?',args.version):raise ValueError('Invalid release version')
output=root/'releases'/args.version;output.mkdir(parents=True,exist_ok=True)
package=root/'build/package/windows'
for name in ('EModLoader.exe','runtime/EModLoader.Host.dll','runtime/EModLoader.Launcher.exe','runtime/EModLoader.OfflineHost.exe','runtime/EModLoader.XInputProxy.dll'):
    assert (package/name).is_file(),name
archive=output/f'EModLoader-{args.version}-win-x64.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for file in sorted(package.rglob('*')):
        if file.is_file():zip.write(file,'EModLoader/'+file.relative_to(package).as_posix())
apk=root/'build/android/app/outputs/apk/debug/app-debug.apk'
assert apk.is_file()
android=output/'EModLoader-0.1.0-dev-android-arm64.apk';shutil.copyfile(apk,android)
sdk=output/'EModLoader-SDK-0.1.0.zip'
with zipfile.ZipFile(sdk,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for folder in ('include','examples/echo','examples/first-person/native','examples/first-person/ui','examples/first-person/tests'):
        for file in sorted((root/folder).rglob('*')):
            if file.is_file():zip.write(file,file.relative_to(root).as_posix())
    for name in ('README.md','LICENSE','THIRD_PARTY_NOTICES.md','docs/SDK.md','docs/COMPATIBILITY.md','docs/DEVELOPMENT.md','docs/USER_GUIDE.md',
                 'examples/first-person/README.md','examples/first-person/CMakeLists.txt','examples/first-person/module.json',
                 'scripts/BuildFirstPerson.ps1','scripts/PackageFirstPerson.py','native/shared/third_party/nlohmann/json.hpp'):
        zip.write(root/name,name)
files=[archive,android,sdk]
first_person=root/'releases/first-person/0.1.0-dev/example.first-person-0.1.0-dev.zip'
if first_person.is_file():
    module=output/first_person.name;shutil.copyfile(first_person,module);files.append(module)
records=[{'file':file.name,'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()} for file in files]
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
(output/'ASSETS.json').write_text(json.dumps({'version':args.version,'source_commit':commit,'game_validation':'not_run','android_device_validation':'not_run','interactive_uac_validation':'not_run',
    'first_person_module':'included' if first_person.is_file() else 'source_only','assets':records},indent=2)+'\n',encoding='utf-8')
(output/'SHA256SUMS.txt').write_text(''.join(f"{item['sha256']}  {item['file']}\n" for item in records),encoding='ascii')
print(json.dumps(records,indent=2))
