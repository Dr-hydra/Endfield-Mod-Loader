"""Create an importable V1 module ZIP from independently built platforms."""
from pathlib import Path
import argparse
import hashlib
import json
import re
from urllib.parse import quote
import xml.etree.ElementTree as ET
import zipfile

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--windows',type=Path,default=root/'build/examples/first-person-windows/package')
parser.add_argument('--android',type=Path)
parser.add_argument('--output',type=Path,default=root/'releases/first-person/0.1.0-dev/example.first-person-0.1.0-dev.zip')
parser.add_argument('--source-ref',help='Git tag or commit for README source links; defaults to v<Directory.Build.props Version>-dev.')
parser.add_argument('--hash-assets',action='store_true',help='Explicitly generate a SHA-256 record; disabled by default.')
args=parser.parse_args()
if args.source_ref is None:
    version=ET.parse(root/'Directory.Build.props').findtext('.//Version')
    if not version:raise ValueError('Directory.Build.props does not define Version')
    args.source_ref='v'+(version if '-' in version else version+'-dev')
if not args.source_ref.strip():raise ValueError('Source ref cannot be empty')
source_url='https://github.com/Dr-hydra/Endfield-Mod-Loader/blob/'+quote(args.source_ref,safe='')+'/'
example=root/'examples/first-person'
manifest=json.loads((example/'module.json').read_text(encoding='utf-8'))
libraries={}
for platform,folder in (('windows-x64',args.windows),('android-arm64',args.android)):
    if folder is None:
        continue
    package_manifest=json.loads((folder/'module.json').read_text(encoding='utf-8'))
    for key in ('id','version','format','abi','default_configuration'):
        if package_manifest[key]!=manifest[key]:
            raise ValueError(f'{platform}: build metadata is stale ({key})')
    relative=manifest['libraries'][platform]
    library=folder/relative
    if not library.is_file():
        raise FileNotFoundError(library)
    libraries[platform]=(relative,library)
manifest['libraries']={platform:relative for platform,(relative,_) in libraries.items()}
args.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as package:
    package.writestr('module.json',json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    for relative,library in libraries.values():
        package.write(library,relative)
    for path in sorted((example/'ui').rglob('*')):
        if path.is_file():
            package.write(path,path.relative_to(example).as_posix())
    guide=(example/'README.md').read_text(encoding='utf-8')
    # This guide sits at ZIP root, so source-relative documentation links must
    # resolve to the published source tree instead of escaping the package.
    guide=re.sub(r'\]\(\.\./\.\./([^\)]+)\)',
        lambda match:']('+source_url+match.group(1)+')',guide)
    package.writestr('README.md',guide)
    package.write(root/'LICENSE','LICENSE')
record={'file':args.output.name,'bytes':args.output.stat().st_size,
        'platforms':list(libraries),'game_validation':'not_run','android_device_validation':'not_run'}
if args.hash_assets:record['sha256']=hashlib.sha256(args.output.read_bytes()).hexdigest()
args.output.with_suffix('.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record,indent=2))
