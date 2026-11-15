#include "Counts.h"
/* Native handle adapters for the legacy Windows file I/O calls. GPL-2.0-or-later. */
#if defined(_WIN64)
#ifndef WINVI64_COMPAT_H
#define WINVI64_COMPAT_H
#define HFILE INT_PTR
#undef HFILE_ERROR
#define HFILE_ERROR ((HFILE)-1)
static UINT Win64Read(HFILE h, void *p, UINT n) {
 DWORD done; return ReadFile((HANDLE)h,p,n,&done,NULL) ? done : (UINT)-1;
}
static UINT Win64Write(HFILE h, const void *p, UINT n) {
 DWORD done;
 if (!n) return SetEndOfFile((HANDLE)h) ? 0 : (UINT)-1;
 return WriteFile((HANDLE)h,p,n,&done,NULL) ? done : (UINT)-1;
}
static WV_LONG Win64Seek(HFILE h,WV_LONG n,int origin) {
 LARGE_INTEGER distance,result; distance.QuadPart=n; return SetFilePointerEx((HANDLE)h,distance,&result,origin) ? result.QuadPart : -1;
}
static HFILE Win64Open(LPCSTR name,LPOFSTRUCT info,UINT mode) {
 (void)info;
 return (HFILE)CreateFileA(name, (mode & OF_READWRITE) ? GENERIC_READ|GENERIC_WRITE : ((mode & OF_WRITE) ? GENERIC_WRITE : GENERIC_READ), FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,(mode & OF_CREATE) ? CREATE_ALWAYS : OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
}
#define _lread Win64Read
#define _hread Win64Read
#define _lwrite Win64Write
#define _hwrite Win64Write
#define _llseek Win64Seek
#define _lclose(h) (CloseHandle((HANDLE)(h)) ? 0 : HFILE_ERROR)
#define OpenFile Win64Open
#endif
#endif
