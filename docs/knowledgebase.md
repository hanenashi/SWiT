# SWiT Knowledgebase

## Windows Shutdown Model

This note captures background that is useful for SWiT's design. Treat Microsoft
documentation as authoritative for API behavior. Treat forum/archive material as
orientation only.

## SWiT's Window Of Influence

SWiT can only help during the early user-session shutdown query phase.

The useful target is:

1. A user chooses Start -> Power -> Shut down, or another app requests shutdown.
2. Windows begins the user-session shutdown negotiation.
3. Top-level GUI windows receive `WM_QUERYENDSESSION`.
4. A GUI app can return `FALSE` to veto session ending.
5. If all apps return `TRUE`, Windows sends `WM_ENDSESSION`.
6. After `WM_ENDSESSION`, apps should clean up; they cannot use that message to
   veto shutdown.

For SWiT, that means:

- The agent must own a real top-level window.
- The window must have a running message loop.
- The process must request an early application shutdown level with
  `SetProcessShutdownParameters`, otherwise Explorer and other apps may receive
  shutdown messages before SWiT and start closing before SWiT cancels.
- The confirmation decision belongs in the `WM_QUERYENDSESSION` path.
- The cancel path returns `FALSE`.
- The allow path returns `TRUE` and lets the original shutdown continue.
- `WM_ENDSESSION` is for logs and cleanup only.
- Calling `ExitWindowsEx` from the confirm path is usually wrong because it
  creates a second shutdown request instead of allowing the original request.

## Why Hooks Are The Wrong Center

Start-menu shutdown eventually becomes a normal Windows shutdown request. The
stable interception point is not the Start menu button or Explorer internals; it
is the session-ending message sent to GUI applications.

Global hooks and shell-message hooks are therefore a diagnostic experiment, not
the product architecture.

## Services Are A Later Phase

Services are handled in the system/session-0 part of shutdown. By that point,
SWiT is no longer in the ergonomic place to ask the logged-in user a question.

Services are useful for:

- enterprise policy;
- background cleanup;
- diagnostics;
- maybe future hardening.

They are a poor v1 fit for:

- ordinary user-facing confirmation UI;
- tray settings;
- canceling a Start-menu shutdown in the interactive user's session.

## Console Apps Are Different

Console apps receive console control events rather than `WM_QUERYENDSESSION`
through a window procedure. SWiT should be a GUI/subsystem app with a top-level
window, not a console process, so it participates in the GUI-app shutdown path.

## Timing And Timeouts

Shutdown negotiation is time-sensitive. SWiT should keep the `WM_QUERYENDSESSION`
path short and deterministic.

Design implications:

- Do not do slow disk, network, update, or telemetry work while handling
  shutdown query.
- Show a simple prompt.
- Keep countdown behavior local and predictable.
- Log quickly.
- Avoid complicated modal stacks.
- Decide a timeout default before real shutdown tests.

## Shutdown Levels

Windows has a shutdown ordering concept for processes. A higher shutdown level
means a process is asked to shut down earlier; a lower level means later. SWiT
should use `SetProcessShutdownParameters` during startup so its shutdown query
happens before ordinary applications begin quitting.

Microsoft documents these application ranges:

- `0x100` to `0x1FF`: application reserved last shutdown range.
- `0x200` to `0x2FF`: application reserved in-between shutdown range.
- `0x300` to `0x3FF`: application reserved first shutdown range.

All processes start at `0x280`, so a SWiT level such as `0x3FF` should place it
ahead of Explorer and normal desktop applications without entering the
system-reserved `0x400` to `0x4FF` range.

Possible future use:

- tune the level if another important app also uses the first-shutdown range;
- test carefully, because ordering can affect interaction with unsaved-work
  prompts in other apps.

This addresses the main failure mode seen in the old SWiT experiments: shutdown
was technically canceled, but Explorer/desktop and other quit-ready apps had
already started closing first. A late veto is not good enough; SWiT needs an
early veto.

## Power-Off Is Too Late

After user processes and services have been handled, Windows proceeds into lower
system shutdown:

- service shutdown;
- system-process cleanup;
- driver shutdown notifications;
- file-system flush;
- registry hive flush;
- storage flush;
- power-state transition.

SWiT should not try to operate here. Once the system is in this phase, the right
behavior is to get out of the way.

## Useful Takeaways From The Archived Overclock.net Thread

The archived thread "Windows: The startup and shutdown process" is useful as a
50,000-foot explanation of why SWiT should live in the user-session GUI path.

Relevant ideas from the thread:

- Start-menu shutdown starts as an `ExitWindowsEx` request.
- The flow involves CSRSS and Winlogon before user-session processes are asked
  whether the session can end.
- GUI applications with top-level windows are queried with
  `WM_QUERYENDSESSION`.
- The first app to return `FALSE` prevents shutdown from continuing.
- If all apps allow shutdown, `WM_ENDSESSION` follows.
- Later service and power-off phases are not a good place for user-facing
  confirmation.

The forum article also mentions registry knobs such as `HungAppTimeout` and
`AutoEndTasks`. SWiT should not depend on changing those values. They are useful
background for interpreting user reports, not part of the v1 design.

Source:

- Archived Overclock.net thread:
  https://web.archive.org/web/20211122222438/https://www.overclock.net/threads/windows-the-startup-and-shutdown-process.1453560
- Microsoft `WM_QUERYENDSESSION`:
  https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession
- Microsoft `WM_ENDSESSION`:
  https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession
- Microsoft `ExitWindowsEx`:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-exitwindowsex
- Microsoft `SetProcessShutdownParameters`:
  https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessshutdownparameters
