# SWiT Packaging

## Package Contract

SWiT uses Inno Setup 7 to produce one x64 installer. The install is per-user and
does not request elevation.

- Install directory: `%LOCALAPPDATA%\Programs\SWiT`
- Log directory: `%LOCALAPPDATA%\SWiT\logs`
- Startup: current user's `Software\Microsoft\Windows\CurrentVersion\Run` key
- Start menu group: current user's `SWiT` group
- Uninstall registration: current user only

First install offers **Start SWiT when I sign in**, selected by default.
Upgrades leave the current Run-key state unchanged so an update cannot silently
undo a user's startup preference.

Uninstall sends SWiT its normal Exit command before removing files. It removes
the Run value only when that value points to the installed executable, then
removes SWiT's install directory, Start menu group, uninstall registration, and
per-user log directory.

## Build

Prerequisites:

- Visual Studio 2022 C++ build tools
- Inno Setup 7.0.2 x64

Install the pinned compiler from its official GitHub release:

```powershell
.\scripts\install-inno.ps1
```

The bootstrap script verifies the pinned SHA-256 hash and requires a valid
Authenticode signature from `Pyrsys B.V.` before running the compiler installer.

Build the binaries, installer, and checksum manifest:

```powershell
.\scripts\build-release.ps1
```

Artifacts are written to `dist\`:

```text
SWiT-Setup-<version>-x64.exe
SHA256SUMS.txt
```

`VERSION` is the release version source. Prerelease versions use forms such as
`0.1.0-alpha.1`; the build maps that to the four-part Windows resource version
`0.1.0.1`.

## Signing

The alpha package can be built unsigned for trusted testing. For a signed
release, make the code-signing certificate available in the current user's
certificate store and run:

```powershell
.\scripts\build-release.ps1 -CertificateThumbprint <thumbprint>
```

The script signs `swit-agent.exe`, `swit-send.exe`, and the final installer with
SHA-256 and an RFC 3161 timestamp before generating checksums.

## Tray Identity

Windows binds a GUID-based notification icon to the full path of the executable
that first registers it. SWiT therefore has separate development and production
tray GUIDs. Debug builds use the development identity; release builds must first
run from the stable installed path. Do not distribute or directly run the
release binary from arbitrary locations as a portable build.

## CI

`.github\workflows\build-release.yml` builds release artifacts on version tags
and manual workflow dispatch. It installs the same pinned and verified Inno
Setup compiler, then uploads the installer and checksum manifest as workflow
artifacts. It does not publish a GitHub Release automatically.

## Release Checklist

1. Update `VERSION`.
2. Build with `scripts\build-release.ps1`.
3. Verify file versions and `dist\SHA256SUMS.txt`.
4. Install over the previous version and verify the startup choice is preserved.
5. Verify tray controls and the no-shutdown synthetic test.
6. Uninstall and confirm process, Run value, files, shortcuts, logs, and uninstall
   registration are gone.
7. Reinstall and perform the Windows 11 shutdown cancel test on a saved session.
8. Sign public artifacts before publishing when a certificate is available.
