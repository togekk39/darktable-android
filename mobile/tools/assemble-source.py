#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Assemble reviewed upstream sources in the build directory, without editing them."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess


class IntegrationError(RuntimeError):
    pass


def safe_path(name):
    path = PurePosixPath(name)
    if path.is_absolute() or not path.parts or ".." in path.parts:
        raise IntegrationError(f"unsafe integration path: {name}")
    return Path(*path.parts)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def apply_patch(original, patch, filename):
    """Apply unified hunks at their exact line and context; never search or fuzz."""
    lines = original.splitlines(keepends=True)
    diff = patch.splitlines(keepends=True)
    result, cursor, index = [], 0, 0
    seen = False
    while index < len(diff):
        header = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", diff[index])
        if not header:
            index += 1
            continue
        seen = True
        start = int(header[1]) - 1 if int(header[1]) else 0
        old_count = int(header[2]) if header[2] is not None else 1
        new_count = int(header[4]) if header[4] is not None else 1
        if start < cursor or start > len(lines):
            raise IntegrationError(f"{filename}: invalid or overlapping patch hunk")
        result.extend(lines[cursor:start])
        new_start = int(header[3]) - 1 if int(header[3]) else 0
        if new_start != len(result):
            raise IntegrationError(f"{filename}: inconsistent new hunk position")
        cursor = start
        index += 1
        old_seen = new_seen = 0
        while index < len(diff) and not diff[index].startswith(("@@ ", "diff --git ", "--- ")):
            item = diff[index]
            if item.startswith("\\ No newline"):
                raise IntegrationError(f"{filename}: non-newline patch unsupported")
            if not item or item[0] not in " +-":
                break
            if item[0] in " -":
                if cursor >= len(lines) or lines[cursor] != item[1:]:
                    raise IntegrationError(f"{filename}:{cursor + 1}: upstream patch context changed; review required")
                cursor += 1
                old_seen += 1
            if item[0] in " +":
                result.append(item[1:])
                new_seen += 1
            index += 1
        if (old_seen, new_seen) != (old_count, new_count):
            raise IntegrationError(f"{filename}: malformed patch hunk counts")
    if not seen:
        raise IntegrationError(f"{filename}: patch has no hunks")
    result.extend(lines[cursor:])
    return "".join(result)


def patch_sections(text):
    sections = re.split(r"(?=^--- a/)", text, flags=re.MULTILINE)
    for section in sections:
        match = re.match(r"--- a/([^\t\n]+)(?:[^\n]*)\n\+\+\+ b/([^\t\n]+)(?:[^\n]*)\n", section)
        if match:
            if match[1] != match[2]:
                raise IntegrationError("integration patches cannot rename upstream files")
            yield safe_path(match[1]), section


def write_if_changed(path, data):
    if path.is_symlink() or any(parent.is_symlink() for parent in path.parents):
        raise IntegrationError(f"refusing to write through symlink: {path}")
    if not path.exists() or path.read_bytes() != data:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def assemble(source, output, manifest, integration, verify_only=False):
    source, output = source.resolve(), output.resolve()
    if output == source or source.is_relative_to(output):
        raise IntegrationError("output must be a separate build-tree directory")
    lock = json.loads(manifest.read_text())
    if (source / ".git").exists():
        roots = {"src", "cmake", "tools", "data", "po"}
        roots.update(name.split("/", 1)[0] for name in lock["inventory"]
                     if "/" in name and not name.startswith(".github/"))
        tracked = subprocess.check_output(
            ["git", "-C", str(source), "ls-files", "-z", "--", *sorted(roots)], text=True)
        expected = set(lock["inventory"]) | set(lock["submodules"])
        additions = set(tracked.rstrip("\0").split("\0")) - expected
        if additions:
            raise IntegrationError(f"unreviewed upstream files: {', '.join(sorted(additions))}; update the reviewed inventory")
    # Validate before writing any generated sources. Header and policy hashes
    # catch API changes even when the patched function itself did not change.
    for name, digest in lock["files"].items():
        path = source / safe_path(name)
        if not path.is_file() or sha256(path) != digest:
            raise IntegrationError(f"{name}: differs from reviewed upstream {lock['upstream']['commit']}; review integration and update mobile/upstream-lock.json")
    submodule_files = {}
    for name, revision in lock["submodules"].items():
        src = source / safe_path(name)
        if not (src / ".git").exists():
            continue
        actual = subprocess.check_output(["git", "-C", str(src), "rev-parse", "HEAD"], text=True).strip()
        if actual != revision:
            raise IntegrationError(f"{name}: submodule changed from reviewed {revision}")
        dirty = subprocess.run(["git", "-C", str(src), "diff", "--quiet", "HEAD", "--"], check=False)
        if dirty.returncode:
            raise IntegrationError(f"{name}: tracked submodule sources differ from reviewed revision")
        tracked = subprocess.check_output(["git", "-C", str(src), "ls-files", "-z"], text=True)
        submodule_files[name] = [safe_path(path) for path in tracked.rstrip("\0").split("\0") if path]
    if verify_only:
        print(f"verified reviewed upstream {lock['upstream']['commit']}")
        return
    marker = output / ".dt-mobile-source-view"
    if output.exists() and any(output.iterdir()) and not marker.exists():
        raise IntegrationError(f"refusing to reuse unowned output directory: {output}")
    if marker.is_symlink() or (marker.exists() and marker.read_text() != str(source) + "\n"):
        raise IntegrationError(f"source-view ownership changed: {output}")
    output.mkdir(parents=True, exist_ok=True)
    marker.write_text(str(source) + "\n")
    patched = {}
    for patch in sorted((integration / "patches").glob("*/*.patch")):
        sections = list(patch_sections(patch.read_text()))
        if not sections:
            raise IntegrationError(f"{patch}: no unified patch file headers")
        for relative, section in sections:
            name = relative.as_posix()
            if name not in lock["files"]:
                raise IntegrationError(f"{name}: patched source is missing from upstream lock")
            original = patched.get(relative, (source / relative).read_text())
            patched[relative] = apply_patch(original, section, name)
    expected = set()
    # Copy the official file inventory, including unchanged code and data.
    # Copies prevent build-system writes from ever reaching the checkout.
    for name in lock["inventory"]:
        relative = safe_path(name)
        expected.add(relative)
        src, dst = source / relative, output / relative
        if src.is_symlink():
            data = src.read_bytes()
        elif src.is_file():
            data = src.read_bytes()
        else:
            raise IntegrationError(f"missing upstream file: {name}")
        write_if_changed(dst, patched[relative].encode() if relative in patched else data)
        dst.chmod(src.stat().st_mode & 0o777)
    # Submodules remain independently updateable, but their revisions are part
    # of the reviewed lock. Copy initialized contents, excluding Git metadata.
    for name, tracked_files in submodule_files.items():
        src = source / safe_path(name)
        for relative in tracked_files:
            path = src / relative
            if not path.is_file():
                continue
            dst = output / name / relative
            write_if_changed(dst, path.read_bytes())
            dst.chmod(path.stat().st_mode & 0o777)
            expected.add(Path(name) / relative)
    for path in output.rglob("*"):
        relative = path.relative_to(output)
        if path.is_file() and relative not in expected and path != marker and relative.parts[0] not in ("mobile", ".git"):
            path.unlink()
    mobile = output / "mobile"
    if mobile.is_symlink():
        if mobile.resolve() != integration.resolve():
            raise IntegrationError(f"unexpected mobile source link: {mobile}")
    elif mobile.exists():
        raise IntegrationError(f"unexpected mobile source directory: {mobile}")
    else:
        mobile.symlink_to(integration.resolve(), target_is_directory=True)
    # Version generation reads Git; the build never mutates this link.
    git = output / ".git"
    if (source / ".git").exists() and not git.exists():
        git.symlink_to((source / ".git").resolve(), target_is_directory=(source / ".git").is_dir())
    print(f"assembled upstream {lock['upstream']['commit']} with {len(patched)} minimal source patches in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--verify-only", action="store_true", help="check upstream compatibility without writing files")
    args = parser.parse_args()
    try:
        assemble(args.source, args.output, args.manifest, Path(__file__).resolve().parents[1], args.verify_only)
    except (IntegrationError, OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Android upstream integration: {error}\n")


if __name__ == "__main__":
    main()
