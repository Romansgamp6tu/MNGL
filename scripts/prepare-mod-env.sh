#!/usr/bin/env bash
set -euo pipefail

repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-build-mods}"
build_type="${2:-Release}"
run_tests="${3:-}"

if [[ "$build_dir" != /* ]]; then
	build_dir="$repo/$build_dir"
fi

for tool in cmake ctest ninja; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		printf 'Missing prerequisite: %s\n' "$tool" >&2
		exit 1
	fi
done

case "$build_type" in
	Debug|Release) ;;
	*)
		printf 'Build type must be Debug or Release; got: %s\n' "$build_type" >&2
		exit 1
		;;
esac

if [[ -n "$run_tests" && "$run_tests" != "--test" ]]; then
	printf 'Optional third argument must be --test; got: %s\n' "$run_tests" >&2
	exit 1
fi

build_testing=OFF
targets=(mngl_mod_host mngl_example_tree_mod)
if [[ "$run_tests" == "--test" ]]; then
	build_testing=ON
	targets+=(mngl_mod_integration_test)
fi

cmake -S "$repo/examples/mods" -B "$build_dir" -G Ninja \
	-DCMAKE_BUILD_TYPE="$build_type" \
	-DBUILD_TESTING="$build_testing" \
	-DMNGL_BUILD_MOD_EXAMPLE=ON

cmake --build "$build_dir" \
	--target "${targets[@]}" \
	--parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

if [[ "$run_tests" == "--test" ]]; then
	ctest --test-dir "$build_dir" --output-on-failure
fi

printf 'Mod environment is ready in %s.\n' "$build_dir"
