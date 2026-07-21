# Pre-Restart Archive

This folder contains SWiT material that existed before the 2026-07-21 restart.

It is kept for reference only. The current implementation should not import or
build from this folder directly.

Contents:

- `BKP/`: early backup of the single-executable hidden-window attempt.
- `BKP 2/`: early service plus GUI split.
- `SWIT/`: older packaged build folder.
- ` LAST/`: latest manual backup before restart, including service/gui sources
  and packaged output.
- `swit_hook.cpp` and `swit_launcher.cpp`: hook DLL experiment.
- `.vs/`: Visual Studio local state from the old workspace.
- `Procmon.exe`: local troubleshooting tool copied into the repo during manual
  investigation.

Generated binaries, logs, Visual Studio state, and zip files in this archive are
ignored by Git. Source files remain visible so useful ideas can be recovered.
