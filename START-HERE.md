# CrashSentinel: START HERE

<!-- #ju56Us -->

This file is the map for reading CrashSentinel in Qt Creator.

## The fastest way to understand the project

Start with these files in this order:

1. `main.cpp` — decides whether CrashSentinel runs as the normal GUI, the
   background monitor (`--service`), or a recovery command.
2. `mainwindow.cpp` / `mainwindow.h` — builds the six-tab user interface.
3. `crash_monitor.cpp` — performs the recurring lightweight monitoring work.
4. `self_protection.cpp` — protects the computer from CrashSentinel itself,
   including the 5-abnormal-reboot precaution and the 20-abnormal-reboot
   hard stop.
5. `resource_guard.cpp` — slows or suspends collection when system resources
   are under pressure.
6. `deep_diagnostics.cpp` — produces read-only evidence reports after a crash.
7. `recovery_diagnostics.cpp` — provides recovery and export operations.
8. `state_paths.cpp` — decides where GUI and service state is stored in
   development/native, Snap, and Flatpak environments.
9. `publishing_status.cpp` — checks only local publishing prerequisites.
10. `snapcraft.yaml` and `PACKAGING-ARCHITECTURE.md` — explain package behavior.

## Why Qt Designer looks almost empty

`mainwindow.ui` contains only a very small placeholder form. The real
CrashSentinel interface is intentionally built in C++ inside `mainwindow.cpp`.
That means the Design view in Qt Creator will not show the six-tab runtime
interface.

To change the visible application interface, normally edit these functions in
`mainwindow.cpp`:

- `buildPublishingTab()` → Install & Publish
- `buildStatusTab()` → Status
- `buildSafeguardsTab()` → Safeguards
- `buildProfilesTab()` → Profiles
- `buildReportsTab()` → Reports
- `buildRecoveryDiagnosticsTab()` → Recovery & Diagnostics

`MainWindow::MainWindow()` controls the order in which those tabs appear.

## Safety rules worth preserving

- Never interpret missing or permission-denied evidence as proof that the
  computer is healthy.
- A whole-computer reboot while CrashSentinel was active is correlation, not
  proof that CrashSentinel caused the reboot.
- At 5 observed abnormal reboots, monitoring stops as a precaution unless the
  user deliberately acknowledges it.
- The abnormal-reboot count is preserved after that acknowledgement.
- At 20 observed abnormal reboots, monitoring must stop and normal quarantine
  clearing is refused until an explicit reboot-safeguard reset.
- Resource-guard hard limits are intentionally stronger than user preferences.
- `phase58790.txt` is a final project-completion marker. Do not create it until
  the Qt Creator work has been verified complete.

## Package model

- Snap: GUI plus a package-managed system monitor daemon.
- Flatpak: primarily GUI/report viewer because unrestricted privileged host
  monitoring is a poor fit for a sandbox.
- Development/native: uses the normal per-user state directory unless
  `CRASHSENTINEL_STATE_DIR` overrides it.

## Comments versus behavior

The explanatory comments added in this pass are documentation. They are meant
to make the source teachable without changing runtime behavior. If a comment
and the code ever disagree, treat the code as authoritative and update the
comment.
