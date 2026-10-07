# MNGL

MNGL is a C++23 header-oriented wrapper around SDL3 for windows, events/input,
surfaces, and SDL's GPU API. At this stage the repository contains a demo
executable, not an installed/exported CMake library target.

## What is in this repository

- `MNGL/` — MNGL headers and the `MNGL.cpp` demo entry point.
- `include/SDL3/`, `include/SDL3_image/`, `include/glm/` — vendored headers for
  SDL3, SDL3_image, and GLM.
- `libs/` — currently bundled x64 Windows libraries:
  `SDL3.lib`, `SDL3.dll`, `SDL3_image.lib`, `SDL3_image.dll`,
  `SDL3_test.lib`, `freetype.lib`, and `freetype.dll`.
- `CMakeLists.txt` — builds `MNGL/MNGL.cpp` as `mngl_demo`. It does not install
  MNGL or create a reusable `MNGL::MNGL` target.

`MNGL.exe` (Visual Studio) and `mngl_demo.exe` (CMake) are build outputs, not
dependencies. `MNGL.pdb` and `mngl_demo.pdb` are optional debug symbols.

## Build requirements

- CMake 3.24 or newer.
- A C++23 compiler with a standard library that supports the C++23 facilities
  used by MNGL, including `std::format`.
- SDL3, SDL3_image, and FreeType libraries matching the target OS and
  architecture.

The SDL3, SDL3_image, and GLM headers are in `include/`; consumers still need
matching SDL3/SDL3_image/FreeType link libraries. The `MNGL/utilities.hpp`
header contains MSVC linker pragmas for `SDL3.lib`, `SDL3_image.lib`,
`SDL3_test.lib`, and `freetype.lib`. The current `libs/` binaries are x64 only;
they cannot be used to link an x86 build.

### Windows

Install Visual Studio with C++ desktop development tools and CMake. From the
repository root, configure for x64 and build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

If CMake does not select the intended Visual Studio installation, pass its
generator explicitly, for example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

The CMake build links against `libs/` and copies `SDL3.dll`,
`SDL3_image.dll`, and `freetype.dll` beside `mngl_demo.exe`. The Visual Studio
project does not currently copy runtime DLLs automatically; when using it
directly, make sure the required DLLs are beside the resulting executable or on
`PATH`.

All native libraries and DLLs must match the executable architecture. In
particular, the currently bundled `.lib` files are x64, so x86 compilation may
compile the source but cannot successfully link against them.

### macOS

MNGL uses SDL3's Metal backend by default when SDL selects it. MoltenVK is not
needed for the native Metal path. It is only needed if the application
specifically uses SDL's Vulkan backend on macOS.

With Homebrew and Xcode Command Line Tools installed:

```sh
brew install cmake sdl3 sdl3_image freetype
cmake -S . -B build-macos -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build-macos --parallel
```

The current demo only exercises windowing and input. It does not create an SDL
GPU device, compile shaders, or verify the Metal/Vulkan rendering path.

### Linux and other non-MSVC builds

Install CMake, a C++23-capable compiler, and development packages for SDL3,
SDL3_image, and FreeType. CMake uses `find_package` for these dependencies, so
their CMake package files must be discoverable (set `CMAKE_PREFIX_PATH` if they
are installed in a nonstandard location):

```sh
cmake -S . -B build
cmake --build build --parallel
```

For the SDL GPU Vulkan backend, the machine also needs a working Vulkan loader
and GPU driver. These are runtime/backend requirements, not dependencies for
building the window/input-only demo.

## Input

Keyboard, mouse, gamepad, and event handling are provided through SDL3; there
is no separate input package to install. The demo drives input and updates by
calling `mngl::Input::continue_work()` in its main loop. The SDL subsystems
initialized by default are controlled by `APP_FLAGS` in
`MNGL/utilities.hpp`.

## Optional shader-compilation tools

Shader compilation is separate from building the demo.
`MNGL/shader_loader.hpp` launches external command-line programs:

- **HLSL input:** `shadercross` is required.
- **GLSL input targeting SPIR-V:** `glslangValidator` is required.
- **GLSL input targeting DXIL, MSL, or another shadercross-supported format:**
  both `glslangValidator` and `shadercross` are required.

The loader searches for `shadercross` and `glslangValidator` by executable name
by default. Put them on `PATH`, or set
`shader_compile_options::shadercross_executable` and
`shader_compile_options::glslang_validator_executable` to explicit paths. If
the validator binary is named `glslang.exe`, configure that exact executable
name/path (or rename it to `glslangValidator.exe`).

For the Windows shader tool set, the executable names/files are:

- `shadercross.exe` — HLSL compilation and shader-format conversion.
- `glslang.exe` — the `glslangValidator` tool used for GLSL compilation. The
  loader's default executable name is `glslangValidator`; either rename the
  binary to `glslangValidator.exe` or set
  `glslang_validator_executable` to `glslang.exe`.

The shadercross package may also require these runtime DLLs beside
`shadercross.exe` or on `PATH`, depending on the package/build used:

- `SDL3_shadercross.dll`
- `dxcompiler.dll` and `dxil.dll` (DirectX Shader Compiler runtime)
- `spirv-cross-c-shared.dll`

Use binaries for the same architecture as the application and keep the
shadercross executable and its companion DLLs from compatible builds. The
CMake target does not download or copy these optional shader tools.

For a Windows x64 application that compiles shaders at runtime, a typical
application directory needs the following files:

```text
mngl_demo.exe
SDL3.dll
SDL3_image.dll
freetype.dll
shadercross.exe
SDL3_shadercross.dll
glslang.exe                 # or glslangValidator.exe
dxcompiler.dll
dxil.dll
spirv-cross-c-shared.dll
```

The executable is built in this directory, and CMake stages `SDL3.dll`,
`SDL3_image.dll`, and `freetype.dll` beside it. Shader tools and their DLLs
must be added separately or exposed through `PATH`. Some upstream binary
packages have additional runtime DLL dependencies, so use the complete matching
package rather than copying only the files named here.

### Shader and graphics backend notes

- `glslangValidator` compiles GLSL to SPIR-V. shadercross is used for HLSL and
  for converting SPIR-V to formats such as DXIL or MSL.
- On Windows, DXIL shader compilation needs a shadercross build with its DXC
  runtime dependencies available.
- On macOS, Metal is the native SDL GPU backend and MNGL's default shader
  format is MSL. MoltenVK is a Vulkan-over-Metal implementation, not a shader
  compiler; it is only relevant when selecting the Vulkan backend.
- On Linux, Vulkan is a common SDL GPU backend and SPIR-V is MNGL's default
  shader format.
- Shader tools are not needed to compile `mngl_demo` unless the application
  actually uses the shader loader.

## Dependency upstreams and downloads

These are upstream project/download pages for obtaining compatible headers,
libraries, tools, or SDK packages:

| Dependency | Used for | Upstream / downloads |
|---|---|---|
| SDL3 | Windowing, events, input, GPU API | [SDL repository](https://github.com/libsdl-org/SDL), [releases](https://github.com/libsdl-org/SDL/releases) |
| SDL3_image | Image loading and surface helpers | [SDL_image repository](https://github.com/libsdl-org/SDL_image), [releases](https://github.com/libsdl-org/SDL_image/releases) |
| GLM | Header-only math types | [GLM repository](https://github.com/g-truc/glm), [releases](https://github.com/g-truc/glm/releases) |
| FreeType | Current MSVC link dependency (`freetype.lib`) | [FreeType project](https://freetype.org/), [official source repository](https://gitlab.freedesktop.org/freetype/freetype), [downloads](https://freetype.org/download.html) |
| SDL_shadercross | HLSL and shader-format conversion tool/runtime | [repository](https://github.com/libsdl-org/SDL_shadercross), [releases](https://github.com/libsdl-org/SDL_shadercross/releases) |
| DirectX Shader Compiler (DXC) | DXIL compilation/runtime (`dxcompiler.dll`, `dxil.dll`) | [repository](https://github.com/microsoft/DirectXShaderCompiler), [releases](https://github.com/microsoft/DirectXShaderCompiler/releases) |
| glslang | GLSL-to-SPIR-V compiler (`glslangValidator`) | [repository](https://github.com/KhronosGroup/glslang), [releases](https://github.com/KhronosGroup/glslang/releases) |
| SPIRV-Cross | SPIR-V translation/runtime (`spirv-cross-c-shared.dll`) | [repository](https://github.com/KhronosGroup/SPIRV-Cross), [releases](https://github.com/KhronosGroup/SPIRV-Cross/releases) |
| Vulkan SDK | Vulkan headers, loader/tools, including `glslangValidator` on Windows | [LunarG SDK downloads](https://vulkan.lunarg.com/sdk/home) |
| MoltenVK | Vulkan implementation over Metal, only for macOS Vulkan backend | [Khronos MoltenVK repository](https://github.com/KhronosGroup/MoltenVK) |
| Dear ImGui | Optional UI integration; not currently vendored or directly used by the demo | [repository](https://github.com/ocornut/imgui), [releases](https://github.com/ocornut/imgui/releases) |

The extra shader-tool binaries listed above are not currently tracked in this
repository. The tracked `libs/` directory only contains the SDL3,
SDL3_image, SDL3_test, and FreeType Windows x64 artifacts listed earlier.
The repository does not record the exact release tag, build options, or hashes
for those binary files, so the links above identify their upstream projects
and official download locations, not verified provenance for a particular
local copy. Pin exact release versions and hashes before distributing binaries.
