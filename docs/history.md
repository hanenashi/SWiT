# SWiT History Notes

## Sources Checked

- Local Codex rollout search under `C:\Users\hanenashi\.codex\sessions`.
- Legacy repo material archived under `archive/2026-07-21-pre-restart/`.
- Grok shared conversation:
  `https://grok.com/share/bGVnYWN5_9f8d2a81-c292-43ef-9cc0-1aa405683f76`
- Archived Overclock.net shutdown explainer:
  `https://web.archive.org/web/20211122222438/https://www.overclock.net/threads/windows-the-startup-and-shutdown-process.1453560`
- Microsoft Win32 shutdown and notification-area documentation.

## Local Codex History

The likely-looking Codex session
`2026\05\21\rollout-2026-05-21T02-05-20-019e4659-55cc-7193-a58d-ebd78e520e75.jsonl`
was a false lead for SWiT implementation details. Its working directory was
`c:\GIT\PNG 200`, and the SWiT matches came from GitHub repository-list output,
not from a SWiT coding discussion.

Other local rollout files contained sparse SWiT mentions, but the low-level API
terms only appear in the current restart session. The old implementation work
was therefore likely done in Grok or another browser chat, not in Codex.

The Codex session metadata that did mention SWiT recorded model `gpt-5.5`, but
that does not appear to be the source of the app's original design.

## Grok Conversation Summary

The Grok conversation identifies the original product idea more clearly than the
current repository does:

- SWiT was intended as a per-user Windows tray app, not only a background daemon.
- The tray icon should normally live in hidden notification icons.
- Right-click menu items were intended to be:
  - Settings
  - Close App
  - About
  - Donate
- About and Donate were placeholders.
- Settings were intended to include:
  - `Shutdown countdown` checkbox plus seconds field.
  - `Cancel countdown` checkbox plus seconds field.
- During shutdown confirmation, enabled countdowns should be visible on the
  confirmation screen.
- When a countdown reaches zero, SWiT should automatically choose the configured
  action: proceed with shutdown or cancel shutdown.

Grok proposed a single Win32 tray app using:

- `resource.h`
- `SWiTResources.rc`
- `SWiTDaemon.cpp`
- `Shell_NotifyIcon`
- `NOTIFYICONDATA`
- a hidden `CreateWindowW` window
- a popup menu from tray right-click
- settings saved under `HKCU\Software\SWiT_Daemon`
- settings and shutdown confirmation dialogs backed by Win32 resources
- `WM_QUERYENDSESSION` as the shutdown interception point

## Problems Found In The Grok Design

The Grok answer was useful for product requirements, but the technical plan
needs cleanup before reuse.

Issues it identified in the user's original code:

- The hidden window class was created without first registering a `WNDCLASSEX`.
- The tray popup menu did not match the intended Settings, Close App, About,
  Donate command set.
- Resource definitions and code identifiers were inconsistent.
- The shutdown dialog countdown behavior was underspecified when both shutdown
  and cancel countdowns were enabled.
- The project depended on an icon file named `SWiTDaemon.ico`.
- Visual Studio reported `unexpected end of file found`, probably from a
  truncated or malformed source file, not from the `resource.h` shown later.

Problems in Grok's proposed implementation:

- It treated "kill the shutdown process" as a viable framing. The cleaner
  Windows model is to veto session ending via `WM_QUERYENDSESSION`; there is no
  stable Start-menu shutdown process to kill.
- It used modal dialogs inside shutdown handling without clearly respecting the
  short time window Windows gives apps before showing its own blocker UI.
- It mixed resource IDs as C++ `constexpr` values. That is useful in C++ code,
  but `.rc` files are preprocessed and generally expect `#define`-style resource
  constants. A clean restart should separate resource IDs in a conventional
  `resource.h`.
- It stored settings in the registry, which is acceptable, but the restart
  should hide that behind a small settings module.

## Confirmed API Direction

The restart should use a normal per-user Win32 app with a real hidden top-level
window and message loop.

Key Win32 facts:

- `WM_QUERYENDSESSION` is sent when a user chooses to end the session or an app
  calls a shutdown function.
- Returning zero from `WM_QUERYENDSESSION` cancels the session end.
- Apps should respond quickly and defer cleanup to `WM_ENDSESSION`.
- If an app must block or postpone shutdown, Microsoft points to
  `ShutdownBlockReasonCreate`.
- `ShutdownBlockReasonCreate` requires the app's main window handle and must be
  called from the thread that created that window.
- Notification-area icons are implemented with `Shell_NotifyIcon` and
  `NOTIFYICONDATA`; Windows may put icons in the overflow area by default.

## Known Working Legacy Blocker

The user reconfirmed that
`archive/2026-07-21-pre-restart/ LAST/swit.exe` prevents final shutdown on
Windows 11, but only after Codex, Total Commander, Explorer, and the desktop
have already begun quitting. Its SHA-256 is
`AAF72758A378E40E47DBCB5332083CFF74DC83D4A3D454F79D588AA9D6919510`.

The executable was built from the adjacent `swit.cpp`. It owns a hidden
top-level window, handles `WM_QUERYENDSESSION`, shows a modal `MessageBox`, and
returns `FALSE`. It does not call `SetProcessShutdownParameters`, so it remains
at the ordinary `0x280` shutdown level. The clean agent reuses only the veto
behavior at level `0x3FF`; it does not rely on or launch the archived binary.

The legacy confirm path calls `ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, ...)`.
That creates a second forced shutdown request, so it must not be copied into the
clean implementation.

## First Early-Veto Result

The 2026-07-21 Start-menu test on Kurochan proved that shutdown ordering works:

- SWiT received the query at level `0x3FF`.
- The helper at default level `0x280` received no query.
- Codex and Total Commander remained running.
- Windows displayed its full-screen blocker UI with SWiT's reason.
- The modal SWiT dialog was hidden until the Windows UI canceled shutdown and
  returned to the desktop.

This changed the MVP design. SWiT now returns `FALSE` immediately and treats
Windows' blocker UI as the confirmation surface.

The revised native-screen build passed a second Start-menu test at 22:25:

- SWiT logged the query at `22:25:28.443` and returned `FALSE` immediately.
- Windows reported `WM_ENDSESSION ending=0` at `22:25:35.326`.
- The native shutdown screen presented its choices without the previous late
  SWiT dialog.
- The `0x280` helper received no shutdown query.
- Codex, Total Commander, and browser audio remained running.

This is the first complete acceptance pass for the protected cancel path.

## Restart Requirements To Preserve

Minimum useful v1:

- Per-user tray app.
- Hidden top-level window.
- Handles `WM_QUERYENDSESSION`.
- Registers a clear shutdown block reason.
- Returns `FALSE` immediately at application-first shutdown priority.
- Uses Windows' native blocker UI for confirmation.
- Tray right-click menu with Settings, Close App, About, Donate.
- Settings persist per user.

Later:

- Countdown UI.
- Default-on-timeout action.
- Distinguish shutdown, restart, and logoff when Windows exposes enough detail.
- Single-instance guard.
- Autostart at sign-in.
- Installer or setup script.
