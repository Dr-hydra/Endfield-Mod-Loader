"""One-time, byte-preserving import. No binaries, credentials or user settings."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

DIRECTORIES = (
    "native/shared/third_party_modules",
    "native/tests/third_party_ui",
    "tools/ThirdPartyModules",
)
FILES = (
    "native/shared/include/BetterEndfieldNext/ThirdPartyModule.h",
    "native/shared/include/BetterEndfieldNext/ModuleApi.h",
    "native/shared/include/BetterEndfieldNext/HookChain.h",
    "native/shared/third_party/nlohmann/json.hpp",
    "native/tests/third_party_host_test.cpp",
    "native/tests/third_party_host_test.py",
    "native/tests/ThirdPartyMaterializerTest.java",
    "native/tests/run_third_party_materializer_test.py",
    "ui/BetterEndfieldNext.UI/Services/ThirdPartyModuleService.cs",
    "ui/BetterEndfieldNext.UI/Services/ThirdPartyWebBridge.cs",
    "ui/BetterEndfieldNext.UI/Views/ThirdPartyModuleWindow.cs",
    "ui/BetterEndfieldNext.UI/Views/ThirdPartyModulesPage.cs",
    "android/app/src/main/assets/third-party-bridge.js",
    "android/app/src/main/res/xml/module_network_security.xml",
    "docs/host/THIRD_PARTY_MODULE_CREATOR_GUIDE.md",
    "docs/host/CREATOR_MODULE_API_DESIGN.md",
)
INTEGRATION = (
    "native/CMakeLists.txt",
    "native/shared/host/host_runtime.cpp",
    "native/shared/host/host_runtime.h",
    "android/app/src/main/cpp/CMakeLists.txt",
    "android/app/src/main/cpp/native_bridge.cpp",
    "android/app/src/main/java/dev/betterendfield/next/FrameworkSettings.java",
    "android/app/src/main/java/dev/betterendfield/next/ModuleConfigurations.java",
    "android/app/src/main/java/dev/betterendfield/next/RuntimeBootstrap.java",
    "android/app/src/main/java/dev/betterendfield/next/NativeCommandBridge.java",
    "android/app/src/main/java/dev/betterendfield/next/XposedEntry.java",
    "android/app/src/main/AndroidManifest.xml",
    "android/app/src/testHost/enhancement_navigation_fixture.py",
    "ui/BetterEndfieldNext.UI/MainWindow.xaml",
    "ui/BetterEndfieldNext.UI/MainWindow.xaml.cs",
    "ui/BetterEndfieldNext.UI/Services/LocalizationService.cs",
    "ui/BetterEndfieldNext.UI/Assets/locales/zh-CN.json",
    "ui/BetterEndfieldNext.UI/Assets/locales/en-US.json",
    "scripts/BuildBetterEndfieldNext.ps1",
    "scripts/PublishRelease.py",
    "config/test-suites.json",
    "native/tests/bem_import/run_state_tests.py",
    "native/tests/bem_import/check_java.py",
    "scripts/tests/test_next_identity.py",
    "README.md",
    "README.en.md",
    "android/README.md",
    "docs/host/INDEX.md",
    "THIRD_PARTY_NOTICES.md",
)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    target = Path(__file__).resolve().parents[1]
    if source == target:
        raise ValueError("Source and target must differ")
    records = []

    def copy(relative, destination=None, role="module"):
        original = source / relative
        output = target / (destination or relative)
        if not original.is_file() or not output.resolve().is_relative_to(target):
            raise ValueError(f"Invalid copy: {relative}")
        content = original.read_bytes()
        if output.exists() and output.read_bytes() != content:
            raise ValueError(f"Refusing to overwrite changed import: {output}")
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, output)
        digest = hashlib.sha256(content).hexdigest()
        if hashlib.sha256(output.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Copy verification failed: {relative}")
        records.append({"source": relative, "destination": output.relative_to(target).as_posix(),
                        "sha256": digest, "role": role})

    tracked = subprocess.check_output(["git", "-C", str(source), "ls-files", "-z"]).decode().split("\0")
    selected = set(FILES)
    for relative in tracked:
        if any(relative.startswith(prefix + "/") for prefix in DIRECTORIES):
            selected.add(relative)
        if relative.startswith("android/app/src/main/java/dev/betterendfield/next/ThirdParty"):
            selected.add(relative)
    for relative in sorted(selected):
        copy(relative)
    for relative in INTEGRATION:
        copy(relative, "provenance/integration/" + relative, "integration-reference")
    archive = "legacy/retired-before-next/first-person/"
    for relative in tracked:
        if relative.startswith(archive):
            copy(relative, "examples/first-person/archive/" + relative[len(archive):], "first-person-archive")
    copy("legacy/retired-before-next/manifest.json", "examples/first-person/be-retirement-manifest.json", "archive-provenance")
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    manifest = {"source_repository": "https://github.com/Dr-hydra/Better-Endfield", "source_commit": commit,
                "captured_working_tree": True, "import_mode": "byte-preserving",
                "game_bootstrap": "undecided", "files": records}
    output = target / "provenance/migration.json"
    output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Copied and SHA-256 verified {len(records)} source files.")

if __name__ == "__main__":
    main()
