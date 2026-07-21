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

## Restart Requirements To Preserve

Minimum useful v1:

- Per-user tray app.
- Hidden top-level window.
- Handles `WM_QUERYENDSESSION`.
- Shows confirmation prompt for Start-menu shutdown.
- Cancel path returns `FALSE`.
- Confirm path returns `TRUE` and lets the original shutdown continue.
- Tray right-click menu with Settings, Close App, About, Donate.
- Settings persist per user.

Later:

- Countdown UI.
- Default-on-timeout action.
- Distinguish shutdown, restart, and logoff when Windows exposes enough detail.
- Single-instance guard.
- Autostart at sign-in.
- Installer or setup script.
