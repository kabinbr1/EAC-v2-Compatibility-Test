#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$repo_root/build}"
appdir="$build_dir/AppDir"
tools_dir="$build_dir/appimage-tools"
dist_dir="${DIST_DIR:-$repo_root/dist}"
linuxdeploy_version="1-alpha-20251107-1"
qt_plugin_version="1-alpha-20250213-1"
linuxdeploy="$tools_dir/linuxdeploy-x86_64.AppImage"
qt_plugin="$tools_dir/linuxdeploy-plugin-qt-x86_64.AppImage"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: CMake is required to build the AppImage." >&2
    exit 1
fi
if ! command -v qmake6 >/dev/null 2>&1; then
    echo "Error: Qt 6 development tools (qmake6) are required to build the AppImage." >&2
    exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
    echo "Error: curl is required to download the pinned AppImage tools." >&2
    exit 1
fi
if ! command -v file >/dev/null 2>&1; then
    echo "Error: the file utility is required to generate an AppImage." >&2
    exit 1
fi

mkdir -p "$tools_dir" "$dist_dir"

if [[ ! -x "$linuxdeploy" ]]; then
    curl --fail --location --silent --show-error \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/${linuxdeploy_version}/linuxdeploy-x86_64.AppImage" \
        --output "$linuxdeploy"
    chmod +x "$linuxdeploy"
fi

if [[ ! -x "$qt_plugin" ]]; then
    curl --fail --location --silent --show-error \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${qt_plugin_version}/linuxdeploy-plugin-qt-x86_64.AppImage" \
        --output "$qt_plugin"
    chmod +x "$qt_plugin"
fi

cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel
cmake --install "$build_dir" --prefix "$appdir/usr"

output_dir="$(mktemp -d "$build_dir/appimage-output.XXXXXX")"
trap 'rm -rf "$output_dir"' EXIT

(
    cd "$output_dir"
    QMAKE="$(command -v qmake6)" NO_STRIP=1 APPIMAGE_EXTRACT_AND_RUN=1 \
        "$linuxdeploy" --appimage-extract-and-run \
        --appdir "$appdir" \
        --plugin qt \
        --output appimage
)

shopt -s nullglob
appimages=("$output_dir"/*.AppImage)
if [[ "${#appimages[@]}" -ne 1 ]]; then
    echo "Error: expected one AppImage from linuxdeploy, found ${#appimages[@]}." >&2
    exit 1
fi

artifact="$dist_dir/EAC-v2-Compatibility-Test-x86_64.AppImage"
install -m 755 "${appimages[0]}" "$artifact"
printf 'Created %s\n' "$artifact"
