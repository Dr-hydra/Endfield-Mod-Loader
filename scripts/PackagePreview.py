from pathlib import Path
import argparse
import hashlib
import json
import shutil
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--version',help='Release version; defaults to Directory.Build.props Version plus -dev.')
parser.add_argument('--package',type=Path,help='Windows distribution directory; defaults to build/package/windows-<version>.')
parser.add_argument('--output',type=Path,help='Destination directory; defaults to releases/<version>.')
parser.add_argument('--android-apk',type=Path,default=root/'build/android/app/outputs/apk/debug/app-debug.apk',help='Existing Android APK to copy unchanged.')
parser.add_argument('--first-person',type=Path,help='Existing module ZIP to copy unchanged; defaults to releases/first-person/0.1.0-dev/example.first-person-0.1.0-dev.zip if present.')
parser.add_argument('--validation-report',type=Path,help='JSON object with explicit game_validation, android_static_validation, android_device_validation and interactive_uac_validation results.')
parser.add_argument('--hash-assets',action='store_true',help='Explicitly generate SHA-256 records; disabled by default.')
args=parser.parse_args()
if args.version is None:
    version=ET.parse(root/'Directory.Build.props').findtext('.//Version')
    if not version:raise ValueError('Directory.Build.props does not define Version')
    args.version=version if '-' in version else version+'-dev'
if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9_.-]+)?',args.version):raise ValueError('Invalid release version')
output=(args.output or root/'releases'/args.version).resolve()
package=(args.package or root/f'build/package/windows-{args.version.split("-")[0]}').resolve()
validation_keys=('game_validation','android_static_validation','android_device_validation','interactive_uac_validation')
validation={key:'not_run' for key in validation_keys}
if args.validation_report:
    report=json.loads(args.validation_report.read_text(encoding='utf-8-sig'))
    if not isinstance(report,dict):raise ValueError('Validation report must be a JSON object')
    for key in validation_keys:
        value=report.get(key)
        if not isinstance(value,(str,dict)) or not value:
            raise ValueError(f'Validation report requires an explicit nonempty string or object: {key}')
        validation[key]=value
for name in ('EModLoader.exe','runtime/EModLoader.Host.dll','runtime/EModLoader.Launcher.exe','runtime/EModLoader.OfflineHost.exe','runtime/EModLoader.XInputProxy.dll'):
    assert (package/name).is_file(),name
apk=args.android_apk.resolve()
if not apk.is_file():raise FileNotFoundError(apk)
first_person=(args.first_person or root/'releases/first-person/0.1.0-dev/example.first-person-0.1.0-dev.zip').resolve()
if args.first_person and not first_person.is_file():raise FileNotFoundError(first_person)
output.mkdir(parents=True,exist_ok=True)
archive=output/f'EModLoader-{args.version}-win-x64.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for file in sorted(package.rglob('*')):
        if file.is_file():zip.write(file,'EModLoader/'+file.relative_to(package).as_posix())
android=output/'EModLoader-0.1.0-dev-android-arm64.apk';shutil.copyfile(apk,android)
sdk=output/'EModLoader-SDK-0.1.0.zip'
with zipfile.ZipFile(sdk,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for folder in ('include','docs','examples/echo','examples/first-person/native','examples/first-person/ui','examples/first-person/tests'):
        for file in sorted((root/folder).rglob('*')):
            if file.is_file():zip.write(file,file.relative_to(root).as_posix())
    for name in ('README.md','LICENSE','THIRD_PARTY_NOTICES.md','Directory.Build.props',
                 'examples/first-person/README.md','examples/first-person/CMakeLists.txt','examples/first-person/module.json',
                 'scripts/BuildFirstPerson.ps1','scripts/PackageFirstPerson.py','native/shared/third_party/nlohmann/json.hpp'):
        zip.write(root/name,name)
files=[archive,android,sdk]
if first_person.is_file():
    module=output/first_person.name;shutil.copyfile(first_person,module);files.append(module)
records=[{'file':file.name,'bytes':file.stat().st_size} for file in files]
if args.hash_assets:
    for i,file in enumerate(files):records[i]['sha256']=hashlib.sha256(file.read_bytes()).hexdigest()
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
(output/'ASSETS.json').write_text(json.dumps({'version':args.version,'source_commit':commit,**validation,
    'first_person_module':'included' if first_person.is_file() else 'source_only','assets':records},indent=2)+'\n',encoding='utf-8')
if args.hash_assets:
    (output/'SHA256SUMS.txt').write_text(''.join(f"{item['sha256']}  {item['file']}\n" for item in records),encoding='ascii')
print(json.dumps(records,indent=2))
