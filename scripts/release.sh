#!/usr/bin/env bash
#
# Cut a release: bump the version (single source of truth in CMakeLists.txt),
# commit, tag, and push. The Release workflow then verifies the tag matches the
# project version and publishes the GitHub release.
#
#   scripts/release.sh 0.2.0
#
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <version>   (e.g. $0 0.2.0)" >&2
  exit 1
fi
new="$1"

if [[ ! "$new" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: version must be semver X.Y.Z (got '$new')" >&2
  exit 1
fi

branch="$(git rev-parse --abbrev-ref HEAD)"
if [[ "$branch" != "main" ]]; then
  echo "error: releases are cut from 'main' (on '$branch')" >&2
  exit 1
fi
if [[ -n "$(git status --porcelain)" ]]; then
  echo "error: working tree is not clean; commit or stash first" >&2
  exit 1
fi

git pull --ff-only origin main

current="$(sed -n -E 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt | head -1)"
if [[ -z "$current" ]]; then
  echo "error: could not find the project VERSION line in CMakeLists.txt" >&2
  exit 1
fi
if [[ "$new" == "$current" ]]; then
  echo "error: version is already $current" >&2
  exit 1
fi

if git rev-parse -q --verify "refs/tags/v$new" >/dev/null; then
  echo "error: tag v$new already exists" >&2
  exit 1
fi

echo "Releasing v$new (was v$current)"

# Bump the single source of truth; version.h is generated from it at build time.
sed -i.bak -E "s/^([[:space:]]*VERSION[[:space:]]+)[0-9]+\.[0-9]+\.[0-9]+/\1$new/" CMakeLists.txt
rm -f CMakeLists.txt.bak

git add CMakeLists.txt
git commit -m "chore: release v$new"
git tag -a "v$new" -m "decart-cpp v$new"

git push origin main
git push origin "v$new"

echo "Pushed v$new — the Release workflow will publish the GitHub release."
