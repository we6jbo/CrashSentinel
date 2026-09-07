<!-- READ ME: package architecture rationale.
This document explains why Snap can host the system monitor while Flatpak is
treated primarily as a GUI/report viewer. Keep it synchronized with actual
package manifests and state-path behavior.
-->

# CrashSentinel Packaging Architecture
<!-- #ju56Us -->

## Snap

The Snap is split into two roles:

- `crashsentinel`: normal desktop GUI.
- `monitor`: `CrashSentinel --service`, declared as a system daemon.

`snapd` is responsible for starting the monitor after installation and on
later boots while the service remains enabled. CrashSentinel also keeps its
own single-instance lock as defense in depth.

Both Snap roles resolve the evidence directory through `SNAP_COMMON/state`.
The daemon is the authoritative writer. The GUI is primarily a reader of
monitor evidence. A packaged GUI must not assume that it can rewrite
root-owned service state.

If a recovery operation requires privileged service state, the UI should
report that authorization is required rather than silently failing.

## Flatpak

Flatpak is the GUI/report-viewer distribution.

It must not claim that sandboxed Flatpak execution has unrestricted access
to host journals, raw storage SMART data, pstore, firmware interfaces, or
other privileged host evidence. If those resources are unavailable, the
application should say that confinement or permissions can be the reason.

## Native/development

Development continues to use the normal per-user Qt state location.

Administrators or future native packages can set:

    CRASHSENTINEL_STATE_DIR=/var/lib/crashsentinel

for a deliberately managed shared location. Ownership and permissions must
be configured by the native package/service definition rather than by making
the directory world-writable.

## Security boundary

CrashSentinel must never interpret "permission denied", "tool unavailable",
or "sandbox unavailable" as a healthy result.

The package should request only interfaces actually needed. Raw block-device
access should not be broadened merely to make SMART work.
