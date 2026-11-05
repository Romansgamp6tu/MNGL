#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
case "${1:-}" in
    --check) set -- --dry-run --Werror ;;
    '') set -- -i ;;
    *) echo 'Usage: format.sh [--check]' >&2; exit 2 ;;
esac
rg --files MNGL examples tests -g '*.hpp' -g '*.h' -g '*.cpp' -g '!MNGL/imgui/**' -0 |
    xargs -0 clang-format --style=file "$@"
