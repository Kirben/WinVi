#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <aclapi.h>
#include <sddl.h>
static int FailFlush,FailReplace;
static BOOL TestFlush(HANDLE h){if(FailFlush){SetLastError(ERROR_DISK_FULL);return FALSE;}return FlushFileBuffers(h);}
static BOOL TestReplace(LPCWSTR target,LPCWSTR stage,LPCWSTR backup,DWORD flags,LPVOID a,LPVOID b){
 if(FailReplace==1){SetLastError(ERROR_SHARING_VIOLATION);return FALSE;}
 if(FailReplace==2){if(!MoveFileExW(target,backup,0))return FALSE;SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT);return FALSE;}
 return ReplaceFileW(target,stage,backup,flags,a,b);
}
#define FlushFileBuffers TestFlush
#define ReplaceFileW TestReplace
#include "../SafeSave.c"
#undef FlushFileBuffers
#undef ReplaceFileW
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s error=%lu\n",__LINE__,#x,GetLastError());return 1;}}while(0)
static BOOL Put(PCWSTR name,const char *text){DWORD n;HANDLE h=CreateFileW(name,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);BOOL ok;if(h==INVALID_HANDLE_VALUE)return FALSE;ok=WriteFile(h,text,(DWORD)strlen(text),&n,NULL)&&n==strlen(text);CloseHandle(h);return ok;}
static BOOL Equal(PCWSTR name,const char *text){char bytes[64]={0};DWORD n=0;HANDLE h=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);BOOL ok;if(h==INVALID_HANDLE_VALUE)return FALSE;ok=ReadFile(h,bytes,63,&n,NULL)&&n==strlen(text)&&!memcmp(bytes,text,n);CloseHandle(h);return ok;}
static BOOL WriteStage(WV_SAVE *s,const char *text){DWORD n=0;return WriteFile(s->File,text,(DWORD)strlen(text),&n,NULL)&&n==strlen(text);}
static PWSTR Dacl(PCWSTR name){PSECURITY_DESCRIPTOR sd=NULL;PWSTR text=NULL;if(GetNamedSecurityInfoW((PWSTR)name,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,NULL,NULL,&sd))return NULL;if(!ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&text,NULL))text=NULL;LocalFree(sd);return text;}
int main(void){
 WV_SAVE s;WCHAR *stage,folder[80],cwd[WV_PATH_CAP];HANDLE lock;FILETIME creation={12345678,29000000},actual;DWORD n;int i;DWORD handlesBefore,handlesAfter;
 CHECK(GetCurrentDirectoryW(WV_PATH_CAP,cwd));_snwprintf(folder,80,L"safe-save-%lu",GetCurrentProcessId());CHECK(CreateDirectoryW(folder,NULL));CHECK(SetCurrentDirectoryW(folder));
 CHECK(Put(L"original.txt","original"));
 {PWSTR before,after;CHECK(Put(L"inherited.txt","old"));before=Dacl(L"inherited.txt");CHECK(before);CHECK(WvBeginSave(L"inherited.txt",FALSE,&s));CHECK(WriteStage(&s,"new"));CHECK(WvCommitSave(&s));WvAbortSave(&s);after=Dacl(L"inherited.txt");CHECK(after&&!wcscmp(before,after));LocalFree(before);LocalFree(after);CHECK(DeleteFileW(L"inherited.txt"));}
 CHECK(WvBeginSave(L"original.txt",FALSE,&s)&&s.Atomic);stage=_wcsdup(s.Temporary);CHECK(stage);CHECK(WriteStage(&s,"new"));CHECK(Equal(L"original.txt","original"));WvAbortSave(&s);CHECK(Equal(L"original.txt","original"));CHECK(GetFileAttributesW(stage)==INVALID_FILE_ATTRIBUTES);free(stage);
 CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"new"));FailFlush=1;CHECK(!WvCommitSave(&s)&&GetLastError()==ERROR_DISK_FULL);FailFlush=0;WvAbortSave(&s);CHECK(Equal(L"original.txt","original"));
 CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"new"));FailReplace=1;CHECK(!WvCommitSave(&s));FailReplace=0;WvAbortSave(&s);CHECK(Equal(L"original.txt","original"));
 CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"recoverable"));stage=_wcsdup(s.Temporary);FailReplace=2;CHECK(!WvCommitSave(&s)&&s.RetainRecovery);FailReplace=0;WvAbortSave(&s);CHECK(Equal(L"original.txt","original")&&Equal(stage,"recoverable"));CHECK(DeleteFileW(stage));free(stage);
 lock=CreateFileW(L"original.txt",GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);CHECK(lock!=INVALID_HANDLE_VALUE);CHECK(SetFileTime(lock,&creation,NULL,NULL));CloseHandle(lock);
 CHECK(Put(L"original.txt:metadata","stream"));
 CHECK(SetFileAttributesW(L"original.txt",FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_NOT_CONTENT_INDEXED));
 {PSECURITY_DESCRIPTOR sd=NULL;CHECK(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)",SDDL_REVISION_1,&sd,NULL));CHECK(SetFileSecurityW(L"original.txt",DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd));LocalFree(sd);}
 CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"saved"));CHECK(WvCommitSave(&s));WvAbortSave(&s);CHECK(Equal(L"original.txt","saved")&&Equal(L"original.txt:metadata","stream"));
 CHECK((GetFileAttributesW(L"original.txt")&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_NOT_CONTENT_INDEXED))==(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_NOT_CONTENT_INDEXED));
 lock=CreateFileW(L"original.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(lock!=INVALID_HANDLE_VALUE);CHECK(GetFileTime(lock,&actual,NULL,NULL)&&!CompareFileTime(&creation,&actual));
 CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"blocked"));CHECK(!WvCommitSave(&s));WvAbortSave(&s);CloseHandle(lock);CHECK(Equal(L"original.txt","saved"));
 CHECK(WvBeginSave(L"new.txt",FALSE,&s));CHECK(WriteStage(&s,"mine"));CHECK(Put(L"new.txt","other"));CHECK(!WvCommitSave(&s));WvAbortSave(&s);CHECK(Equal(L"new.txt","other"));CHECK(DeleteFileW(L"new.txt"));
 CHECK(WvBeginSave(L"new.txt",FALSE,&s));CHECK(WriteStage(&s,"created"));CHECK(WvCommitSave(&s));WvAbortSave(&s);CHECK(Equal(L"new.txt","created"));CHECK(DeleteFileW(L"new.txt"));
 CHECK(CreateHardLinkW(L"alias.txt",L"original.txt",NULL));CHECK(WvBeginSave(L"original.txt",FALSE,&s)&&!s.Atomic);CHECK(WriteStage(&s,"linked"));CHECK(WvCommitSave(&s));WvAbortSave(&s);CHECK(Equal(L"alias.txt","linked"));CHECK(DeleteFileW(L"alias.txt"));
 CHECK(WvBeginSave(L"original.txt",TRUE,&s)&&!s.Atomic);{LARGE_INTEGER zero={0};CHECK(SetFilePointerEx(s.File,zero,NULL,FILE_END));}CHECK(WriteStage(&s,"+tail"));CHECK(WvCommitSave(&s));WvAbortSave(&s);CHECK(Equal(L"original.txt","linked+tail"));
 CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesBefore));
 for(i=0;i<100;i++){CHECK(WvBeginSave(L"original.txt",FALSE,&s));CHECK(WriteStage(&s,"abort"));WvAbortSave(&s);}
 CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter)&&handlesAfter==handlesBefore);
 CHECK(DeleteFileW(L"original.txt"));CHECK(SetCurrentDirectoryW(cwd));CHECK(RemoveDirectoryW(folder));
 puts("PASS staged save/abort, flush and replace failures, partial-replace recovery, metadata/ADS, sharing, racing create, hard links, append and 100 handle-cleanup cycles");return 0;
}
