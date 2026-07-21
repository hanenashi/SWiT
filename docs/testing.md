# SWiT Testing Battleplan

## Safety Rules

Shutdown tests are destructive to the current session. Use this order:

1. Unit tests.
2. Synthetic window-message tests.
3. Logoff tests.
4. Scheduled shutdown tests with a long timeout.
5. Real Start-menu shutdown tests.

Before every real shutdown test:

- Save all work.
- Run SWiT and its helper from a normal, non-elevated terminal.
- Keep a local console path available, not only SSH or phone control.
- Make sure the repo has no unsaved editor buffer state.
- Keep this command ready in an elevated or normal terminal:

```cmd
shutdown /a
```

For forced-shutdown negative testing, long timers are useful:

```cmd
shutdown /s /t 120
```

Abort immediately if the prompt does not appear:

```cmd
shutdown /a
```

Do not use forced shutdown flags such as `/f` during SWiT validation. Forced
shutdown is for a different behavior class and can bypass or shorten normal app
cleanup.

Important: Microsoft documents that if `/t` is greater than zero, `/f` is
implied. That means `shutdown /s /t 120` is not a normal veto test. It arrives
as forced shutdown (`ENDSESSION_CRITICAL`) and SWiT's cancel may be ignored.

## Test Matrix

Track every test with:

- SWiT build/version.
- Windows version.
- Trigger used.
- Expected SWiT decision.
- Actual result.
- Whether the machine stayed reachable.
- Relevant log lines.

Suggested result states:

- `PASS`
- `FAIL_SAFE`: shutdown did not happen, but behavior was wrong.
- `FAIL_DANGEROUS`: shutdown happened when cancel was expected.
- `BLOCKED`: test could not be completed.

## Recorded Results

Kurochan, 2026-07-21, Start-menu shutdown, native-screen build: `PASS`.

- Agent level changed from `0x280` to `0x3FF`.
- Agent returned `FALSE` for a non-critical query.
- Windows followed with `WM_ENDSESSION ending=0` about seven seconds later.
- Default-level helper received no `WM_QUERYENDSESSION`.
- Codex, Total Commander, and browser audio remained running.
- No delayed SWiT dialog appeared after returning to the desktop.

## Level 1: No-Shutdown Tests

Goal: test logic without invoking Windows shutdown.

Build:

```cmd
scripts\build.bat
```

Run:

```cmd
build\swit-agent.exe --test-mode --cancel-on-query --log logs\smoke.log
build\swit-send.exe ping
build\swit-send.exe shutdown
build\swit-send.exe restart
build\swit-send.exe logoff
build\swit-send.exe disable
build\swit-send.exe shutdown
build\swit-send.exe enable
build\swit-send.exe shutdown
build\swit-send.exe exit
```

Run from the repository root. If you run from `build\`, use `swit-send.exe`
directly and write logs to `..\logs\...` or an absolute path.

Tests:

- Start app in `--test-mode`.
- Verify hidden window is created.
- Verify the tray icon appears, possibly in the overflow area.
- Verify its menu checkmark follows enable/disable state.
- Synthetic `WM_APP` test message triggers the same decision function used by
  shutdown handling.
- Block mode logs `decision=cancel` without showing UI.
- Allow mode logs `decision=allow` without showing UI.
- Starting a second agent exits with code 0 and does not create a second log.
- Disable removes the block reason; enable recreates it.
- `swit-send.exe exit` closes the agent cleanly.
- Exit removes the tray icon and block reason.
- Logs are readable while the agent is still running.

Pass criteria:

- No real shutdown or logoff occurs.
- Logs show every decision.
- `type logs\smoke.log` works while the agent is still running and begins with
  `[` rather than a UTF-8 BOM artifact.

## Level 2: Message-Only Harness

Goal: exercise the window procedure safely.

Build a small test helper that finds the SWiT window and sends custom messages,
not real `WM_QUERYENDSESSION` from Windows.

Tests:

- Send `SWIT_TEST_QUERY_SHUTDOWN`.
- Send `SWIT_TEST_QUERY_RESTART`.
- Send `SWIT_TEST_QUERY_LOGOFF`.
- Send malformed/unknown test values.
- Send repeated query messages.

Pass criteria:

- Decisions are deterministic.
- Repeated messages remain deterministic and do not show UI.
- Unknown values fall back to the safest configured behavior.

## Level 3: ShutdownBlockReason Smoke Test

Goal: verify `ShutdownBlockReasonCreate` usage without relying on shutdown
timing.

Tests:

- Call `ShutdownBlockReasonCreate` from the same thread that created SWiT's
  main window.
- Query or log success/failure and `GetLastError`.
- Call `ShutdownBlockReasonDestroy` on disable/exit.
- Try intentionally calling it from the wrong thread in a debug-only test and
  verify failure is logged.

Pass criteria:

- Normal path succeeds.
- Wrong-thread path fails predictably and does not crash.

## Level 3A: Shutdown Order Smoke Test

Goal: verify SWiT will be queried before ordinary apps.

Tests:

- On startup, call `GetProcessShutdownParameters` and log the default level.
- Call `SetProcessShutdownParameters(0x3FF, 0)`.
- Call `GetProcessShutdownParameters` again and verify the level changed.
- Run one helper GUI app at default level `0x280`:

```cmd
build\swit-helper.exe --log logs\helper-default.log
build\swit-send.exe --helper ping
build\swit-send.exe --helper exit
```

- Run one helper GUI app at an explicit later level such as `0x180`:

```cmd
build\swit-helper.exe --level 0x180 --log logs\helper-late.log
build\swit-send.exe --helper exit
```

Pass criteria:

- SWiT logs shutdown level `0x3FF`.
- Helper apps remain ordinary or later priority.
- The real-shutdown test plan does not proceed until this is confirmed.

## Level 4: Logoff Test

Goal: test session-ending behavior with less disruption than power-off.

Preparation:

- Save work.
- Keep local login available.
- Use a test Windows account if possible.

Tests:

- Trigger sign out from Start menu.
- Choose Cancel in Windows' blocker UI.
- Confirm the session remains alive.
- Repeat and choose Allow.

Pass criteria:

- Cancel keeps the session.
- Allow signs out normally.
- Logs include `WM_QUERYENDSESSION` and `WM_ENDSESSION`.

## Level 5: Forced Shutdown Negative Test

Goal: understand the command-line timer trap before trying the real veto path.

This command has an abort window, but it also implies `/f` because `/t` is
greater than zero:

```cmd
shutdown /s /t 120
```

Expected:

- SWiT receives `WM_QUERYENDSESSION`.
- The reason flags include `ENDSESSION_CRITICAL`.
- SWiT logs that cancel may be ignored.
- Helper apps may also receive `WM_QUERYENDSESSION`.
- The machine may shut down.

If anything looks wrong:

```cmd
shutdown /a
```

Do not treat this as a failed early-veto test. It is a forced-shutdown test.

## Level 6: Non-Forced Shutdown Veto Test

Goal: test real cancel behavior without the `/t > 0` forced-shutdown path.

Preparation:

- Save all work.
- Keep local physical access available.
- Start SWiT in cancel mode.
- Start `swit-helper.exe` at default `0x280`.

Test:

```cmd
shutdown /s /t 0
```

Expected:

- SWiT receives `WM_QUERYENDSESSION`.
- The reason flags do not include `ENDSESSION_CRITICAL`.
- SWiT returns `FALSE`.
- Windows shows its native blocker UI with SWiT's reason.
- Choosing Cancel aborts shutdown.
- The helper does not receive `WM_QUERYENDSESSION`.
- Explorer and desktop remain alive.

This is riskier because there is no timer window. Run it only after the
no-shutdown harness and helper tests are clean.

## Level 7: Restart Test

Goal: verify restart is not accidentally treated as plain shutdown if the UI
text claims to distinguish them.

Test:

```cmd
shutdown /r /t 0
```

Expected:

- Cancel keeps machine running.
- Allow restarts.
- UI text is generic unless SWiT can reliably identify restart.

## Level 8: Start Menu Test

Goal: verify the real target workflow.

Cancel-path test:

1. Save all work.
2. Start SWiT with verbose logging.
3. Use Start -> Power -> Shut down.
4. Choose Cancel in Windows' blocker UI.
5. Confirm the session remains usable.
6. Check logs.

Allow-path test:

1. Save all work.
2. Start SWiT with verbose logging.
3. Use Start -> Power -> Shut down.
4. Choose Shut down anyway in Windows' blocker UI.
5. Confirm the machine shuts down cleanly.

Pass criteria:

- Start-menu shutdown shows SWiT's reason in Windows' blocker UI.
- Cancel does not require racing `shutdown /a`.
- Cancel preserves Explorer and ordinary applications even though Windows
  temporarily shows its full-screen shutdown UI.
- Shut down anyway does not depend on a second shutdown request from SWiT.

## Level 9: Edge Cases

Run these only after the basic path is stable:

- SWiT running with no tray icon visible in taskbar overflow.
- Explorer restarted while SWiT is running.
- SWiT started twice.
- Settings dialog open when shutdown begins.
- A second shutdown request occurs immediately after a canceled request.
- Remote session attached.
- Machine locked.
- Unsaved Notepad window open.
- A helper app that logs `WM_QUERYENDSESSION` at default shutdown level.
- A helper app that logs `WM_QUERYENDSESSION` at later shutdown level.
- Low countdown values: 1, 2, 5 seconds.
- Invalid settings values.

## Recovery Plan

Every build must preserve a way to disable SWiT:

- `build\swit-send.exe exit` during the trayless MVP.
- Close App from tray menu.
- Task Manager process end.
- Startup entry removal.
- A documented command or script to disable autostart.

Do not install SWiT as always-on autostart until the recovery path is tested.

## References

- `WM_QUERYENDSESSION`:
  https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession
- `WM_ENDSESSION`:
  https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-endsession
- `ShutdownBlockReasonCreate`:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-shutdownblockreasoncreate
- `shutdown` command:
  https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/shutdown
- `SetProcessShutdownParameters`:
  https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessshutdownparameters
