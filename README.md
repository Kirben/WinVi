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

See [EX-COMMANDS.md](EX-COMMANDS.md) for the ex command additions, their local
regression suites and the separate command-syntax follow-up work.

The MBCS regression suites cover Shift-JIS (932) and Big5 (950) in ANSI and
PC/OEM text modes. Run `tests/mbcs-test.cmd`, `tests/mbcs-clipboard-test.cmd`
and `tests/mbcs-command-test.cmd` (optionally with `asan`), plus
`pwsh -STA -NoProfile -File tests/mbcs.ps1` with `WINVI_TEST_EXE` set to the
build under test. They are also included in `test.ps1`. Clipboard tests use
an isolated fake clipboard and do not replace the system clipboard contents.
