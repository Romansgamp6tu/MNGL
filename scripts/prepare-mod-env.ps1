[CmdletBinding()]
param(
	[string]$BuildDirectory = 'build-mods',
	[ValidateSet('Debug', 'Release')]
	[string]$Configuration = 'Release',
	[switch]$RunTests
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = if ([IO.Path]::IsPathRooted($BuildDirectory)) {
	[IO.Path]::GetFullPath($BuildDirectory)
} else {
	[IO.Path]::GetFullPath((Join-Path $repo $BuildDirectory))
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
	$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (Test-Path -LiteralPath $vswhere) {
		$vsInstall = & $vswhere -latest -products '*' -property installationPath
		if ($vsInstall) {
			$bundledCMake = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
			if (Test-Path -LiteralPath $bundledCMake) {
			$cmakeCommand = [pscustomobject]@{ Path = $bundledCMake; Source = $bundledCMake }
			}
		}
	}
}

if (-not $cmakeCommand) {
	throw 'cmake was not found on PATH or in the Visual Studio installation. Install CMake 3.24 or newer.'
}

$cmakeExecutable = if ($cmakeCommand.Path) { $cmakeCommand.Path } else { $cmakeCommand.Source }
$cmakeBin = Split-Path -Parent $cmakeExecutable
$ctestExecutable = Join-Path $cmakeBin 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctestExecutable)) {
	$ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
	if ($ctestCommand) {
		$ctestExecutable = if ($ctestCommand.Path) { $ctestCommand.Path } else { $ctestCommand.Source }
	}
}
if (-not (Test-Path -LiteralPath $ctestExecutable)) {
	throw 'ctest was not found on PATH. Install CMake 3.24 or newer and reopen the shell.'
}

function Invoke-NativeTool {
	param(
		[Parameter(Mandatory = $true)][string]$Executable,
		[Parameter(Mandatory = $true)][string[]]$Arguments
	)

	& $Executable @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "$Executable failed with exit code $LASTEXITCODE."
	}
}

Invoke-NativeTool $cmakeExecutable @(
	'-S', (Join-Path $repo 'examples/mods'),
	'-B', $build,
	'-A', 'x64',
	("-DBUILD_TESTING=$($RunTests.IsPresent.ToString().ToUpperInvariant())"),
	'-DMNGL_BUILD_MOD_EXAMPLE=ON'
)

$targets = @('mngl_mod_host', 'mngl_example_tree_mod')
if ($RunTests) {
	$targets += 'mngl_mod_integration_test'
}

$buildArguments = @(
	'--build', $build,
	'--config', $Configuration,
	'--target'
) + $targets + @('--parallel', '1', '--', '/verbosity:minimal')
Invoke-NativeTool $cmakeExecutable $buildArguments

if ($RunTests) {
	Invoke-NativeTool $ctestExecutable @(
		'--test-dir', $build,
		'-C', $Configuration,
		'--output-on-failure'
	)
}

Write-Host "Mod environment is ready in $build."
