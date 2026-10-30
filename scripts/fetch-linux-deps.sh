#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
dependency_root="$repo_root/build/_deps"
mkdir -p "$dependency_root"
fetch() {
    local name="$1" url="$2" tag="$3" commit="$4"
    local destination="$dependency_root/$name-src"
    if [[ ! -e "$destination" ]]; then
        git -c advice.detachedHead=false clone --depth 1 --branch "$tag" "$url" "$destination"
    fi
    if [[ "$(git -C "$destination" rev-parse HEAD)" != "$commit" ]]; then
        printf 'Refusing to overwrite mismatched dependency: %s\n' "$destination" >&2
        exit 1
    fi
    printf '%s %s\n' "$name" "$commit"
}
fetch muduo https://github.com/chenshuo/muduo.git v2.0.2 b55b0ac6a10d4ded1301c3172e779ec46ada2ab3
fetch googletest https://github.com/google/googletest.git v1.15.2 b514bdc898e2951020cbdca1304b75f5950d1f59

patch="$repo_root/server/patches/muduo-stop-accepting.patch"
source="$dependency_root/muduo-src"
if git -C "$source" apply --check "$patch" 2>/dev/null; then
    git -C "$source" apply "$patch"
elif ! git -C "$source" apply --reverse --check "$patch" 2>/dev/null; then
    printf 'Cannot apply the tracked muduo lifecycle patch\n' >&2
    exit 1
fi
if [[ "$(git -C "$source" diff --binary HEAD)" != "$(cat "$patch")" ]]; then
    printf 'Unexpected modifications in muduo checkout\n' >&2
    exit 1
fi
git -C "$dependency_root/googletest-src" diff --exit-code --quiet HEAD
