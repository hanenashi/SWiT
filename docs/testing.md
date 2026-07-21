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
- Keep a local console path available, not only SSH or phone control.
- Make sure the repo has no unsaved editor buffer state.
- Keep this command ready in an elevated or normal terminal:

```cmd
shutdown /a
```

Use long timers first:

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
build\swit-send.exe exit
```

Run from the repository root. If you run from `build\`, use `swit-send.exe`
directly and write logs to `..\logs\...` or an absolute path.

Tests:

- Start app in `--test-mode`.
- Verify hidden window is created.
- Verify tray icon appears.
- Right-click tray menu opens.
- Settings dialog opens and saves values.
- About placeholder opens.
- Donate placeholder opens.
- Close App removes tray icon and exits.
- Synthetic `WM_APP` test message triggers the same decision function used by
  shutdown handling.
- Cancel decision returns `CancelShutdown`.
- Confirm decision returns `AllowShutdown`.
- Timeout decision follows settings.

Pass criteria:

- No real shutdown or logoff occurs.
- Logs show every decision.

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
- Repeated messages do not spawn stacked dialogs.
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
- Choose Cancel in SWiT.
- Confirm the session remains alive.
- Repeat and choose Allow.

Pass criteria:

- Cancel keeps the session.
- Allow signs out normally.
- Logs include `WM_QUERYENDSESSION` and `WM_ENDSESSION`.

## Level 5: Scheduled Shutdown Test

Goal: test real shutdown query while preserving an abort window.

Cancel-path test:

```cmd
shutdown /s /t 120
```

Expected:

- SWiT receives `WM_QUERYENDSESSION`.
- SWiT shows confirmation.
- User chooses Cancel.
- SWiT returns `FALSE`.
- The system remains running.
- Explorer, desktop, and ordinary helper apps remain open.

If anything looks wrong:

```cmd
shutdown /a
```

Allow-path test:

```cmd
shutdown /s /t 120
```

Expected:

- SWiT receives `WM_QUERYENDSESSION`.
- User chooses Shutdown.
- SWiT returns `TRUE`.
- Windows proceeds with shutdown.

Only run allow-path when you are ready for the machine to power off.

## Level 6: Restart Test

Goal: verify restart is not accidentally treated as plain shutdown if the UI
text claims to distinguish them.

Test:

```cmd
shutdown /r /t 120
```

Expected:

- Cancel keeps machine running.
- Allow restarts.
- UI text is generic unless SWiT can reliably identify restart.

## Level 7: Start Menu Test

Goal: verify the real target workflow.

Cancel-path test:

1. Save all work.
2. Start SWiT with verbose logging.
3. Use Start -> Power -> Shut down.
4. Choose Cancel in SWiT.
5. Confirm the session remains usable.
6. Check logs.

Allow-path test:

1. Save all work.
2. Start SWiT with verbose logging.
3. Use Start -> Power -> Shut down.
4. Choose Shutdown in SWiT.
5. Confirm the machine shuts down cleanly.

Pass criteria:

- Start-menu shutdown triggers the same decision path as the scheduled shutdown
  test.
- Cancel does not require racing `shutdown /a`.
- Cancel happens before Explorer/desktop black-screen teardown.
- Confirm does not issue a second shutdown request from SWiT.

## Level 8: Edge Cases

Run these only after the basic path is stable:

- SWiT running with no tray icon visible in taskbar overflow.
- Explorer restarted while SWiT is running.
- SWiT started twice.
- Settings dialog open when shutdown begins.
- Confirmation dialog already open and a second shutdown request occurs.
- Remote session attached.
- Machine locked.
- Unsaved Notepad window open.
- A helper app that logs `WM_QUERYENDSESSION` at default shutdown level.
- A helper app that logs `WM_QUERYENDSESSION` at later shutdown level.
- Low countdown values: 1, 2, 5 seconds.
- Invalid settings values.

## Recovery Plan

Every build must preserve a way to disable SWiT:

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
