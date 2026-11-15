WinVi 3.02 — native x64 port, Windows 10/11

Security update (15 November 2026): see SECURITY-AUDIT.txt for fixes and evidence.

Extract the ZIP and run WinVi64.exe. Keep WinVi-en.htm and WinVi-de.htm beside it for Help.
There is no fixed GiB file-size cap. Capacity depends on available memory, temporary disk
space, the filesystem, and signed 64-bit file positions. See SIZE-AUDIT.txt for evidence
and the remaining operation-specific limits.

Source and original GPL license are included in source/ and COPYING.txt.
Build on Windows with Visual Studio C++ x64 tools and Windows SDK:
  source\build-x64.cmd
The rebuilt executable and manuals are placed in source\x64\.
The release uses the static CRT; no separate Visual C++ redistributable is needed.
For an AddressSanitizer build, install the MSVC ASan component and run source\build-asan.cmd.

Tests require PowerShell 7; native helper tests require the C++ build tools.
  pwsh -NoProfile -File source\tests\integration.ps1
  pwsh -NoProfile -File source\tests\read-failure.ps1
  source\tests\security-regression.cmd
  source\tests\hex-layout-test.cmd
  source\tests\page-fault-test.cmd
  source\tests\size-test.cmd
  source\tests\io-test.cmd
  source\tests\format-test.cmd
  source\tests\platform-test.cmd
By default the PowerShell tests use the shipped executable. Set WINVI_TEST_EXE to an
absolute executable path to test a rebuilt or sanitizer version.

Optional full-content stress tests:
  pwsh -NoProfile -File source\tests\large-audit.ps1 -GiB 17 -DenseLines
  pwsh -NoProfile -File source\tests\large-audit.ps1 -GiB 32
Allow disk space for both the generated input and the editor's temporary page store.
Large fixtures are removed by default; -KeepFile retains them. -ReuseInput restores and
reuses a previously generated -KeepFile fixture. Do not run two tests on the same fixture.
The tests edit only their scratch files under source\tests\cases and close their own
editor processes. They also exercise normal editor settings and recent-file behavior.

The original source README and most of the original help are historical documentation.
The x64 port notes supersede their old operating-system and file-size statements.
Upstream source: https://www.winvi.de/wvsrc302.zip
Upstream help distribution: https://www.winvi.de/winvi302.zip

Security regressions require the MSVC ASan component. They intercept unsafe tag
commands and mock network APIs. They launch trusted System32 cmd.exe with exit 0
and force-terminate their own temporary child to check paging-file cleanup.

Security restrictions: tags accept numeric/search addresses, not general Ex commands.
Shell formats must start with %0 or quoted %0. Bare cmd.exe resolves to System32.
Background BMPs must be validated, uncompressed BI_RGB. See SECURITY-AUDIT.txt.
