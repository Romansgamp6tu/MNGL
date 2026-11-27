#!/usr/bin/env bash
set -euo pipefail

for variable in PACKAGE_VERSION SDL_PREFIX SDL_VERSION SDL_IMAGE_VERSION; do
	if [[ -z "${!variable:-}" ]]; then
		printf '%s must be set by the workflow.\n' "$variable" >&2
		exit 1
	fi
done

for tool in cmake ninja git bison flex dylibbundler; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		printf 'Missing runner prerequisite: %s\n' "$tool" >&2
		exit 1
	fi
done

repo=$(pwd)
work="$repo/build-runtime-macos"
stage="$work/stage"
mngl_build="$work/mngl"
glslang_build="$work/glslang"
spirv_build="$work/spirv-cross"
spirv_prefix="$work/spirv-cross-prefix"
shadercross_source="$repo/upstream/SDL_shadercross"
shadercross_build="$work/shadercross"
shadercross_prefix="$work/shadercross-prefix"
metal_shader_dir="$work/metal-shaders"
brew_prefix=$(brew --prefix)
cmake_prefix="$SDL_PREFIX;$brew_prefix"

rm -rf "$work"
mkdir -p "$stage/bin" "$stage/lib" "$metal_shader_dir"

# Build the library and verify its built-in HLSL -> SPIR-V -> MSL path.
cmake -S "$repo" -B "$mngl_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
	-DCMAKE_PREFIX_PATH="$cmake_prefix" \
	-DMNGL_METAL_SHADER_ASSET_DIR="$metal_shader_dir" \
	-DMNGL_BUILD_MACOS_DEVEL_PACKAGE=ON \
	-DMNGL_PACKAGE_BRANCH=main \
	-DBUILD_TESTING=ON
cmake --build "$mngl_build" --target \
	mngl_audio_runtime_smoke mngl_gpu_runtime_smoke mngl_shader_compile_smoke \
	mngl_metal_shader_assets_smoke mngl_svg_runtime_test --parallel 4

mngl_library=$(find "$mngl_build" -type f -name 'libmngl_imgui.dylib' -print -quit)
mixer_library=$(find "$mngl_build" \( -type f -o -type l \) -name 'libSDL3_mixer*.dylib' -print -quit)
audio_smoke=$(find "$mngl_build" -type f -name 'mngl_audio_runtime_smoke' -print -quit)
gpu_smoke=$(find "$mngl_build" -type f -name 'mngl_gpu_runtime_smoke' -print -quit)
shader_smoke=$(find "$mngl_build" -type f -name 'mngl_shader_compile_smoke' -print -quit)
metal_smoke=$(find "$mngl_build" -type f -name 'mngl_metal_shader_assets_smoke' -print -quit)
svg_runtime_smoke=$(find "$mngl_build" -type f -name 'mngl_svg_runtime_test' -print -quit)
if [[ -z "$mngl_library" || -z "$mixer_library" || -z "$audio_smoke" || -z "$gpu_smoke" || -z "$shader_smoke" || -z "$metal_smoke" || -z "$svg_runtime_smoke" ]]; then
	printf '%s\n' 'MNGL macOS runtime libraries or smoke-test executables were not produced.' >&2
	exit 1
fi
cp -p "$mngl_library" "$stage/lib/"
if [[ ! -e "$stage/lib/$(basename "$mixer_library")" ]]; then
	printf '%s\n' 'SDL3_mixer runtime library was not installed into the package.' >&2
	exit 1
fi

cmake -S "$repo/upstream/glslang" -B "$glslang_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
	-DENABLE_GLSLANG_BINARIES=ON
cmake --build "$glslang_build" --target glslang-standalone --parallel 4
glslang_exe=$(find -L "$glslang_build" -type f -name glslangValidator -print -quit)
if [[ -z "$glslang_exe" ]]; then
	printf '%s\n' 'glslangValidator was not produced.' >&2
	exit 1
fi
install -m 0755 "$glslang_exe" "$stage/bin/glslangValidator"

# macOS has no upstream DXC binary in the pinned shadercross checkout. MNGL's
# macOS shader loader therefore translates HLSL through glslang/SPIR-V.
cmake -S "$shadercross_source/external/SPIRV-Cross" -B "$spirv_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
	-DSPIRV_CROSS_SHARED=ON \
	-DSPIRV_CROSS_STATIC=OFF \
	-DSPIRV_CROSS_CLI=OFF \
	-DSPIRV_CROSS_ENABLE_TESTS=OFF \
	-DCMAKE_INSTALL_PREFIX="$spirv_prefix"
cmake --build "$spirv_build" --parallel 4
cmake --install "$spirv_build"

cmake -S "$shadercross_source" -B "$shadercross_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
	-DCMAKE_PREFIX_PATH="$SDL_PREFIX;$spirv_prefix;$brew_prefix" \
	-Dspirv_cross_c_shared_ROOT="$spirv_prefix" \
	-DSDLSHADERCROSS_DXC=OFF \
	-DSDLSHADERCROSS_SHARED=ON \
	-DSDLSHADERCROSS_STATIC=OFF \
	-DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON \
	-DSDLSHADERCROSS_VENDORED=OFF \
	-DSDLSHADERCROSS_CLI=ON \
	-DSDLSHADERCROSS_INSTALL=ON \
	-DSDLSHADERCROSS_INSTALL_RUNTIME=OFF \
	-DCMAKE_INSTALL_PREFIX="$shadercross_prefix"
cmake --build "$shadercross_build" --parallel 4
cmake --install "$shadercross_build"

install -m 0755 "$shadercross_prefix/bin/shadercross" "$stage/bin/shadercross"
find "$shadercross_prefix/lib" \( -type f -o -type l \) -name '*.dylib*' \
	-exec cp -p {} "$stage/lib/" \;
export DYLD_LIBRARY_PATH="$stage/lib:$SDL_PREFIX/lib:$spirv_prefix/lib:$brew_prefix/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

# SDL_shadercross emits MSL source for offline use; SDL_GPU compiles it through
# Metal when creating the pipeline. Keep compiler tools out of the rendering path.
generate_msl_shader() {
	local source_name=$1
	local glsl_stage=$2
	local shadercross_stage=$3
	local source="$repo/MNGL/shaders/$source_name.hlsl"
	local spirv="$metal_shader_dir/$source_name.spv"
	local msl="$metal_shader_dir/$source_name.msl"
	"$stage/bin/glslangValidator" -D --hlsl-offsets --hlsl-iomap -V -S "$glsl_stage" \
		-e main -o "$spirv" "$source"
	"$stage/bin/shadercross" "$spirv" -s SPIRV -d MSL -t "$shadercross_stage" \
		-o "$msl" -e main
	if [[ ! -s "$msl" ]]; then
		printf 'No MSL sidecar was produced for %s.\n' "$source_name" >&2
		exit 1
	fi
	rm "$spirv"
}
generate_msl_shader batcher2d.vert vert vertex
generate_msl_shader batcher2d.frag frag fragment
generate_msl_shader svg_gradient.frag frag fragment
generate_msl_shader svg_filter.vert vert vertex
generate_msl_shader svg_filter.frag frag fragment

# Re-run staging now that the offline sidecars exist. This verifies the same
# application-directory path used by CMake development executables.
cmake --build "$mngl_build" --target mngl_demo mngl_metal_shader_assets_smoke \
	mngl_svg_runtime_test --parallel 4
cmake --install "$mngl_build" --prefix "$stage" --component library

cp -R "$repo/MNGL/shaders" "$stage/shaders"
cp "$metal_shader_dir"/*.msl "$stage/shaders/"
mkdir -p "$stage/licenses"
cp "$repo/upstream/glslang/LICENSE.txt" "$stage/licenses/glslang-LICENSE.txt"
cp "$shadercross_source/LICENSE.txt" "$stage/licenses/SDL_shadercross-LICENSE.txt"
cp "$shadercross_source/external/SPIRV-Cross/LICENSE" "$stage/licenses/SPIRV-Cross-LICENSE.txt"

# Copy the dependency closure and rewrite load commands to the package layout.
dylib_args=(-b -cd -d "build-runtime-macos/stage/lib" -p '@executable_path/../lib')
for search_path in \
	"$SDL_PREFIX/lib" \
	"$brew_prefix/lib" \
	"$spirv_prefix/lib" \
	"$shadercross_prefix/lib"; do
	dylib_args+=(-s "$search_path")
done
for binary in \
	"$stage/bin/shadercross" \
	"$stage/bin/glslangValidator" \
	"$stage/lib/$(basename "$mngl_library")" \
	"$stage/lib/$(basename "$mixer_library")" \
	"$audio_smoke" \
	"$gpu_smoke" \
	"$shader_smoke" \
	"$metal_smoke" \
	"$svg_runtime_smoke"; do
	dylib_args+=(-x "$binary")
done
dylibbundler "${dylib_args[@]}"

install -m 0755 "$audio_smoke" "$stage/bin/mngl_audio_runtime_smoke"
install -m 0755 "$gpu_smoke" "$stage/bin/mngl_gpu_runtime_smoke"
install -m 0755 "$shader_smoke" "$stage/bin/mngl_shader_compile_smoke"
install -m 0755 "$metal_smoke" "$stage/bin/mngl_metal_shader_assets_smoke"
install -m 0755 "$svg_runtime_smoke" "$stage/bin/mngl_svg_runtime_test"
export PATH="$stage/bin:$PATH"
export DYLD_LIBRARY_PATH="$stage/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
SDL_AUDIODRIVER=dummy "$stage/bin/mngl_audio_runtime_smoke"
"$stage/bin/mngl_shader_compile_smoke"
# The MSL sidecar smoke uses deliberately invalid compiler paths and cannot see
# the packaged compiler tools through PATH. Run it in both CMake staging and the
# installed application layout.
PATH=/usr/bin:/bin "$metal_smoke"
PATH=/usr/bin:/bin "$stage/bin/mngl_metal_shader_assets_smoke"
set +e
PATH=/usr/bin:/bin SDL_GPU_DRIVER=metal "$svg_runtime_smoke"
svg_development_status=$?
PATH=/usr/bin:/bin SDL_GPU_DRIVER=metal "$stage/bin/mngl_svg_runtime_test"
svg_installed_status=$?
set -e
for status in "$svg_development_status" "$svg_installed_status"; do
	if [[ "$status" -ne 0 && "$status" -ne 77 ]]; then
		printf 'Metal SVG rendering smoke failed with exit code %s.\n' "$status" >&2
		exit "$status"
	fi
done
svg_metal_smoke_status=passed
if [[ "$svg_development_status" -eq 77 || "$svg_installed_status" -eq 77 ]]; then
	svg_metal_smoke_status='skipped; runner has no usable Metal GPU backend'
fi
set +e
"$stage/bin/mngl_gpu_runtime_smoke"
gpu_smoke_status=$?
set -e
if [[ "$gpu_smoke_status" -ne 0 && "$gpu_smoke_status" -ne 77 ]]; then
	printf 'Packaged GPU smoke test failed with exit code %s.\n' "$gpu_smoke_status" >&2
	exit "$gpu_smoke_status"
fi
rm "$stage/bin/mngl_audio_runtime_smoke" \
	"$stage/bin/mngl_gpu_runtime_smoke" \
	"$stage/bin/mngl_shader_compile_smoke" \
	"$stage/bin/mngl_metal_shader_assets_smoke" \
	"$stage/bin/mngl_svg_runtime_test"

cp "$repo/MNGL/imgui/LICENSE.txt" "$stage/MNGL-ImGui-LICENSE.txt"
cat > "$stage/runtime-manifest.txt" <<EOF
MNGL runtime package $PACKAGE_VERSION
Platform: macOS $(uname -m)
Minimum macOS version: 15.0
MNGL source: ${GITHUB_SHA:-local}
glslang: $(git -C "$repo/upstream/glslang" rev-parse HEAD)
SDL_shadercross: $(git -C "$shadercross_source" rev-parse HEAD; printf ' (DXC disabled; HLSL compiles through glslang and SPIR-V)')
SDL3: $SDL_VERSION
SDL3_image: $SDL_IMAGE_VERSION
SDL3_mixer: bundled release-3.2.4 with built-in decoders
SDL3_mixer and built-in decoder license notices: included
Audio runtime smoke: passed
HLSL-to-MSL shader smoke: passed
Built-in Metal 2D/SVG shader sidecars: generated at package time and loaded without compiler tools on PATH
Metal SVG rendering smoke: $svg_metal_smoke_status
GPU device smoke: $([[ "$gpu_smoke_status" -eq 0 ]] && printf 'passed' || printf 'skipped; no usable video/GPU backend or adapter on runner')

The archive contains runtime libraries, shader tools, built-in shaders, and licenses. It does not include the demo, development archive, or headers.
Apple system frameworks and GPU drivers remain host-provided dependencies.
EOF

case "$(uname -m)" in
	arm64) package_arch=arm64 ;;
	x86_64) package_arch=x64 ;;
	*) printf 'Unsupported macOS package architecture: %s\n' "$(uname -m)" >&2; exit 1 ;;
esac
archive="$repo/mngl-runtime-macos-${package_arch}-$PACKAGE_VERSION.tar.gz"
tar -C "$stage" -czf "$archive" .
