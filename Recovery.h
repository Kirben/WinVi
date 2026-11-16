#ifndef WINVI_RECOVERY_H
#define WINVI_RECOVERY_H
#include <windows.h>
#include "Counts.h"
#define WV_RECOVERY_PATH_CAP 32768
typedef struct {
 DWORD encoding,characterSet,littleEndian;
 LONG newLine;
 WV_ULONG cursor;
 FILETIME sourceTime;
 DWORD newFile;
} WV_RECOVERY_META;
typedef BOOL (*WV_RECOVERY_READ)(void *,BYTE *,DWORD,DWORD *);
typedef BOOL (*WV_RECOVERY_CANCEL)(void *);
typedef struct {
 HANDLE file;
 PWSTR original;
 WV_RECOVERY_META meta;
 WV_ULONG length;
 FILETIME created;
} WV_RECOVERY;
/* The caller supplies a dedicated directory and an absolute original name
   (or an empty name for an unnamed buffer). No document/global UI state. */
BOOL WvRecoverySave(PCWSTR,PCWSTR,const WV_RECOVERY_META *,WV_ULONG,
 WV_RECOVERY_READ,void *,WV_RECOVERY_CANCEL,void *,PWSTR);
BOOL WvRecoveryOpen(PCWSTR,WV_RECOVERY *,WV_RECOVERY_CANCEL,void *);
BOOL WvRecoveryFind(PCWSTR,PCWSTR,WV_RECOVERY *,WV_RECOVERY_CANCEL,void *);
void WvRecoveryClose(WV_RECOVERY *);
#endif
