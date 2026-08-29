#!/usr/bin/env bash

set -Eeuo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-build}"
build_jobs="${BUILD_JOBS:-$(nproc)}"

if [[ "$build_dir" != /* ]]; then
    build_dir="$repo_root/$build_dir"
fi

for command in cmake ninja dpkg-deb dpkg-shlibdeps; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'Error: required command not found: %s\n' "$command" >&2
        exit 1
    fi
done

cmake -S "$repo_root" -B "$build_dir" -G Ninja \
    -D CMAKE_BUILD_TYPE=Release \
    -D CPACK_GENERATOR=DEB \
    "$@"

cmake --build "$build_dir" --target package --parallel "$build_jobs"

deb_path=""
while IFS= read -r -d '' candidate; do
    if [[ -z "$deb_path" || "$candidate" -nt "$deb_path" ]]; then
        deb_path="$candidate"
    fi
done < <(find "$build_dir" -maxdepth 1 -type f -name '*.deb' -print0)

if [[ -z "$deb_path" ]]; then
    printf 'Error: CPack completed but no DEB package was found in %s\n' "$build_dir" >&2
    exit 1
fi

printf '\nBuilt package: %s\n' "$deb_path"
dpkg-deb -f "$deb_path" Package Version Architecture
sha256sum "$deb_path"