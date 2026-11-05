# Working on MNGL

Read `docs/design/api-style.md` before changing public APIs. MNGL is a C++23 library:
common tasks should stay short, advanced control should be opt-in, and SDL/GPU access
must remain available. Prefer the current snake_case APIs in code and examples.

Use `.clang-format` for owned C++ sources and `.editorconfig` for text files. Do not
format or refactor bundled sources under `include/`, `third_party/`, or `MNGL/imgui/`.
Do not commit generated build output, amalgamated copies of headers, or local logs.

Public API changes must update guides and the module map. Retain compatibility
aliases/wrappers where they do not compromise ownership or error semantics; explain
source migrations in `docs/design/migration.md`. Validate API and behavior changes
with the CMake build and relevant CTest targets, and documentation with MkDocs strict.
