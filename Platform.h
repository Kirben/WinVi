#ifndef WINVI_PLATFORM_H
#define WINVI_PLATFORM_H
#include <commdlg.h>
#include <string.h>
#include <wchar.h>
/* Legacy path handling sometimes copies a string onto itself or an overlapping prefix. */
static __inline WCHAR *WvCopyString(WCHAR *dst,const WCHAR *src) { return (WCHAR*)memmove(dst,src,(wcslen(src)+1)*sizeof(WCHAR)); }
#define WV_PATH_CAP 32768
#define WV_TEXT_CAP (WV_PATH_CAP * 2 + 1024)
BOOL InitializeWindowsPlatform(void);
BOOL WvCreateJobProcessW(LPCWSTR,LPWSTR,LPSTARTUPINFOW,HANDLE,LPPROCESS_INFORMATION);
LPWSTR WvAbsolutePath(LPCWSTR);
BOOL WvResolveShellExecutable(LPCSTR,LPWSTR,DWORD);
BOOL WvCreateProcessW(LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,LPCWSTR,LPSTARTUPINFOW,LPPROCESS_INFORMATION);
HANDLE WvCreateFileW(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
BOOL WvDeleteFileW(LPCWSTR);
DWORD WvGetFileAttributesW(LPCWSTR);
BOOL WvGetFileAttributesExW(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
BOOL WvSetFileAttributesW(LPCWSTR,DWORD);
HANDLE WvFindFirstFileW(LPCWSTR,LPWIN32_FIND_DATAW);
BOOL WvSetCurrentDirectoryW(LPCWSTR);
DWORD WvGetCurrentDirectoryW(DWORD,LPWSTR);
BOOL WvOpenFileDialog(LPOPENFILENAMEW,BOOL);
BOOL ResolveWinViHelpUrl(LPCSTR,LPCWSTR,LPCWSTR,LPWSTR,DWORD);
BOOL OpenWinViUrlW(LPCWSTR);
#ifndef WINVI_PLATFORM_IMPLEMENTATION
#define CreateFileW WvCreateFileW
#define CreateProcessW WvCreateProcessW
#define DeleteFileW WvDeleteFileW
#define GetFileAttributesW WvGetFileAttributesW
#define GetFileAttributesExW WvGetFileAttributesExW
#define SetFileAttributesW WvSetFileAttributesW
#define FindFirstFileW WvFindFirstFileW
#define SetCurrentDirectoryW WvSetCurrentDirectoryW
#define GetCurrentDirectoryW WvGetCurrentDirectoryW
#define GetOpenFileNameW(p) WvOpenFileDialog(p,FALSE)
#define GetSaveFileNameW(p) WvOpenFileDialog(p,TRUE)
#endif
#endif

