"""Derive Android module management while preserving imported sources."""
from pathlib import Path
import argparse
import shutil

parser=argparse.ArgumentParser();parser.add_argument('--be-source',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
target=root/'src/android/app/src/main/java/dev/endfield/modloader';target.mkdir(parents=True,exist_ok=True)
original=root/'android/app/src/main/java/dev/betterendfield/next'
names={'ThirdPartyModulePackage':'ModulePackage','ThirdPartyModuleStore':'ModuleStore','ThirdPartyModulesPage':'ModulePage','ThirdPartyModuleActivity':'ModuleActivity','ThirdPartyRuntimeMaterializer':'RuntimeMaterializer'}
for old,new in names.items():
    text=(original/(old+'.java')).read_text(encoding='utf-8').replace('dev.betterendfield.next','dev.endfield.modloader')
    for before,after in names.items():text=text.replace(before,after)
    text=text.replace('betterendfieldnext.','betterendfield.').replace('betterendfieldnext/third-party','emodloader/modules')
    text=text.replace('third_party_runtime','module_runtime').replace('third-party-bridge.js','module-bridge.js')
    text=text.replace('BetterEndfieldNextModuleHost','BetterEndfieldModuleHost').replace('better-endfield-next.module-ui.v1','better-endfield.module-ui.v1')
    text=text.replace('"third-party"','"modules"').replace('.bemod.local','.eml.local')
    text=text.replace('"游戏模块未连接"','"模块宿主未连接"')
    if new=='ModuleStore':text=text.replace('.put("connected",result.optBoolean("connected",false))','.put("connected",result.optBoolean("connected",false)).put("mode",result.optString("mode",""))')
    (target/(new+'.java')).write_text(text,encoding='utf-8')
bridge=(root/'android/app/src/main/assets/third-party-bridge.js').read_text(encoding='utf-8').replace('betterEndfieldNext','betterEndfield').replace('BetterEndfieldNextModuleHost','BetterEndfieldModuleHost').replace('better-endfield-next.module-ui.v1','better-endfield.module-ui.v1')
assets=root/'src/android/app/src/main/assets';assets.mkdir(parents=True,exist_ok=True);(assets/'module-bridge.js').write_text(bridge,encoding='utf-8')
drawables=root/'src/android/app/src/main/res/drawable-nodpi';drawables.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/'resources/logo.png',drawables/'logo.png')
for name in ('gradlew','gradlew.bat','gradle/wrapper/gradle-wrapper.properties','gradle/wrapper/gradle-wrapper.jar'):
    output=root/'src/android'/name;output.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(args.be_source/'android'/name,output)
print('Android product management sources and original logo copied into the independent app.')
