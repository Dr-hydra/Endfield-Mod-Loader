"""Verify imported bytes without requiring the original BE workspace."""
from pathlib import Path
import hashlib
import json


def main():
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "provenance/migration.json").read_text(encoding="utf-8"))
    for item in manifest["files"]:
        path = (root / item["destination"]).resolve()
        if not path.is_relative_to(root):
            raise ValueError("Manifest path escapes workspace")
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError("Imported source changed: " + item["destination"])
    print(f"PASS: {len(manifest['files'])} imported source files match the migration manifest.")


if __name__ == "__main__":
    main()
