# WinVi x64 community continuation

Unofficial continuation of Raphael Molle's WinVi 3.02. Original project:
https://www.winvi.de/en/ . Source archive: https://www.winvi.de/wvsrc302.zip .
The original author notices and GPL licence remain in the source and COPYING.txt.

The release targets Windows 7 SP1 with KB2533623 (or a superseding update),
Windows 8/8.1, 10 and 11 x64. See WINDOWS-COMPATIBILITY.md for requirements,
toolset selection and the legacy runtime validation still needed. See README-x64.txt for build and test instructions;
when working from this repository, run test.ps1 in this directory (the older
packaged instructions use a source/ prefix).

History starts with the unmodified official 3.02 source,
then imports preserved snapshots of the x64, large-file, security, stability
and reliability work. See HISTORY-IMPORT.md for provenance and limitations.
Unapplied review patches outside this directory are not part of this history.
