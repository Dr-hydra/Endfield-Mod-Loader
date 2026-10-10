from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
output=root/'releases/0.1.0-dev';output.mkdir(parents=True,exist_ok=True)
package=root/'build/package/windows'
for name in ('EModLoader.exe','runtime/EModLoader.Host.dll','runtime/EModLoader.Launcher.exe','runtime/EModLoader.OfflineHost.exe'):
    assert (package/name).is_file(),name
archive=output/'EModLoader-0.1.0-dev-win-x64.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for file in sorted(package.rglob('*')):
        if file.is_file():zip.write(file,'EModLoader/'+file.relative_to(package).as_posix())
apk=root/'build/android/app/outputs/apk/debug/app-debug.apk'
assert apk.is_file()
android=output/'EModLoader-0.1.0-dev-android-arm64.apk';shutil.copyfile(apk,android)
sdk=output/'EModLoader-SDK-0.1.0.zip'
with zipfile.ZipFile(sdk,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as zip:
    for folder in ('include','examples/echo'):
        for file in sorted((root/folder).rglob('*')):
            if file.is_file():zip.write(file,file.relative_to(root).as_posix())
    for name in ('LICENSE','docs/SDK.md','docs/COMPATIBILITY_PLAN.md'):zip.write(root/name,name)
records=[{'file':file.name,'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()} for file in (archive,android,sdk)]
(output/'ASSETS.json').write_text(json.dumps({'version':'0.1.0-dev','game_validation':'not_run','first_person_module':'not_implemented','assets':records},indent=2)+'\n',encoding='utf-8')
print(json.dumps(records,indent=2))
