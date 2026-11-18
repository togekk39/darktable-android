#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write a candidate compatibility lock for explicit upstream integration review."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--revision", required=True, help="official upstream commit to review")
    parser.add_argument("--output", required=True, type=Path, help="new candidate file; never overwrites a lock")
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "--verify", args.revision + "^{commit}"], text=True).strip()
    tree = subprocess.check_output(["git", "-C", str(args.source), "ls-tree", "-rz", "--full-tree", revision])
    files, inventory, submodules = {}, [], {}
    for entry in tree.split(b"\0"):
        if not entry:
            continue
        metadata, raw_name = entry.split(b"\t", 1)
        mode, kind, object_id = metadata.split()
        name = raw_name.decode()
        if kind == b"commit":
            submodules[name] = object_id.decode()
            continue
        if name.startswith(("mobile/", "android/", "docs/")):
            parser.error("revision includes integration files; select an official upstream commit")
        inventory.append(name)
        if name.startswith(("src/", "cmake/", "tools/introspection/")) or name.endswith((".cmake", "CMakeLists.txt")):
            content = subprocess.check_output(["git", "-C", str(args.source), "cat-file", "blob", object_id.decode()])
            files[name] = hashlib.sha256(content).hexdigest()
    lock = {"upstream": {"repository": "https://github.com/darktable-org/darktable.git", "commit": revision},
            "files": files, "inventory": inventory, "submodules": submodules}
    with args.output.open("x") as output:
        json.dump(lock, output, indent=2, sort_keys=True)
        output.write("\n")
    print(f"candidate only: {args.output}; review upstream changes, adapters and patches before accepting")


if __name__ == "__main__":
    main()
