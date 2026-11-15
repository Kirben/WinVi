WinVi 3.02 native x64 port

Based on Raphael Molle's WinVi 3.02 source archive:
https://www.winvi.de/wvsrc302.zip
Original copyright and GPL-2.0-or-later terms apply; see COPYING.txt.
This is an unofficial modified build.

Run WinVi64.exe directly. The C runtime is statically linked.

Build from source on Windows with Visual Studio C++ x64 tools and a Windows SDK:
  build-x64.cmd
The result is x64\WinVi64.exe. Build diagnostics are in x64\build.log.

Changes:
- Pointer-sized file handles and native Win32 file I/O adapters.
- Pointer-sized tooltip storage, font handles and dialog callback returns.
- Pointer-sized interprocess message result and control window parameters.
- Modern MSVC snprintf compatibility and a native amd64 controls manifest.
- Reproducible x64 build script using Visual Studio discovery.

Validation:
- PE header checked for AMD64 (0x8664), PE32+ (0x20B).
- Launched the application, opened a text file, inserted a character,
  saved it, compared the exact resulting contents and closed the program.

Limits:
This port retains the original editor's 32-bit file lengths and offsets.
A 64-bit executable does not imply support for files larger than 2 GB.
The old help system and all editor commands have not been exhaustively tested.
Compiler warnings remain for legacy integer sizes and deprecated Windows APIs.
