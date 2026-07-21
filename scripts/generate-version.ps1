[CmdletBinding()]
param(
    [string]$VersionFile,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($VersionFile)) {
    $VersionFile = Join-Path $PSScriptRoot '..\VERSION'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PSScriptRoot '..\build\swit_version.rcinc'
}

$version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
$match = [regex]::Match(
    $version,
    '^(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)(?:-(?:alpha|beta|rc)\.(?<build>\d+))?$'
)
if (-not $match.Success) {
    throw "VERSION must look like 1.2.3 or 1.2.3-alpha.4; got '$version'."
}

$build = if ($match.Groups['build'].Success) {
    $match.Groups['build'].Value
} else {
    '0'
}

$content = @(
    "#define SWIT_VERSION_NUMERIC $($match.Groups['major'].Value),$($match.Groups['minor'].Value),$($match.Groups['patch'].Value),$build"
    "#define SWIT_VERSION_STRING `"$version`""
) -join "`r`n"

$directory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $directory | Out-Null
[System.IO.File]::WriteAllText($OutputPath, "$content`r`n", [System.Text.Encoding]::ASCII)

Write-Output "Generated $OutputPath for SWiT $version"
