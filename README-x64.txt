WinVi 3.02 native x64 community port - reliability update
Windows 7 SP1 + KB2533623 (or superseding update), 8/8.1, 10 and 11 x64

Extract the ZIP and run WinVi64.exe. Keep WinVi-en.htm and WinVi-de.htm beside it.
The original GPL license and author notices are retained. See COPYING.txt.

This update adds:
- Staged ordinary saves: write a same-directory temporary file, flush it, then replace
  the destination. Write/flush failures do not overwrite the original. Replacement
  failures retain recovery data when necessary. Existing creation time, ACLs and named
  streams are preserved through the Windows replacement API.
- Cooperative command/file-watcher shutdown, operation-owned buffers and contexts,
  checked worker/event creation, and generation-checked asynchronous completion.
- A single portable build-and-test entry point, with isolated per-build scratch data
  and machine-readable results.

Append writes, hard-linked files, reparse-point paths and alternate data streams retain
in-place writing to preserve their identity/append semantics; those writes are not
transactional. Ordinary staged saves need space for an additional copy of the file and
permission to create/replace files in the destination directory. There is no unsafe
in-place fallback if staging fails.

If an unusual replacement failure leaves recovery files, WinVi reports their paths.
Files named ~wv-save-<random>.tmp/.bak belong to staged saves. After a forced process
termination a partial temporary file can remain; the original is not overwritten until
the commit step. Do not assume a temporary file contains a complete document.

Shell commands run in an owned job. Cancelling a command or closing its editor stops
that command and its descendants. Background descendants also end with their command
session. Command output remains editable/undoable through the editor's normal mechanisms.

The earlier x64, Unicode, security and large-file changes remain. There is no fixed GiB
file-size cap; real limits include signed 64-bit positions, memory, filesystem capacity,
undo/yank allocation and temporary disk space. Historical audits describe earlier builds;
RELIABILITY-AUDIT.txt and test-results describe this release.

Build
Install MSVC v143 14.44 C++ x64 tools and a Windows SDK (see WINDOWS-COMPATIBILITY.md):
  source\build-x64.cmd
For Windows 10/11 AddressSanitizer diagnostics, install the MSVC ASan component:
  source\build-asan.cmd
Release uses the static CRT. No separate Visual C++ redistributable is needed.

Unified verification (PowerShell 7 plus MSVC x64 tools and the ASan component):
  pwsh -NoProfile -File source\test.ps1 -Configuration Both
  pwsh -NoProfile -File source\test.ps1 -Configuration Both -LargeFiles -LargeGiB 17 -DenseLines
Options:
  -Configuration Release|Asan|Both
  -SkipBuild        skip rebuilding application executables; native helpers still build
  -LargeFiles       run full-content generated-file open/edit/undo/save/hash tests
  -LargeGiB N       generated input size, default 17 GiB
  -DenseLines       use two-byte lines for line-count stress
The runner writes isolated cases, logs, JSON results and executable hashes under
source/tests/results/<timestamp>. It runs formatting/path/size/I/O/pager/hex tests,
focused ASan regressions, save-failure injection, editing, read-failure preservation,
regex cancellation and command/watcher lifecycle checks.

The runner builds x64/WinVi64-reliable.exe and asan/WinVi64.exe. Individual tests accept
WINVI_TEST_EXE as an absolute executable path. Tests operate on local dummy data and
close only their own child processes. Inherited security tests mock network enumeration
and unsafe dispatch; their trusted shell test runs System32 cmd.exe with "exit 0".

Allow ample temporary disk space for large tests: input, paging store and staged output
can coexist. Large fixtures are removed by default. Timings reflect host load, not a
controlled performance benchmark.

Runtime verification here used Windows 11. Windows 10 remains the supported minimum
target but was not separately runtime-tested on another installation.

Original upstream source: https://www.winvi.de/wvsrc302.zip
Original upstream help: https://www.winvi.de/winvi302.zip
An existing public source mirror/VS project is https://github.com/FigBug/WinVi .
This community port is distinct from the upstream 32-bit 3.02 release.

Automatic line splitting (off by default)
Edit > Automatic line splitting toggles hard wrapping of ordinary typed insert-mode
text. Settings > Edit > Width (columns) accepts 1-1000, default 80. Settings can be
saved per profile. :set splitlines / nosplitlines and :set splitwidth=80 use the
same settings. Tabs follow TabStop; Unicode scalars count as one logical column.
Newlines and AutoIndent are preserved; whole words are not broken. Paste, file
insertion, filters, hex and replace modes bypass automatic splitting. Existing
lines beyond an 8192-character look-back are left intact for responsiveness.
Under page-storage failure, an undoable partial newline/indent insertion may
remain; original words and separator are retained if insertion cannot complete.
