# Windows compatibility

The x64 release target is Windows 7 SP1 with KB2533623 or a superseding
update providing `SetDefaultDllDirectories`, Windows 8/8.1, Windows 10 and 11.
Windows 7 RTM is rejected. The update is detected by API availability and
successful initialization, rather than an installed-update inventory.
Required DLL-search hardening is never silently disabled.

`build-x64.cmd` and `build-trace.cmd` select MSVC v143 14.44 explicitly, even
when Visual Studio 2026 is installed. Install the optional
`Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64` component and a
Windows SDK. These scripts can run from a regular command prompt. They use
the static CRT (`/MT`), Windows 7 declarations and subsystem version 6.01.
The trace executable is named WinVi64-trace.exe to preserve the release build.
The static CRT avoids a separate Visual C++ redistributable installation.

The manifest declares all five Windows generations. Enhanced GDI scaling is
requested dynamically on newer Windows; Windows 7/8 use system DPI awareness.
Extended absolute file paths remain in use independently of long-path policy.
Other optional newer CRT entry points are resolved dynamically by the v143 CRT.

Commands remain suspended until attached to WinVi's kill-on-close job. Windows
7 cannot nest process jobs. If WinVi inherited a job from its launcher, it
retries a denied assignment using breakaway only on pre-Windows-8 systems.
The launcher must permit breakaway. Otherwise the command fails safely and
no uncontained command is resumed. Windows 8 and later retain nested jobs.

`build-asan.cmd` and all AddressSanitizer diagnostics require Windows 10/11;
they are not legacy distributables. The automated PowerShell/UI tests are
modern-host tooling, not requirements for running the release editor.
`tests/compatibility-test.cmd` uses v143 and has no UI or sound; its `asan`
argument selects modern-host diagnostics. It exercises startup policy and
job-launch failure paths using API doubles, not emulation of an older OS.

Validation on Windows 11 includes release/trace/ASan compilation, PE import
inspection, native regressions, and local editor tests. This does not establish
runtime compatibility on Windows 7/8/8.1/10. Before distributing a supported
legacy release, run on real machines or VMs for each OS: startup; open/save;
Unicode and long paths; dialogs; text and hexadecimal printing; command
execution/cancellation/close; watcher reload; and DPI scaling. Include Windows
7 launchers both permitting and denying job breakaway. Test Windows 7 RTM and
SP1 without the API update for rejection, using disposable VMs.

References:
- [DLL-search hardening and KB2533623](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories)
- [Compiler target support](https://learn.microsoft.com/en-us/cpp/overview/supported-platforms-visual-cpp)
- [Job assignment and pre-Windows-8 restrictions](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject)
- [ASan host requirements](https://learn.microsoft.com/en-us/cpp/sanitizers/asan)
