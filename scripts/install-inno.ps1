[CmdletBinding()]
param(
    [string]$Version = '7.0.2',
    [string]$ExpectedSha256 = '5ad54ca3def786f8f4212552e54cc6d8d61329e2d24a1cfee0571d42c2684ff1'
)

$ErrorActionPreference = 'Stop'

$installDir = Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7'
$compiler = Join-Path $installDir 'ISCC.exe'
if (Test-Path -LiteralPath $compiler) {
    $uninstallRoot = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall'
    $installed = Get-ChildItem -LiteralPath $uninstallRoot -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ItemProperty -LiteralPath $_.PSPath } |
        Where-Object {
            $_.DisplayName -eq "Inno Setup $Version" -and
            $_.DisplayVersion -eq $Version
        } |
        Select-Object -First 1
    if ($installed) {
        Write-Output "Inno Setup $Version is already installed at $compiler"
        return
    }
}

$tag = 'is-' + $Version.Replace('.', '_')
$fileName = "innosetup-$Version-x64.exe"
$url = "https://github.com/jrsoftware/issrc/releases/download/$tag/$fileName"
$downloadDir = Join-Path $env:TEMP "swit-inno-$Version"
$installer = Join-Path $downloadDir $fileName
New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null

Invoke-WebRequest -Uri $url -OutFile $installer

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash
if ($hash -ne $ExpectedSha256) {
    throw "Inno Setup hash mismatch. Expected $ExpectedSha256, got $hash."
}

$signature = Get-AuthenticodeSignature -LiteralPath $installer
if ($signature.Status -ne 'Valid' -or
    $signature.SignerCertificate.Subject -notmatch 'CN=Pyrsys B\.V\.') {
    throw "Inno Setup Authenticode verification failed: $($signature.Status)."
}

$arguments = @('/VERYSILENT', '/CURRENTUSER', '/NORESTART', '/SUPPRESSMSGBOXES')
$process = Start-Process -FilePath $installer -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "Inno Setup installation failed with exit code $($process.ExitCode)."
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Inno Setup completed but ISCC.exe was not found at $compiler."
}

Write-Output "Installed verified Inno Setup $Version at $compiler"
