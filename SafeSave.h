#ifndef WV_SAFE_SAVE_H
#define WV_SAFE_SAVE_H
#include <windows.h>
typedef struct {
 HANDLE File,Original;
 WCHAR *Target,*Temporary,*Backup;
 BOOL Atomic,Exists,RetainRecovery;
 BY_HANDLE_FILE_INFORMATION Identity;
} WV_SAVE;
BOOL WvBeginSave(PCWSTR,BOOL,WV_SAVE*);
BOOL WvCommitSave(WV_SAVE*);
void WvAbortSave(WV_SAVE*);
#endif
