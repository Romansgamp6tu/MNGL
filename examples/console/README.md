# MNGL console API example

This standalone example registers `echo`, `sum`, and `status` and uses
`dispatcher::run_process()` for direct command-line calls and command streams.
The `status` command shows that handlers share state while a stream is running.

## Build

From the repository root:

```sh
cmake -S examples/console -B build-console-example
cmake --build build-console-example --config Release
```

On Windows with a multi-configuration generator, the executable is
`build-console-example/Release/mngl_console_example.exe`. On single-configuration
generators, it is `build-console-example/mngl_console_example`.

## Direct commands

The following commands assume `mngl_console_example` is on `PATH`; in the shell
examples below, `exe` points to the built binary.

```sh
mngl_console_example --help
mngl_console_example echo "Hello world" --upper
mngl_console_example sum 20 22
mngl_console_example status
mngl_console_example echo --help
mngl_console_example sum 20 nope
```

Errors return a non-zero process code. Add `--json` before a command to receive
the JSON response envelope on stdout:

```sh
mngl_console_example --json sum 20 22
mngl_console_example --json sum 20 nope
```

## Bash

```sh
# Git Bash on Windows with a multi-configuration CMake generator:
exe=./build-console-example/Release/mngl_console_example.exe
# On Linux/macOS with a single-configuration generator, use:
# exe=./build-console-example/mngl_console_example
"$exe" --help
"$exe" --json sum 20 22
status=$?
"$exe" sum 20 nope
bad_status=$?
printf 'invalid_argument_exit=%s\n' "$bad_status"
printf 'echo hello\nsum 20 22\nstatus\n:quit\n' | "$exe" --console
stream_status=$?
```

Read the process exit code from `$?` immediately after the invocation or
pipeline.

## PowerShell

```powershell
$exe = '.\build-console-example\Release\mngl_console_example.exe'
& $exe --help
& $exe echo --help
& $exe --json sum 20 22
$exitCode = $LASTEXITCODE
& $exe sum 20 nope
$badExitCode = $LASTEXITCODE
@('echo hello', 'sum 20 22', 'status', ':quit') |
    & $exe --json --console
$streamExitCode = $LASTEXITCODE
```

Read native process exit codes from `$LASTEXITCODE`.
