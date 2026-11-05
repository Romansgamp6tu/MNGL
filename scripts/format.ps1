param(
    [switch]$Check,
    [string]$ClangFormat = "clang-format"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!(Get-Command $ClangFormat -ErrorAction SilentlyContinue)) {
    $bundledFormatter = Join-Path $env:ProgramFiles 'LLVM\bin\clang-format.exe'
    if (Test-Path -LiteralPath $bundledFormatter) { $ClangFormat = $bundledFormatter }
    else { throw 'Install clang-format or pass -ClangFormat with its path.' }
}
$files = foreach ($folder in @('MNGL', 'examples', 'tests')) {
    Get-ChildItem -LiteralPath (Join-Path $projectRoot $folder) -File -Recurse |
        Where-Object {
            $_.Extension -in '.hpp', '.h', '.cpp' -and
            $_.FullName -notlike (Join-Path $projectRoot 'MNGL\imgui\*')
        } | Select-Object -ExpandProperty FullName
}
$formatArguments = @('--style=file')
if ($Check) { $formatArguments += @('--dry-run', '--Werror') }
else { $formatArguments += '-i' }
& $ClangFormat @formatArguments @files
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
