# Repository history and source provenance

This local history was assembled on 15 November 2026. The import commits use
the repository owner's identity as importer; they do not attribute the original
WinVi implementation to that owner. Original authorship and licence notices
are preserved. Import dates are not original release or development dates.

The first commit contains all 75 non-directory members of the official
wvsrc302.zip, byte for byte. Later commits follow source snapshots preserved in
the release ZIPs listed below. Unchanged original files omitted from a release
package are retained when they remain in the current working source. Obsolete
test scripts absent from the next snapshot are removed. Consequently these
commits reconstruct source evolution, not the exact contents of release ZIPs.

The final housekeeping commit adds repository documentation, byte-preserving
Git attributes, ignore rules and the existing trace-build script. Current C/H
source and build settings are not rewritten during repository setup.

Each archived source blob was compared byte for byte with its import commit.
Historical snapshots were not rebuilt or re-tested during this import. Tests
previously reported for release packages retain their original scope; creating
Git history is not new runtime validation. No generated executables, logs,
case fixtures or large test data are intentionally tracked. Original upstream
assets and legacy project files are retained.

Future changes should be committed by feature or fix with the relevant tests.
Keep optional compatibility and help review patches separate until approved.

The six snapshot import commits use concise titles. Their shared provenance
and testing qualifications are documented here rather than repeated in each
commit message. These commits record existing snapshots; they do not recreate
the original development chronology or claim that intermediate states were
separately tested. The archive for each snapshot is listed below, and original
author and licence notices remain retained.

## Imported snapshots

| Milestone | Source archive | SHA-256 |
| --- | --- | --- |
| Original WinVi 3.02 | `wvsrc302.zip` | `e62af90a149234375945fd36644ae13aa3846b7c5c286dd872d11ca974bbcd65` |
| Native x64 port | `WinVi-3.02-x64.zip` | `5fdd2189f43f7e0a69fad7ae88eafcdb02dcf1d02b73f45d8396303785f69824` |
| Expanded file-size support | `WinVi-3.02-x64-uncapped.zip` | `1bdfafd1da5a9fd854be71e4f17a24b83ce55b19317a9bf27a277d4c5cb59539` |
| Security hardening | `WinVi-3.02-x64-security.zip` | `38df71d2bacabe321077c35692122b2fffffa2cf3a69793b80d90843c1d78142` |
| Stability fixes | `WinVi-3.02-x64-stability.zip` | `b0d672be2caaef22f4d64a4dc0aca7ba58f3b601787969fad9bdf258040c27bc` |
| Reliability improvements | `WinVi-3.02-x64-reliability.zip` | `48710db2b7eb11a10079a523ab87051a5c1ecf61aa7832488d576db64344fd1a` |
