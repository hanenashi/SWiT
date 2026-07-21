# SWiT Roadmap

## Principle

Build SWiT as an accidental-shutdown guard, not as a power-policy tool.

The project should prove each layer independently before testing real shutdown.
Shutdown testing is disruptive, can lose unsaved work, and can break the remote
control loop if the machine actually powers off.

## Phase 0: Repository Reset

Goal: make the repo understandable and reproducible.

Tasks:

- Keep old experiments available under `archive/2026-07-21-pre-restart/` until
  reviewed.
- Keep generated binaries and scratch output out of the source path or ignored
  when archived.
- Create a clean `src/` layout for the restart.
- Keep docs for design, history, roadmap, and testing.
- Add a simple MSVC build script.

Done when:

- `git status` clearly separates intentional source/docs from old deleted files.
- A fresh reader can understand what SWiT is and what path is current.

## Phase 1: Window And Message Harness

Goal: prove the Win32 message core without tray UI or real shutdown.

Tasks:

- Create `swit-agent.exe`. Done.
- Register a real top-level hidden window class. Done.
- Run a normal message loop. Done.
- Call `SetProcessShutdownParameters(0x3FF, 0)` and log success/failure. Done.
- Log `WM_CREATE`, `WM_CLOSE`, `WM_DESTROY`, `WM_QUERYENDSESSION`, and
  `WM_ENDSESSION`. Done.
- Add a custom registered test message that exercises the same decision path
  without asking Windows to shut down. Done.
- Add command-line flags. Started:
  - `--test-mode`
  - `--allow-on-query`
  - `--cancel-on-query`
  - `--log <path>`
- Add `swit-send.exe` helper for synthetic messages. Done.

Done when:

- The app starts and exits normally.
- The test message can drive the confirmation decision without ending the
  Windows session.
- Logs are timestamped and readable.

Current status: completed for no-shutdown smoke testing. Real shutdown testing
still waits for helper-app ordering checks.

## Phase 2: Shutdown Decision Core

Goal: make the shutdown decision deterministic and testable.

Tasks:

- Extract shutdown decision state into plain C++ functions/classes.
- Define explicit decisions:
  - `AllowShutdown`
  - `CancelShutdown`
  - `TimedOutAllow`
  - `TimedOutCancel`
- Define default behavior when the prompt cannot be shown.
- Decide whether v1 defaults to cancel or allow on timeout.
- Unit-test countdown and settings decisions without Win32 shutdown calls.

Done when:

- Countdown behavior is predictable.
- Both buttons and timeout paths are testable without a real shutdown.

## Phase 2A: Ordering Helper

Goal: prove that real shutdown tests can detect whether SWiT vetoes before
ordinary apps are asked to quit.

Tasks:

- Create `swit-helper.exe`. Done.
- Log default shutdown level `0x280`. Done.
- Support explicit helper shutdown level such as `0x180`. Done.
- Support synthetic ping/exit through `swit-send.exe --helper`. Done.
- During the first cancel-path real shutdown test, run the helper and verify it
  does not receive `WM_QUERYENDSESSION` before SWiT cancels.

Done when:

- Helper starts and exits cleanly.
- Helper logs its configured shutdown level.
- Real cancel-path test leaves helper running and its log has no shutdown query.

## Phase 3: Tray App MVP

Goal: restore the original product shape safely.

Tasks:

- Add notification-area icon via `Shell_NotifyIcon`.
- Add right-click menu:
  - Settings
  - Close App
  - About
  - Donate
- Add placeholders for About and Donate.
- Add single-instance guard.
- Handle Explorer/taskbar restart and recreate the tray icon.
- Add explicit Close App behavior that removes the tray icon cleanly.

Done when:

- The app lives in the tray.
- Right-click menu works.
- Closing the app cannot leave stale UI state behind.

## Phase 4: Settings

Goal: add the controls recovered from the Grok history.

Tasks:

- Add per-user persisted settings.
- Implement:
  - Shutdown countdown enabled.
  - Shutdown countdown seconds.
  - Cancel countdown enabled.
  - Cancel countdown seconds.
- Validate settings ranges.
- Make mutually enabled countdowns deterministic. Pick one policy:
  - one default action radio group, or
  - reject enabling both countdowns.

Done when:

- Settings survive app restart.
- Invalid values cannot produce strange shutdown behavior.

## Phase 5: Controlled Shutdown Integration

Goal: test real `WM_QUERYENDSESSION` handling with recoverable shutdown attempts.

Tasks:

- Register a short shutdown block reason with `ShutdownBlockReasonCreate`.
- Set an application-first shutdown level before real shutdown tests.
- On `WM_QUERYENDSESSION`, show confirmation only when safe.
- Return `FALSE` for cancel.
- Return `TRUE` for confirm and let the original shutdown continue.
- Handle `WM_ENDSESSION` only for cleanup/logging.
- Never call `ExitWindowsEx` from inside the normal confirm path.

Done when:

- The forced-shutdown negative test logs `ENDSESSION_CRITICAL`, proving why
  timed `shutdown /t > 0` is not a valid veto test.
- A non-forced shutdown request plus a cancel decision keeps the machine
  running.
- Explorer and ordinary test apps remain open after a cancel decision.
- Logs clearly show query, decision, and end-session results.

## Phase 6: Start Menu Validation

Goal: verify the real target workflow.

Tasks:

- Test Start -> Power -> Shut down manually.
- Test cancel.
- Test confirm.
- Test restart and logoff separately.
- Test while common apps have unsaved work.
- Test from both local console and remote-control workflow.

Done when:

- Start-menu shutdown produces the intended SWiT prompt.
- Cancel keeps the Windows session alive.
- Confirm proceeds without launching a second shutdown request.

## Phase 7: Packaging

Goal: make SWiT easy to install and remove.

Tasks:

- Add autostart at sign-in.
- Add uninstall/disable path.
- Decide between zip, installer, or simple setup script.
- Document recovery command to disable SWiT if it misbehaves.

Done when:

- A clean Windows account can install, run, disable, and uninstall SWiT.
