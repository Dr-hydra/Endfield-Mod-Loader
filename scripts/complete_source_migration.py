"""Remove only verified, copied extension-owned files from the original workspace."""
from pathlib import Path
import argparse
import hashlib
import json

OWNED_PREFIXES = (
    "native/shared/third_party_modules/", "native/tests/third_party_ui/",
    "tools/ThirdPartyModules/", "android/app/src/main/java/dev/betterendfield/next/ThirdParty",
)
OWNED_FILES = {
    "native/shared/include/BetterEndfieldNext/ThirdPartyModule.h",
    "native/tests/third_party_host_test.cpp", "native/tests/third_party_host_test.py",
    "native/tests/ThirdPartyMaterializerTest.java", "native/tests/run_third_party_materializer_test.py",
    "ui/BetterEndfieldNext.UI/Services/ThirdPartyModuleService.cs",
    "ui/BetterEndfieldNext.UI/Services/ThirdPartyWebBridge.cs",
    "ui/BetterEndfieldNext.UI/Views/ThirdPartyModuleWindow.cs",
    "ui/BetterEndfieldNext.UI/Views/ThirdPartyModulesPage.cs",
    "android/app/src/main/assets/third-party-bridge.js",
    "android/app/src/main/res/xml/module_network_security.xml",
    "docs/host/THIRD_PARTY_MODULE_CREATOR_GUIDE.md", "docs/host/CREATOR_MODULE_API_DESIGN.md",
}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    target = Path(__file__).resolve().parents[1]
    manifest = json.loads((target / "provenance/migration.json").read_text(encoding="utf-8"))
    planned = []
    for record in manifest["files"]:
        relative = record["source"]
        if record["role"] != "module" or not (relative in OWNED_FILES or relative.startswith(OWNED_PREFIXES)):
            continue
        original = (source / relative).resolve()
        saved = (target / record["destination"]).resolve()
        if not original.is_relative_to(source) or not saved.is_relative_to(target):
            raise ValueError("Path escapes a workspace")
        if hashlib.sha256(saved.read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError(f"Import changed: {relative}")
        if original.exists():
            if hashlib.sha256(original.read_bytes()).hexdigest() != record["sha256"]:
                raise ValueError(f"Source changed since copy: {relative}")
            planned.append(original)
    for original in planned:
        original.unlink()
    print(f"Removed {len(planned)} verified extension-owned source files; shared Host and Hook code retained.")

if __name__ == "__main__":
    main()
