from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--json-jar',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
build=root/'build/tests/android-materializer';build.mkdir(parents=True,exist_ok=True)
stubs={
    'android/content/Context.java':'package android.content;public abstract class Context {public abstract java.io.File getFilesDir();}',
    'android/system/Os.java':'package android.system;public class Os {public static void rename(String a,String b)throws java.io.IOException {java.nio.file.Files.move(java.nio.file.Path.of(a),java.nio.file.Path.of(b),java.nio.file.StandardCopyOption.REPLACE_EXISTING);}}',
}
files=[]
for name,content in stubs.items():
    path=build/'src'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(content,encoding='utf-8');files.append(path)
source=(root/'native/tests/ThirdPartyMaterializerTest.java').read_text(encoding='utf-8').replace('dev.betterendfield.next','dev.endfield.modloader').replace('ThirdPartyRuntimeMaterializer','RuntimeMaterializer').replace('betterendfieldnext/third-party','emodloader/modules').replace('betterendfieldnext.','betterendfield.')
path=build/'src/dev/endfield/modloader/ThirdPartyMaterializerTest.java';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(source,encoding='utf-8');files.append(path)
files.append(root/'src/android/app/src/main/java/dev/endfield/modloader/RuntimeMaterializer.java')
classes=build/'classes';classes.mkdir(exist_ok=True)
subprocess.run(['javac','--release','17','-encoding','UTF-8','-cp',str(args.json_jar),'-d',str(classes),*map(str,files)],check=True)
import os
subprocess.run(['java','-cp',os.pathsep.join((str(classes),str(args.json_jar))), 'dev.endfield.modloader.ThirdPartyMaterializerTest'],check=True)
