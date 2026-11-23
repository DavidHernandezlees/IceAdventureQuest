"""Check a built APK against the editable project's resource tree."""
import argparse
import hashlib
from pathlib import Path
from zipfile import ZipFile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("apk", type=Path)
args = parser.parse_args()
resources = Path(__file__).resolve().parents[1] / "Resources"


def digest(stream):
    value = hashlib.sha256()
    for block in iter(lambda: stream.read(256 * 1024), b""):
        value.update(block)
    return value.digest()


with ZipFile(args.apk) as apk:
    entries = set(apk.namelist())
    index_name = "assets/original-index.txt"
    if index_name not in entries:
        raise SystemExit("Missing original campaign index. This APK predates the resource update.")
    stages = apk.read(index_name).decode("utf-8").splitlines()
    if not stages or any("assets/" + stage not in entries for stage in stages):
        raise SystemExit("The APK contains an incomplete original campaign.")
    count = 0
    for file in sorted(resources.rglob("*")):
        if not file.is_file():
            continue
        name = "assets/" + file.relative_to(resources).as_posix()
        if name not in entries:
            raise SystemExit("Asset absent from APK: " + name)
        with file.open("rb") as original, apk.open(name) as bundled:
            if digest(original) != digest(bundled):
                raise SystemExit("Asset differs from source: " + name)
        count += 1
    print(f"Verified {count} resources and {len(stages)} indexed stages in {args.apk.name}.")
