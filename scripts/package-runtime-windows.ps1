$ErrorActionPreference = 'Stop'

if (-not $env:PACKAGE_VERSION) {
	throw 'PACKAGE_VERSION must be set by the workflow.'
}
if (-not $env:SDL_ROOT) {
	throw 'SDL_ROOT is missing; setup-sdl did not provide its install prefix.'
}

$repo = (Get-Location).Path
$work = Join-Path $repo 'build-runtime'
$stage = Join-Path $work 'stage'
$mnglBuild = Join-Path $work 'mngl'
$glslangBuild = Join-Path $work 'glslang'
$spirvBuild = Join-Path $work 'spirv-cross'
$spirvPrefix = Join-Path $work 'spirv-cross-prefix'
$shadercrossBuild = Join-Path $work 'shadercross'
$shadercrossPrefix = Join-Path $work 'shadercross-prefix'
$bin = Join-Path $stage 'bin'

if (Test-Path -LiteralPath $work) {
	Remove-Item -LiteralPath $work -Recurse -Force
}
New-Item -ItemType Directory -Path $bin -Force | Out-Null

cmake -S $repo -B $mnglBuild -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "MNGL runtime configuration failed with exit code $LASTEXITCODE." }
cmake --build $mnglBuild --config Release --target mngl_audio_runtime_smoke --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Audio runtime smoke build failed with exit code $LASTEXITCODE." }
cmake --build $mnglBuild --config Release --target mngl_gpu_runtime_smoke --parallel 4
if ($LASTEXITCODE -ne 0) { throw "GPU runtime smoke build failed with exit code $LASTEXITCODE." }
cmake --install $mnglBuild --config Release --prefix $stage --component library
if ($LASTEXITCODE -ne 0) { throw "SDL3_mixer runtime install failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath (Join-Path $bin 'SDL3_mixer.dll'))) {
	throw 'SDL3_mixer.dll was not installed into the runtime package.'
}

cmake -S (Join-Path $repo 'upstream/glslang') -B $glslangBuild -A x64 -DCMAKE_BUILD_TYPE=Release -DENABLE_GLSLANG_BINARIES=ON
cmake --build $glslangBuild --config Release --target glslang-standalone --parallel 4
$glslangExe = Get-ChildItem -LiteralPath $glslangBuild -Filter glslangValidator.exe -File -Recurse |
	Sort-Object Length -Descending | Select-Object -First 1
if (-not $glslangExe) {
	throw 'glslangValidator.exe was not produced.'
}
Copy-Item -LiteralPath $glslangExe.FullName -Destination $bin

$shadercrossSource = Join-Path $repo 'upstream/SDL_shadercross'
cmake -DCMAKE_SYSTEM_NAME=Windows -P (Join-Path $shadercrossSource 'build-scripts/download-prebuilt-DirectXShaderCompiler.cmake')

cmake -S (Join-Path $shadercrossSource 'external/SPIRV-Cross') -B $spirvBuild -G 'Visual Studio 18 2026' -A x64 `
	-DCMAKE_BUILD_TYPE=Release `
	-DSPIRV_CROSS_SHARED=ON `
	-DSPIRV_CROSS_STATIC=OFF `
	-DSPIRV_CROSS_CLI=OFF `
	-DSPIRV_CROSS_ENABLE_TESTS=OFF `
	"-DCMAKE_INSTALL_PREFIX=$spirvPrefix"
cmake --build $spirvBuild --config Release --parallel 4
cmake --install $spirvBuild --config Release

$dxcRoot = Join-Path $shadercrossSource 'external/DirectXShaderCompiler-binaries/windows'
$env:spirv_cross_c_shared_ROOT = "$spirvPrefix;$spirvPrefix/share/spirv_cross_c_shared"
cmake -S $shadercrossSource -B $shadercrossBuild -G 'Visual Studio 18 2026' -A x64 `
	-DCMAKE_BUILD_TYPE=Release `
	-DSDLSHADERCROSS_DXC=ON `
	-DSDLSHADERCROSS_SHARED=ON `
	-DSDLSHADERCROSS_STATIC=OFF `
	-DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON `
	-DSDLSHADERCROSS_VENDORED=OFF `
	-DSDLSHADERCROSS_CLI=ON `
	-DSDLSHADERCROSS_INSTALL=ON `
	-DSDLSHADERCROSS_INSTALL_RUNTIME=ON `
	"-DCMAKE_PREFIX_PATH=$env:SDL_ROOT" `
	"-DDirectXShaderCompiler_ROOT=$dxcRoot" `
	"-DCMAKE_INSTALL_PREFIX=$shadercrossPrefix"
cmake --build $shadercrossBuild --config Release --parallel 4
cmake --install $shadercrossBuild --config Release

# Include the repository's SDL_image and FreeType runtime DLLs. SDL3.dll from
# setup-sdl is copied afterward so shadercross and MNGL share the same SDL3.
Get-ChildItem -LiteralPath (Join-Path $repo 'libs') -Filter *.dll -File |
	Copy-Item -Destination $bin
$sdlDll = Join-Path $env:SDL_ROOT 'bin/SDL3.dll'
if (-not (Test-Path -LiteralPath $sdlDll)) {
	throw "SDL3.dll was not found in the setup-sdl prefix: $sdlDll"
}
Copy-Item -LiteralPath $sdlDll -Destination $bin -Force
$mnglDll = Get-ChildItem -LiteralPath $mnglBuild -Filter mngl_imgui.dll -File -Recurse |
	Select-Object -First 1
if (-not $mnglDll) {
	throw 'mngl_imgui.dll was not produced.'
}
Copy-Item -LiteralPath $mnglDll.FullName -Destination $bin

foreach ($directory in @((Join-Path $shadercrossPrefix 'bin'), (Join-Path $shadercrossPrefix 'lib'))) {
	if (Test-Path -LiteralPath $directory) {
		Get-ChildItem -LiteralPath $directory -File |
			Where-Object { $_.Extension -in @('.dll', '.exe') } |
			Copy-Item -Destination $bin -Force
	}
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
	throw 'vswhere.exe was not found; cannot stage the app-local MSVC runtime.'
}
$vsPath = & $vswhere -latest -products '*' -property installationPath
$crtRoot = Join-Path $vsPath 'VC/Redist/MSVC'
$crtDirectory = Get-ChildItem -Path (Join-Path $crtRoot '*/x64/Microsoft.VC143.CRT') -Directory |
	Sort-Object FullName -Descending | Select-Object -First 1
if (-not $crtDirectory) {
	throw 'The x64 Microsoft.VC143.CRT redist directory was not found.'
}
Get-ChildItem -LiteralPath $crtDirectory.FullName -Filter *.dll -File |
	Copy-Item -Destination $bin -Force

$smokeDirectory = Join-Path $work 'smoke'
New-Item -ItemType Directory -Path $smokeDirectory -Force | Out-Null
$audioSmoke = Get-ChildItem -LiteralPath $mnglBuild -Filter mngl_audio_runtime_smoke.exe -File -Recurse |
	Select-Object -First 1
if (-not $audioSmoke) {
	throw 'mngl_audio_runtime_smoke.exe was not produced.'
}
Copy-Item -LiteralPath $audioSmoke.FullName -Destination $smokeDirectory
$gpuSmoke = Get-ChildItem -LiteralPath $mnglBuild -Filter mngl_gpu_runtime_smoke.exe -File -Recurse |
	Select-Object -First 1
if (-not $gpuSmoke) {
	throw 'mngl_gpu_runtime_smoke.exe was not produced.'
}
Copy-Item -LiteralPath $gpuSmoke.FullName -Destination $smokeDirectory

$env:PATH = "$bin;$env:PATH"
$env:SDL_AUDIODRIVER = 'dummy'
& (Join-Path $smokeDirectory 'mngl_audio_runtime_smoke.exe')
if ($LASTEXITCODE -ne 0) {
	throw "Packaged audio smoke test failed with exit code $LASTEXITCODE."
}
Remove-Item Env:SDL_AUDIODRIVER -ErrorAction SilentlyContinue

& (Join-Path $smokeDirectory 'mngl_gpu_runtime_smoke.exe')
$gpuSmokeStatus = $LASTEXITCODE
if ($gpuSmokeStatus -notin @(0, 77)) {
	throw "Packaged GPU smoke test failed with exit code $gpuSmokeStatus."
}

Copy-Item -LiteralPath (Join-Path $repo 'MNGL/shaders') -Destination (Join-Path $stage 'shaders') -Recurse
Copy-Item -LiteralPath (Join-Path $repo 'MNGL/imgui/LICENSE.txt') -Destination (Join-Path $stage 'MNGL-ImGui-LICENSE.txt')

$manifest = @"
MNGL runtime package $($env:PACKAGE_VERSION)
Platform: Windows x64
MNGL source: $($env:GITHUB_SHA)
glslang: $(git -C (Join-Path $repo 'upstream/glslang') rev-parse HEAD)
SDL_shadercross: $(git -C (Join-Path $repo 'upstream/SDL_shadercross') rev-parse HEAD)
SDL3 / SDL3_image: setup-sdl versions $($env:SDL_VERSION) / 3.4.8
SDL3_mixer: bundled release-3.2.4 with built-in decoders
SDL3_mixer and built-in decoder license notices: included
MSVC runtime: app-local x64 Microsoft.VC143.CRT
Audio runtime smoke: passed
GPU device smoke: $(if ($gpuSmokeStatus -eq 0) { 'passed' } else { 'skipped; no usable video/GPU backend or adapter on runner' })

This archive contains runtime files and shaders only. It does not include a demo or development headers/import libraries.
"@
Set-Content -LiteralPath (Join-Path $stage 'runtime-manifest.txt') -Value $manifest -Encoding utf8

$archive = Join-Path $repo "mngl-runtime-windows-x64-$($env:PACKAGE_VERSION).zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
