/* Same-volume staged saves. GPL-2.0-or-later. */
#include <windows.h>
#include <aclapi.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <wchar.h>
#include "Platform.h"
#include "SafeSave.h"

static BOOL SaveName(WCHAR *out,PCWSTR target,PCWSTR extension)
{
 BYTE bytes[16];size_t i,prefix;PCWSTR slash=wcsrchr(target,L'\\');
 static const WCHAR hex[]=L"0123456789abcdef";
 if(!slash){SetLastError(ERROR_INVALID_NAME);return FALSE;}
 prefix=slash-target+1;
 if(prefix+55>=WV_PATH_CAP-16){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
 if(BCryptGenRandom(NULL,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)){SetLastError(ERROR_GEN_FAILURE);return FALSE;}
 memcpy(out,target,prefix*sizeof(WCHAR));WvCopyString(out+prefix,L"~wv-save-");
 for(i=0;i<16;i++){out[prefix+9+2*i]=hex[bytes[i]>>4];out[prefix+10+2*i]=hex[bytes[i]&15];}
 WvCopyString(out+prefix+41,extension);return TRUE;
}
void WvAbortSave(WV_SAVE *s)
{
 DWORD error=GetLastError();
 if(s->File && s->File!=INVALID_HANDLE_VALUE)CloseHandle(s->File);
 if(s->Original && s->Original!=INVALID_HANDLE_VALUE)CloseHandle(s->Original);
 if(s->Temporary && !s->RetainRecovery)DeleteFileW(s->Temporary);
 free(s->Target);free(s->Temporary);free(s->Backup);memset(s,0,sizeof(*s));SetLastError(error);
}
BOOL WvBeginSave(PCWSTR name,BOOL append,WV_SAVE *s)
{
 DWORD attr,error;PSECURITY_DESCRIPTOR sd=NULL;SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,FALSE};int attempt;
 memset(s,0,sizeof(*s));s->File=s->Original=INVALID_HANDLE_VALUE;
 s->Target=WvAbsolutePath(name);if(!s->Target)return FALSE;
 attr=GetFileAttributesW(s->Target);error=GetLastError();
 s->Exists=attr!=INVALID_FILE_ATTRIBUTES;
 if(!s->Exists && error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND)goto fail;
 if(s->Exists) {
  s->Original=CreateFileW(s->Target,GENERIC_READ|READ_CONTROL,
   FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(s->Original==INVALID_HANDLE_VALUE)goto fail;
  if(GetFileType(s->Original)!=FILE_TYPE_DISK || !GetFileInformationByHandle(s->Original,&s->Identity)){SetLastError(ERROR_NOT_SUPPORTED);goto fail;}
 }
 /* Preserve explicit append and hard-link/reparse/stream identity semantics. */
 s->Atomic=!append && (!s->Exists || (s->Identity.nNumberOfLinks==1 && !(attr&FILE_ATTRIBUTE_REPARSE_POINT))) && !wcschr(s->Target+6,L':');
 if(!s->Atomic) {
  if(s->Original!=INVALID_HANDLE_VALUE){CloseHandle(s->Original);s->Original=INVALID_HANDLE_VALUE;}
  s->File=CreateFileW(s->Target,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  if(s->File==INVALID_HANDLE_VALUE)goto fail;return TRUE;
 }
 if(s->Exists) {
  error=GetSecurityInfo(s->Original,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,NULL,NULL,&sd);
  if(error){SetLastError(error);goto fail;}
  if(!SetSecurityDescriptorControl(sd,SE_DACL_PROTECTED,SE_DACL_PROTECTED))goto fail;
  sa.lpSecurityDescriptor=sd;
 }
 s->Temporary=calloc(WV_PATH_CAP,sizeof(WCHAR));s->Backup=calloc(WV_PATH_CAP,sizeof(WCHAR));
 if(!s->Temporary || !s->Backup){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
 for(attempt=0;attempt<16;attempt++){
  if(!SaveName(s->Temporary,s->Target,L".tmp") || !SaveName(s->Backup,s->Target,L".bak"))goto fail;
  s->File=CreateFileW(s->Temporary,GENERIC_WRITE,0,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  if(s->File!=INVALID_HANDLE_VALUE){if(sd)LocalFree(sd);return TRUE;}
  if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)goto fail;
  s->Temporary[0]=0;
 }
 SetLastError(ERROR_FILE_EXISTS);
fail:
 error=GetLastError();if(sd)LocalFree(sd);
 /* Never remove a name which was not successfully created by this operation. */
 if(s->File==INVALID_HANDLE_VALUE && s->Temporary)s->Temporary[0]=0;
 WvAbortSave(s);SetLastError(error);return FALSE;
}
BOOL WvCommitSave(WV_SAVE *s)
{
 HANDLE check;BY_HANDLE_FILE_INFORMATION now;DWORD error;
 if(!SetEndOfFile(s->File) || !FlushFileBuffers(s->File))return FALSE;
 if(!CloseHandle(s->File)){s->File=INVALID_HANDLE_VALUE;return FALSE;}s->File=INVALID_HANDLE_VALUE;
 if(!s->Atomic)return TRUE;
 if(s->Exists){
  check=CreateFileW(s->Target,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
  if(check==INVALID_HANDLE_VALUE)return FALSE;
  if(!GetFileInformationByHandle(check,&now)){error=GetLastError();CloseHandle(check);SetLastError(error);return FALSE;}CloseHandle(check);
  if(now.dwVolumeSerialNumber!=s->Identity.dwVolumeSerialNumber || now.nFileIndexHigh!=s->Identity.nFileIndexHigh || now.nFileIndexLow!=s->Identity.nFileIndexLow){SetLastError(ERROR_FILE_INVALID);return FALSE;}
  if(!ReplaceFileW(s->Target,s->Temporary,s->Backup,0,NULL,NULL)){
   error=GetLastError();
   if(GetFileAttributesW(s->Backup)!=INVALID_FILE_ATTRIBUTES){
    s->RetainRecovery=TRUE;
    if(GetFileAttributesW(s->Target)==INVALID_FILE_ATTRIBUTES)MoveFileExW(s->Backup,s->Target,MOVEFILE_WRITE_THROUGH);
   }
   SetLastError(error);return FALSE;
  }
  CloseHandle(s->Original);s->Original=INVALID_HANDLE_VALUE;
  DeleteFileW(s->Backup);
 } else if(!MoveFileExW(s->Temporary,s->Target,MOVEFILE_WRITE_THROUGH))return FALSE;
 s->Temporary[0]=0;return TRUE;
}
