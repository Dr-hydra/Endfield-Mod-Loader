"""Create the maintained product sources; imported snapshots remain immutable."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--logo', type=Path, required=True)
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--be-source', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
records = []
def write(relative, content, source):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists(): raise ValueError('Already initialized: ' + relative)
    path.write_bytes(content)
    records.append({'path': relative, 'source': source, 'sha256_at_initialization': hashlib.sha256(content).hexdigest()})

logo = args.logo.read_bytes()
assert logo.startswith(b'\x89PNG\r\n\x1a\n')
width, height = struct.unpack('>II', logo[16:24])
assert width <= 256 and height <= 256
write('resources/logo.png', logo, 'user-provided logo')
# ICO wraps the original PNG without resizing or editing its pixels.
icon = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', width % 256, height % 256, 0, 0, 1, 32, len(logo), 22) + logo
write('resources/logo.ico', icon, 'user-provided logo, PNG container in ICO')
with zipfile.ZipFile(args.sdk) as sdk:
    for name in ('ModuleApi.h','HookChain.h','ThirdPartyModule.h'):
        write('include/BetterEndfield/' + name, sdk.read('include/BetterEndfield/' + name), 'original BE SDK 1.0.0')

original = root / 'native/shared/third_party_modules'
text = (original/'third_party_host.cpp').read_text(encoding='utf-8')
text = text.replace('"third_party_host.h"', '"module_runtime.h"').replace('"../third_party/nlohmann/json.hpp"','<nlohmann/json.hpp>')
text = text.replace('BetterEndfieldNext::ThirdParty','EML').replace('ThirdPartyHost','ModuleRuntime').replace('BE_ModuleRuntimeV1','BE_ThirdPartyHostV1').replace('BetterEndfieldNext_GetThirdPartyModuleV1','BetterEndfield_GetThirdPartyModuleV1')
text = text.replace('lower.starts_with("betterendfieldnext.")','lower.starts_with("betterendfield.")')
text = text.replace('uint16_t port=0;', 'uint16_t port=0;std::string mode="offline";')
text = text.replace('{{"connected",true},{"protocol",1}', '{{"connected",true},{"host_product","Endfield Mod Loader"},{"mode",mode},{"protocol",1}')
text = text.replace('const BE_HookChainApiV1* hooks){', 'const BE_HookChainApiV1* hooks,const std::string& mode){')
text = text.replace('s.runtime=runtime;s.hooks=hooks;', 's.runtime=runtime;s.hooks=hooks;s.mode=mode;')
text = text.replace('socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)', 'socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)')
write('src/native/runtime/module_runtime.cpp', text.encode(), 'derived from imported third_party_host.cpp; AGPL-3.0-only')
text = (original/'third_party_host.h').read_text(encoding='utf-8').replace('BetterEndfieldNext/','BetterEndfield/').replace('BetterEndfieldNext::ThirdParty','EML').replace('ThirdPartyHost','ModuleRuntime')
text = text.replace('const BE_HookChainApiV1* hooks=nullptr);','const BE_HookChainApiV1* hooks=nullptr,const std::string& mode="offline");')
write('src/native/runtime/module_runtime.h', text.encode(), 'derived from imported third_party_host.h; AGPL-3.0-only')
for relative, target in [('native/shared/hooks/hook_chain.h','src/native/hooks/hook_chain.h'),('native/shared/host/type_name_contract.h','src/native/runtime/type_name_contract.h'),('native/shared/host/dynamic_resolver.h','src/native/runtime/engine_resolver.h'),('native/shared/host/dynamic_resolver.cpp','src/native/runtime/engine_resolver.cpp')]:
    content=(args.be_source/relative).read_text(encoding='utf-8')
    content=content.replace('BetterEndfieldNext','EModLoader').replace('EModLoader/ModuleApi.h','BetterEndfield/ModuleApi.h').replace('dynamic_resolver.h','engine_resolver.h').replace('DynamicResolver','EngineResolver')
    write(target, content.encode(), 'derived from BE '+relative+'; AGPL-3.0-only')
vendor=root/'src/native/vendor/minhook'
shutil.copytree(args.be_source/'native/shared/third_party/minhook',vendor,ignore=shutil.ignore_patterns('build','.git'))
write('resources/source-initialization.json',(json.dumps({'files':records},indent=2)+'\n').encode(),'initialization record')
print('Product sources initialized; original logo pixels and imported snapshots preserved.')
