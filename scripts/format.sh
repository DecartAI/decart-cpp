#!/usr/bin/env bash
# Format all SDK sources in place with clang-format.
set -euo pipefail
cd "$(dirname "$0")/.."

mode="-i"
if [[ "${1:-}" == "--check" ]]; then
  mode="--dry-run --Werror"
fi

find include src examples tests \
  \( -name '*.h' -o -name '*.cpp' \) \
  -print0 | xargs -0 clang-format $mode
echo "clang-format done (${mode})"
