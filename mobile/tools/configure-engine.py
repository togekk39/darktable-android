#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Configure the Android engine from an isolated, reviewed upstream source view."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-B", "--build-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("cmake_args", nargs=argparse.REMAINDER, help="CMake configure arguments after --")
    args = parser.parse_args()
    cmake_args = args.cmake_args
    if cmake_args[:1] == ["--"]:
        cmake_args = cmake_args[1:]
    if any(arg.startswith(("-S", "-B")) or arg in ("--build", "--install", "--fresh") for arg in cmake_args):
        parser.error("source and build directories belong to the integration; pass -B before --")
    source, build = args.source_root.resolve(), args.build_dir.resolve()
    integration = Path(__file__).resolve().parents[1]
    if source == build or source.is_relative_to(build):
        parser.error("the build directory must be separate from the source checkout")
    cmake = shutil.which(args.cmake)
    if not cmake:
        parser.error(f"CMake executable not found: {args.cmake}")
    view = build / "_upstream"
    try:
        subprocess.run([sys.executable, str(integration / "tools/assemble-source.py"),
                        "--source", str(source), "--output", str(view),
                        "--manifest", str(integration / "upstream-lock.json")], check=True)
        subprocess.run([cmake, "-S", str(view), "-B", str(build),
                        f"-DDT_MOBILE_SOURCE_ROOT:PATH={source}",
                        f"-DDT_MOBILE_INTEGRATION_ROOT:PATH={integration}", *cmake_args], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Android engine configuration failed: {error}\n")


if __name__ == "__main__":
    main()
