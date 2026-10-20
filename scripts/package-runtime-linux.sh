#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${PACKAGE_VERSION:-}" ]]; then
	printf '%s\n' 'PACKAGE_VERSION must be set by the workflow.' >&2
	exit 1
fi

for tool in cmake ninja git pkg-config bison patchelf readelf; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		printf 'Missing runner prerequisite: %s\n' "$tool" >&2
		exit 1
	fi
done

repo=$(pwd)
work="$repo/build-runtime"
stage="$work/stage"
mngl_build="$work/mngl"
glslang_build="$work/glslang"
spirv_build="$work/spirv-cross"
spirv_prefix="$work/spirv-cross-prefix"
shadercross_source="$repo/upstream/SDL_shadercross"
shadercross_build="$work/shadercross"
shadercross_prefix="$work/shadercross-prefix"
mkdir -p "$work"
rm -rf "$stage" "$mngl_build" "$glslang_build" "$spirv_build" "$spirv_prefix" "$shadercross_build" "$shadercross_prefix"
mkdir -p "$stage/bin" "$stage/lib"

# Build only the runtime library; the runtime archive does not contain the demo.
cmake -S "$repo" -B "$mngl_build" -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build "$mngl_build" --target mngl_imgui --parallel 4
mngl_library=$(find "$mngl_build" -maxdepth 2 -type f -name 'libmngl_imgui.so*' -print -quit)
if [[ -z "$mngl_library" ]]; then
	printf '%s\n' 'libmngl_imgui.so was not produced.' >&2
	exit 1
fi
cp -a "$mngl_library" "$stage/lib/"

cmake -S "$repo/upstream/glslang" -B "$glslang_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release -DENABLE_GLSLANG_BINARIES=ON
cmake --build "$glslang_build" --target glslang-standalone --parallel 4
glslang_exe=$(find -L "$glslang_build" -type f -name glslangValidator -perm -u+x -print -quit)
if [[ -z "$glslang_exe" ]]; then
	printf '%s\n' 'glslangValidator was not produced.' >&2
	exit 1
fi
install -m 0755 "$glslang_exe" "$stage/bin/glslangValidator"

# SDL_shadercross ships a pinned helper script that fetches checksum-verified
# DXC runtime binaries. Build SPIRV-Cross separately as in its upstream CI.
cmake -DCMAKE_SYSTEM_NAME=Linux -P "$shadercross_source/build-scripts/download-prebuilt-DirectXShaderCompiler.cmake"
cmake -S "$shadercross_source/external/SPIRV-Cross" -B "$spirv_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DSPIRV_CROSS_SHARED=ON \
	-DSPIRV_CROSS_STATIC=OFF \
	-DSPIRV_CROSS_CLI=OFF \
	-DSPIRV_CROSS_ENABLE_TESTS=OFF \
	-DCMAKE_INSTALL_PREFIX="$spirv_prefix"
cmake --build "$spirv_build" --parallel 4
cmake --install "$spirv_build"

dxc_root="$shadercross_source/external/DirectXShaderCompiler-binaries/linux"
export spirv_cross_c_shared_ROOT="$spirv_prefix:$spirv_prefix/share/spirv_cross_c_shared"
cmake -S "$shadercross_source" -B "$shadercross_build" -GNinja \
	-DCMAKE_BUILD_TYPE=Release \
	-DSDLSHADERCROSS_DXC=ON \
	-DSDLSHADERCROSS_SHARED=ON \
	-DSDLSHADERCROSS_STATIC=OFF \
	-DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON \
	-DSDLSHADERCROSS_VENDORED=OFF \
	-DSDLSHADERCROSS_CLI=ON \
	-DSDLSHADERCROSS_INSTALL=ON \
	-DSDLSHADERCROSS_INSTALL_RUNTIME=ON \
	-DDirectXShaderCompiler_ROOT="$dxc_root" \
	-DCMAKE_INSTALL_PREFIX="$shadercross_prefix"
cmake --build "$shadercross_build" --parallel 4
cmake --install "$shadercross_build"

install -m 0755 "$shadercross_prefix/bin/shadercross" "$stage/bin/shadercross"
if [[ ! -x "$stage/bin/shadercross" ]]; then
	printf '%s\n' 'shadercross was not installed.' >&2
	exit 1
fi
find "$shadercross_prefix/lib" -maxdepth 1 \( -type f -o -type l \) -name '*.so*' \
	-exec cp -a -t "$stage/lib" -- {} +
cp -a "$repo/MNGL/shaders" "$stage/shaders"

# Copy the non-system ELF dependency closure required by the package's shader
# tools and MNGL's SDL3/SDL3_image/FreeType runtime. glibc, display/audio stacks,
# Vulkan loader, and GPU drivers remain host-provided dependencies.
is_host_runtime() {
	case "$(basename "$1")" in
		libc.so*|ld-linux*.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|libresolv.so*|libutil.so*|libnss_*.so*|libanl.so*|libvulkan.so*|libGL.so*|libGLX.so*|libEGL.so*|libgbm.so*|libdrm.so*|libX*.so*|libxcb*.so*|libwayland*.so*|libxkbcommon*.so*|libudev.so*|libasound.so*|libpulse*.so*|libpipewire*.so*)
			return 0
			;;
		*)
			return 1
			;;
	esac
}

declare -A visited=()
queue=(
	"$glslang_exe"
	"$shadercross_prefix/bin/shadercross"
	"$mngl_library"
)
while ((${#queue[@]})); do
	current=${queue[0]}
	queue=("${queue[@]:1}")
	[[ -e "$current" ]] || continue
	current=$(readlink -f "$current")
	[[ -n "${visited[$current]:-}" ]] && continue
	visited[$current]=1

	ldd_output=$(LD_LIBRARY_PATH="$stage/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ldd "$current" 2>&1 || true)
	if grep -q 'not found' <<<"$ldd_output"; then
		printf 'Unresolved dependency while packaging %s:\n%s\n' "$current" "$ldd_output" >&2
		exit 1
	fi
	while IFS= read -r dependency; do
		[[ -e "$dependency" ]] || continue
		dependency_link=$dependency
		dependency_name=$(basename "$dependency_link")
		if [[ -e "$stage/lib/$dependency_name" ]]; then
			dependency_link="$stage/lib/$dependency_name"
		fi
		if is_host_runtime "$dependency_link"; then
			continue
		fi
		dependency=$(readlink -f "$dependency_link")
		queue+=("$dependency")
		dependency_name=$(basename "$dependency_link")
		resolved_name=$(basename "$dependency")
		[[ -e "$stage/lib/$resolved_name" ]] || cp -a "$dependency" "$stage/lib/"
		if [[ "$dependency_name" != "$resolved_name" && ! -e "$stage/lib/$dependency_name" ]]; then
			ln -s "$resolved_name" "$stage/lib/$dependency_name"
		fi
	done < <(awk '
		/=> \/.* \(/{print $3}
		/^\// {print $1}
	' <<<"$ldd_output" | sort -u)
done

# Keep bundled shared objects relocatable when their original install prefix is
# not the same directory as the extracted runtime package.
while IFS= read -r -d '' library; do
	if ! readelf -d "$library" 2>/dev/null | grep -q '(RUNPATH).*\$ORIGIN'; then
		patchelf --set-rpath '$ORIGIN' "$library"
	fi
done < <(find "$stage/lib" -maxdepth 1 -type f -name '*.so*' -print0)

cp -f "$repo/MNGL/imgui/LICENSE.txt" "$stage/MNGL-ImGui-LICENSE.txt"
cat > "$stage/runtime-manifest.txt" <<EOF
MNGL runtime package $PACKAGE_VERSION
Platform: Linux x86_64
Build host: $(. /etc/os-release && printf '%s %s' "$NAME" "$VERSION_ID")
MNGL source: ${GITHUB_SHA:-local}
glslang: $(git -C "$repo/upstream/glslang" rev-parse HEAD)
SDL_shadercross: $(git -C "$shadercross_source" rev-parse HEAD)
SDL3: $(pkg-config --modversion sdl3 2>/dev/null || printf 'CMake package on runner')

The archive contains runtime libraries, shader tools, and built-in shaders only. It does not include the demo, development archive, headers, or import/static libraries.
glibc, the Vulkan loader, GPU drivers, and the host display/audio stack are system dependencies and are not bundled.
EOF

archive="$repo/mngl-runtime-linux-x64-$PACKAGE_VERSION.tar.gz"
tar -C "$stage" -czf "$archive" .

