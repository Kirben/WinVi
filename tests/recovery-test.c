#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static int failure,shortWrites;
static BOOL WINAPI ProbeWrite(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI ProbeFlush(HANDLE);
static BOOL WINAPI ProbeMove(LPCWSTR,LPCWSTR,DWORD);
#define WriteFile ProbeWrite
#define FlushFileBuffers ProbeFlush
#define MoveFileExW ProbeMove
#include "../Recovery.c"
#undef WriteFile
#undef FlushFileBuffers
#undef MoveFileExW
static BOOL WINAPI ProbeWrite(HANDLE h,LPCVOID p,DWORD n,LPDWORD got,LPOVERLAPPED o)
{if(failure==1){*got=0;return TRUE;}if(shortWrites && n>1)n/=2;return WriteFile(h,p,n,got,o);}
static BOOL WINAPI ProbeFlush(HANDLE h)
{if(failure==2){SetLastError(ERROR_DISK_FULL);return FALSE;}return FlushFileBuffers(h);}
static BOOL WINAPI ProbeMove(LPCWSTR a,LPCWSTR b,DWORD flags)
{if(failure==3){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return MoveFileExW(a,b,flags);}
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s (error %lu)\n",__LINE__,#x,GetLastError());exit(1);}}while(0)
typedef struct {WV_ULONG at,length;BOOL fail;} RECOVERY_INPUT;
static BOOL Input(void *context,BYTE *buffer,DWORD capacity,DWORD *got)
{
 RECOVERY_INPUT *i=context;if(i->fail){SetLastError(ERROR_READ_FAULT);return FALSE;}
 *got=i->length-i->at<capacity?(DWORD)(i->length-i->at):capacity;
 for(DWORD n=0;n<*got;n++)buffer[n]=(BYTE)((i->at+n)*17+5);
 i->at+=*got;return TRUE;
}
static BOOL Cancel(void *p){return *(BOOL*)p;}
static void Corrupt(PCWSTR file,LONGLONG at)
{
 HANDLE h=CreateFileW(file,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);BYTE byte=0x79;DWORD n;LARGE_INTEGER pos;
 CHECK(h!=INVALID_HANDLE_VALUE);pos.QuadPart=at;CHECK(SetFilePointerEx(h,pos,NULL,FILE_BEGIN));CHECK(WriteFile(h,&byte,1,&n,NULL) && n==1);CHECK(CloseHandle(h));
}
int wmain(void)
{
 WCHAR directory[WV_RECOVERY_PATH_CAP],file[WV_RECOVERY_PATH_CAP],good[WV_RECOVERY_PATH_CAP],alternate[128],original[]=L"C:\\dummy\\Unicode-\xc4-\x4e2d-\xd83d\xde00.txt";
 WV_RECOVERY_META meta={8,2,0,-4,12,{123,456}};WV_RECOVERY record={0};BOOL cancel=FALSE;RECOVERY_INPUT input={0,200000,FALSE};
 DWORD n;BYTE block[65536];WV_ULONG at;HANDLE h;WIN32_FIND_DATAW found;WCHAR pattern[WV_RECOVERY_PATH_CAP];
 CHECK(GetCurrentDirectoryW(WV_RECOVERY_PATH_CAP,directory)>0);wcscat(directory,L"\\tests\\cases");
 CHECK(CreateDirectoryW(directory,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
 wcscat(directory,L"\\recovery-store");
 CHECK(WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,file));
 {
  DWORD size=0,revision;SECURITY_DESCRIPTOR_CONTROL control;BOOL present,defaulted;PACL dacl;
  GetFileSecurityW(file,DACL_SECURITY_INFORMATION,NULL,0,&size);CHECK(size>0);
  PSECURITY_DESCRIPTOR sd=malloc(size);CHECK(sd!=NULL);
  CHECK(GetFileSecurityW(file,DACL_SECURITY_INFORMATION,sd,size,&size));
  CHECK(GetSecurityDescriptorControl(sd,&control,&revision));CHECK(control&SE_DACL_PROTECTED);
  CHECK(GetSecurityDescriptorDacl(sd,&present,&dacl,&defaulted));CHECK(present && dacl && dacl->AceCount==2);free(sd);
 }
 CHECK(WvRecoveryOpen(file,&record,Cancel,&cancel));CHECK(record.length==input.length && record.meta.cursor==12);
 CHECK(!wcscmp(record.original,original));CHECK(record.meta.sourceTime.dwLowDateTime==123);
 at=0;while(at<record.length){CHECK(ReadFile(record.file,block,sizeof(block),&n,NULL) && n>0);
  for(DWORD i=0;i<n;i++)CHECK(block[i]==(BYTE)((at+i)*17+5));at+=n;}
 CHECK(at==record.length);
 h=CreateFileW(file,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(h==INVALID_HANDLE_VALUE);
 WvRecoveryClose(&record);WvRecoveryClose(&record);
 Corrupt(file,sizeof(RECOVERY_HEADER)+wcslen(original)*2+999);CHECK(!WvRecoveryOpen(file,&record,Cancel,&cancel));CHECK(DeleteFileW(file));
 input.at=0;input.fail=TRUE;CHECK(!WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,file));
 wcscpy(pattern,directory);wcscat(pattern,L"\\*");h=FindFirstFileW(pattern,&found);CHECK(h!=INVALID_HANDLE_VALUE);
 do{CHECK(!wcscmp(found.cFileName,L".") || !wcscmp(found.cFileName,L".."));}while(FindNextFileW(h,&found));FindClose(h);
 input.fail=FALSE;input.at=0;cancel=TRUE;CHECK(!WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,file));cancel=FALSE;
 input.at=input.length=0;meta.cursor=0;CHECK(WvRecoverySave(directory,L"",&meta,0,Input,&input,Cancel,&cancel,file));
 CHECK(WvRecoveryOpen(file,&record,Cancel,&cancel));CHECK(!record.length && !*record.original);WvRecoveryClose(&record);
 Corrupt(file,0);CHECK(!WvRecoveryOpen(file,&record,Cancel,&cancel));CHECK(DeleteFileW(file));
 input.at=0;input.length=1000;meta.cursor=0;shortWrites=TRUE;
 CHECK(WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,good));shortWrites=FALSE;
 CHECK(WvRecoveryOpen(good,&record,Cancel,&cancel));WvRecoveryClose(&record);
 CHECK(WvRecoveryFind(directory,original,&record,Cancel,&cancel));CHECK(record.meta.cursor==0);WvRecoveryClose(&record);
 wcscpy(alternate,original);*wcschr(alternate,0xc4)=0xe4;
 CHECK(WvRecoveryFind(directory,alternate,&record,Cancel,&cancel));WvRecoveryClose(&record);
 input.at=0;meta.cursor=1;
 CHECK(WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,file));
 CHECK(WvRecoveryFind(directory,original,&record,Cancel,&cancel));CHECK(record.meta.cursor==1);WvRecoveryClose(&record);
 Corrupt(file,sizeof(RECOVERY_HEADER)+wcslen(original)*2+200);
 CHECK(WvRecoveryFind(directory,original,&record,Cancel,&cancel));CHECK(record.meta.cursor==0);WvRecoveryClose(&record);CHECK(DeleteFileW(file));
 CHECK(!WvRecoveryFind(directory,L"C:\\unrelated-dummy.txt",&record,Cancel,&cancel));
 cancel=TRUE;CHECK(!WvRecoveryFind(directory,original,&record,Cancel,&cancel));CHECK(GetLastError()==ERROR_CANCELLED);cancel=FALSE;
 for(failure=1;failure<=3;failure++){
  input.at=0;CHECK(!WvRecoverySave(directory,original,&meta,input.length,Input,&input,Cancel,&cancel,file));
  CHECK(WvRecoveryOpen(good,&record,Cancel,&cancel));CHECK(record.length==1000);WvRecoveryClose(&record);
 }
 failure=0;CHECK(DeleteFileW(good));
 meta.encoding=17;CHECK(!WvRecoverySave(directory,original,&meta,0,Input,&input,Cancel,&cancel,file));
 meta.encoding=16;input.length=1;CHECK(!WvRecoverySave(directory,original,&meta,1,Input,&input,Cancel,&cancel,file));
 CHECK(RemoveDirectoryW(directory));printf("PASS %u recovery storage checks\n",checks);return 0;
}
