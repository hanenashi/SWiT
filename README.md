# SWiT

SWiT is a small Windows 11 utility whose goal is to prevent accidental shutdowns.

The intended behavior is:

1. The user clicks **Start -> Power -> Shut down**.
2. Windows begins ending the interactive session.
3. SWiT receives the shutdown query in the logged-in user's session.
4. SWiT asks for confirmation.
5. If the user cancels, SWiT cancels the shutdown.
6. If the user confirms, SWiT allows Windows to continue shutting down.

## Current Status

This repository is being restarted. The existing files include several early
experiments:

- A hidden-window app that handles `WM_QUERYENDSESSION`.
- A Windows service that attempts to handle preshutdown and launch a GUI prompt.
- A hook DLL experiment that tries to intercept Start menu or shell messages.

Those experiments are archived under `archive/2026-07-21-pre-restart/` as
research material. The clean implementation should be built around a normal
per-user background app with a hidden top-level window and message loop.

## Design Direction

Preferred architecture:

- `swit-agent.exe`: per-user background process started at sign-in.
- Hidden message-only/control window for shutdown notifications.
- `WM_QUERYENDSESSION` handler decides whether to block the shutdown.
- `ShutdownBlockReasonCreate` provides the visible reason Windows shows when
  shutdown is blocked.
- A small foreground confirmation dialog is used when practical.
- Optional tray icon later for settings, enable/disable, and logs.

Avoid as the primary design:

- Global Windows hooks.
- Injecting into Explorer, Start menu, or shell processes.
- A service-first design that needs to cross session boundaries just to show UI.

## Important Windows Behavior

Windows sends `WM_QUERYENDSESSION` when the user or an application requests logoff
or shutdown. If any GUI application returns zero, the session is not ended.

Shutdown blocking is limited by Windows. If an app takes too long or the user
forces shutdown, Windows can continue. SWiT should be treated as an accidental
click guard, not as a hard power-control policy.

Useful Microsoft references:

- `WM_QUERYENDSESSION`: https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession
- `WM_ENDSESSION`: https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession
- Shutdown changes and blocking behavior: https://learn.microsoft.com/en-us/windows/win32/shutdown/shutdown-changes-for-windows-vista
- Application shutdown guidance: https://learn.microsoft.com/en-us/windows/win32/shutdown/shutting-down
- Service shutdown and preshutdown controls: https://learn.microsoft.com/en-us/windows/win32/services/service-control-handler-function

## Build

The old experiments were built directly with MSVC `cl.exe`. The restarted
version should keep that path initially, then move to CMake only if the project
needs more structure.

Expected developer shell:

```cmd
x64 Native Tools Command Prompt for VS
```

Initial build shape:

```cmd
cl.exe /DUNICODE /D_UNICODE /EHsc /W4 /Fe:swit-agent.exe src\swit_agent.cpp user32.lib shell32.lib
```

## Legacy Notes

The old code and backups are archived under
`archive/2026-07-21-pre-restart/`. Do not delete or overwrite them until the
useful details from the old AI chat and manual experiments have been reviewed.

See `docs/history.md` for recovered notes from the old Grok conversation and
local Codex session search.

## Planning Docs

- `docs/roadmap.md`: phased rebuild plan.
- `docs/testing.md`: shutdown-safe test battleplan.
- `docs/design.md`: technical design notes.
- `docs/knowledgebase.md`: Windows shutdown model notes relevant to SWiT.
- `docs/history.md`: recovered history from old AI chats and experiments.
