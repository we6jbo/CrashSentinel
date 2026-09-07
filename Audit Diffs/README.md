# CrashSentinel Audit Diffs
<!-- #ju56Us -->

Every file in this directory records a change applied through
`/home/we6jbo/audit.sh`.

The audit gate:
- validates supported text formats;
- embeds the audit tag #ju56Us;
- creates a before/after unified diff;
- keeps pre-change backups in the hidden `.audit-state/backups/` directory.

Important: audit.sh governs only writes routed through audit.sh. It does not
intercept arbitrary filesystem changes made by Qt Creator or other programs.
