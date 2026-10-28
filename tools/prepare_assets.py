"""Prepare the user's Zombiewood files for the isolated Vita3K test."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path


def prepare(source: Path, apk: Path, destination: Path):
    destination.mkdir(parents=True, exist_ok=True)
    destination = destination.resolve()
    archives = [
        "main.1501.com.gameloft.android.ANMP.GloftZOHM.obb",
        "patch.15325.com.gameloft.android.ANMP.GloftZOHM.obb",
    ]
    manifest = {}
    for name in ["libZombieWood.so", *archives]:
        original = source / name
        with original.open("rb") as handle:
            manifest[name] = hashlib.file_digest(handle, "sha256").hexdigest()
        if original.resolve() != (destination / name).resolve():
            shutil.copy2(original, destination / name)
    # Patch assets must replace main assets with the same name.
    for name in archives:
        with zipfile.ZipFile(source / name) as archive:
            bad = archive.testzip()
            if bad:
                raise ValueError(f"Corrupt archive member: {name}/{bad}")
            for member in archive.infolist():
                target = (destination / member.filename).resolve()
                if not target.is_relative_to(destination):
                    raise ValueError(f"Unsafe archive path: {member.filename}")
                archive.extract(member, destination)
    with zipfile.ZipFile(apk) as archive:
        for member in archive.infolist():
            if member.filename.startswith("assets/"):
                target = (destination / member.filename).resolve()
                if not target.is_relative_to(destination):
                    raise ValueError(f"Unsafe APK asset path: {member.filename}")
                archive.extract(member, destination)
    for name in ["zombies_load.swf", "zombies.swf", "assets/colors.conf", "assets/families.list"]:
        if not (destination / name).is_file():
            raise ValueError(f"Required startup asset missing: {name}")
    (destination / "asset-manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"Prepared original assets in {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--apk", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.apk, args.destination)
