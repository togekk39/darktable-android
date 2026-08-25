#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

readonly VCPKG_TAG=2025.06.13
readonly ABI=arm64-android
readonly API=26
readonly root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
readonly triplet_dir="$root_dir/mobile/dependencies/triplets"
readonly source_dir=${DT_MOBILE_VCPKG_ROOT:-"$root_dir/.vcpkg/$VCPKG_TAG"}
readonly install_dir=${DT_MOBILE_VCPKG_INSTALLED:-"$root_dir/.vcpkg/installed"}

: "${ANDROID_NDK_HOME:?ANDROID_NDK_HOME must name NDK 27.2.12479018}"
if [[ $(basename "$ANDROID_NDK_HOME") != 27.2.12479018 ]]; then
  echo "Expected Android NDK 27.2.12479018, got $ANDROID_NDK_HOME" >&2
  exit 1
fi
if [[ ! -d "$source_dir/.git" ]]; then
  mkdir -p "$(dirname "$source_dir")"
  git clone --branch "$VCPKG_TAG" --depth 1 https://github.com/microsoft/vcpkg.git "$source_dir"
fi
test "$(git -C "$source_dir" describe --tags --exact-match)" = "$VCPKG_TAG"
[[ -x "$source_dir/vcpkg" ]] || "$source_dir/bootstrap-vcpkg.sh" -disableMetrics

export ANDROID_NDK_HOME
export VCPKG_FORCE_SYSTEM_BINARIES=1
"$source_dir/vcpkg" install \
  --triplet "$ABI" \
  --overlay-triplets="$triplet_dir" \
  --x-manifest-root="$root_dir/mobile/dependencies" \
  --x-install-root="$install_dir" \
  --clean-after-build

printf 'VCPKG_ROOT=%s\nVCPKG_INSTALLED_DIR=%s\nVCPKG_TARGET_TRIPLET=%s\nANDROID_PLATFORM=android-%s\n' \
  "$source_dir" "$install_dir" "$ABI" "$API" > "$root_dir/mobile/dependencies/android-dependencies.env"
