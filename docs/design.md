# SWiT Design Notes

## Goal

Catch accidental shutdowns initiated from Windows 11's normal UI, especially
Start -> Power -> Shut down, and ask the interactive user to confirm.

## Non-goals

- Prevent forced shutdowns.
- Block physical power-button behavior at firmware or hardware level.
- Inject code into Explorer or Start menu as the main mechanism.
- Depend on a Windows service for user-facing UI.

## Recommended First Implementation

Build a per-user Win32 app:

1. Start at user sign-in.
2. Create a hidden top-level window.
3. Run a standard message loop.
4. Handle `WM_QUERYENDSESSION`.
5. If protection is enabled:
   - call `ShutdownBlockReasonCreate`;
   - show a short confirmation prompt if Windows still permits UI;
   - return `FALSE` when the user cancels or does not explicitly confirm.
6. Handle `WM_ENDSESSION` for cleanup and logging.

This should catch the normal Start menu shutdown path because that path asks
Windows to end the session, which broadcasts `WM_QUERYENDSESSION` to GUI apps
with windows/message queues.

See `docs/roadmap.md` for implementation phases and `docs/testing.md` for the
shutdown test ladder.

## Open Questions

- Should SWiT ask every time, or only when certain apps are running?
- Should cancel be the default on timeout?
- Should confirmation text distinguish shutdown, restart, and logoff when
  Windows exposes enough detail?
- Should there be a tray icon for temporary disable?
- Should settings live in the registry, a local config file, or both?
- Should logs go to a text file, Event Log, or DebugView only?

## Legacy Experiments

Recovered product and AI-chat history is captured in `docs/history.md`. Legacy
source experiments are archived under `archive/2026-07-21-pre-restart/`.

### Hidden Window

The old `swit.cpp` approach is closest to the clean design. It already uses a
hidden window, `WM_QUERYENDSESSION`, and `WTSRegisterSessionNotification`.

Problems to revisit:

- It calls `ExitWindowsEx` after confirmation, which may create a second
  shutdown request instead of simply allowing the original one to continue.
- It uses a modal `MessageBox` directly inside the shutdown query handler.
- It needs clearer timeout and fallback behavior.

### Service Plus GUI

The old `swit_service.cpp` tries to receive preshutdown in a service and launch
`swit_gui.exe` in the active user session.

Useful ideas:

- Preshutdown gives earlier notice than normal shutdown service control.
- A separate UI process keeps service code simpler.

Problems:

- Services run in session 0 and cannot directly present normal user UI.
- Cross-session token/process launching adds privilege complexity.
- Canceling shutdown from a service is not the same UX as a user-session app
  vetoing `WM_QUERYENDSESSION`.

### Hook DLL

The untracked `swit_hook.cpp` and `swit_launcher.cpp` experiment tries to hook
shell or Start menu messages.

Useful ideas:

- It produced logs that may reveal message IDs seen during manual tests.

Problems:

- Hooks are brittle across Windows 11 updates.
- Hooking Explorer/Start menu internals is harder to test and support.
- It creates avoidable security and stability risk for a simple accidental
  shutdown guard.

## Useful Chat-Recovery Targets

When looking for the old AI conversation, search for terms like:

- `WM_QUERYENDSESSION`
- `ShutdownBlockReasonCreate`
- `SERVICE_ACCEPT_PRESHUTDOWN`
- `SWiTService`
- `WTSQueryUserToken`
- `CreateProcessAsUser`
- `SetWindowsHookEx`
- `Start menu shutdown`
- `AbortSystemShutdown`
