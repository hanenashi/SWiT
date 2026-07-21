[CmdletBinding()]
param(
    [string]$InnoCompiler,
    [string]$CertificateThumbprint,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
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
$numericVersion = '{0}.{1}.{2}.{3}' -f $match.Groups['major'].Value, $match.Groups['minor'].Value, $match.Groups['patch'].Value, $build

if (-not $SkipBuild) {
    & (Join-Path $root 'scripts\build.bat') release
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE."
    }
}

if ([string]::IsNullOrWhiteSpace($InnoCompiler)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe')
    )
    $InnoCompiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($InnoCompiler) -or
    -not (Test-Path -LiteralPath $InnoCompiler)) {
    throw 'Inno Setup 7 ISCC.exe was not found. Install it or pass -InnoCompiler.'
}

function Find-SignTool {
    $command = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    return Get-ChildItem -LiteralPath $kits -Filter signtool.exe -Recurse |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

function Invoke-CodeSign([string[]]$Paths) {
    if ([string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        return
    }

    $signTool = Find-SignTool
    if ([string]::IsNullOrWhiteSpace($signTool)) {
        throw 'signtool.exe was not found.'
    }

    foreach ($path in $Paths) {
        & $signTool sign /sha1 $CertificateThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 $path
        if ($LASTEXITCODE -ne 0) {
            throw "Signing failed for $path."
        }
    }
}

$agentPath = Join-Path $root 'build\swit-agent.exe'
$senderPath = Join-Path $root 'build\swit-send.exe'
Invoke-CodeSign @($agentPath, $senderPath)

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$installerPath = Join-Path $dist "SWiT-Setup-$version-x64.exe"
Remove-Item -LiteralPath $installerPath -Force -ErrorAction SilentlyContinue

& $InnoCompiler "/DAppVersion=$version" "/DAppNumericVersion=$numericVersion" (Join-Path $root 'installer\SWiT.iss')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $installerPath)) {
    throw "Installer build failed with exit code $LASTEXITCODE."
}

Invoke-CodeSign @($installerPath)

$artifacts = @($installerPath)
$checksumLines = foreach ($artifact in $artifacts) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact).Hash.ToLowerInvariant()
    '{0}  {1}' -f $hash, (Split-Path -Leaf $artifact)
}
$checksumPath = Join-Path $dist 'SHA256SUMS.txt'
[System.IO.File]::WriteAllLines(
    $checksumPath,
    $checksumLines,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Output "Built SWiT $version release:"
Write-Output $installerPath
Write-Output $checksumPath
