[CmdletBinding()]
param(
	[ValidateSet('Debug', 'Release')]
	[string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$solution = Join-Path $repo 'MNGL.slnx'
if (-not (Test-Path -LiteralPath $solution)) {
	throw "MNGL solution was not found: $solution"
}

# vswhere is installed under Program Files (x86), even for 64-bit Visual Studio.
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) `
	'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
	throw "Visual Studio locator was not found: $vswhere"
}

$vsInstall = & $vswhere -latest -products '*' `
	-requires Microsoft.Component.MSBuild -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vsInstall) {
	throw 'vswhere could not find a Visual Studio installation with MSBuild.'
}
$vsInstall = $vsInstall.Trim()

$msbuildCandidates = @(
	'MSBuild\Current\Bin\amd64\MSBuild.exe',
	'MSBuild\17.0\Bin\amd64\MSBuild.exe',
	'MSBuild\16.0\Bin\amd64\MSBuild.exe',
	'MSBuild\15.0\Bin\amd64\MSBuild.exe'
)
$msbuild = $null
foreach ($relativePath in $msbuildCandidates) {
	$candidate = Join-Path $vsInstall $relativePath
	if (Test-Path -LiteralPath $candidate) {
		$msbuild = $candidate
		break
	}
}
if (-not $msbuild) {
	throw "64-bit MSBuild was not found in Visual Studio at '$vsInstall'."
}

Write-Host "Building the tree practicum with $msbuild"
& $msbuild $solution /m:1 `
	"/p:Configuration=$Configuration" `
	'/p:Platform=x64' `
	'/verbosity:minimal'
if ($LASTEXITCODE -ne 0) {
	throw "MSBuild failed with exit code $LASTEXITCODE."
}

Write-Host "Practicum build succeeded ($Configuration|x64)."
