/* Quiet policy/failure tests: no dialogs, sound, or child processes. */
#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static DWORD major=6,minor=1;static WORD sp=1;
static int missing,dllFail,dpiMode,dpiFallback,dpiEnhanced,dllCalls;
static int creates,assigns,terminates,closes,inJob=1,queryFail,terminateFail,createFail,assignFail=1,versionFail;
static DWORD assignError=ERROR_ACCESS_DENIED,flagsSeen[3];
static BOOL WINAPI FakeVersion(LPOSVERSIONINFOEXW v,DWORD type,DWORDLONG mask){
 BOOL ok;
 if(versionFail){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if(type!=(VER_MAJORVERSION|VER_MINORVERSION|VER_SERVICEPACKMAJOR) || !mask)abort();
 ok=major>v->dwMajorVersion || (major==v->dwMajorVersion && (minor>v->dwMinorVersion || (minor==v->dwMinorVersion && sp>=v->wServicePackMajor)));
 if(!ok)SetLastError(ERROR_OLD_WIN_VERSION);return ok;
}
static BOOL WINAPI FakeDirectories(DWORD flags){dllCalls++;if(flags!=(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS))abort();if(dllFail){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return TRUE;}
static BOOL WINAPI FakeContext(HANDLE context){if(context!=(HANDLE)(INT_PTR)-5)abort();dpiEnhanced++;return dpiMode==2;}
static FARPROC WINAPI FakeProc(HMODULE module,LPCSTR name){(void)module;if(!strcmp(name,"SetDefaultDllDirectories"))return missing?NULL:(FARPROC)FakeDirectories;if(!strcmp(name,"SetProcessDpiAwarenessContext"))return dpiMode?(FARPROC)FakeContext:NULL;abort();return NULL;}
static BOOL WINAPI FakeDpi(void){dpiFallback++;return TRUE;}
static BOOL WINAPI FakeCreate(LPCWSTR app,LPWSTR line,LPSECURITY_ATTRIBUTES a,LPSECURITY_ATTRIBUTES b,BOOL inherit,DWORD flags,LPVOID env,LPCWSTR dir,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION process){
 (void)app;(void)a;(void)b;(void)env;(void)dir;(void)startup;
 creates++;flagsSeen[creates-1]=flags;if(!inherit || !(flags&CREATE_SUSPENDED) || wcscmp(line,L"dummy original"))abort();
 line[0]=L'X'; /* CreateProcess may modify its command-line buffer. */
 if(creates==createFail){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
 process->hProcess=(HANDLE)(INT_PTR)(100+creates);process->hThread=(HANDLE)(INT_PTR)(200+creates);return TRUE;
}
static BOOL WINAPI FakeAssign(HANDLE job,HANDLE process){(void)job;(void)process;assigns++;if(assigns<=assignFail){SetLastError(assignError);return FALSE;}return TRUE;}
static BOOL WINAPI FakeInJob(HANDLE process,HANDLE job,PBOOL result){(void)process;(void)job;if(queryFail){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}*result=inJob;return TRUE;}
static BOOL WINAPI FakeTerminate(HANDLE process,UINT code){(void)process;(void)code;terminates++;if(terminateFail){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}return TRUE;}
static BOOL WINAPI FakeClose(HANDLE handle){(void)handle;closes++;return TRUE;}
#define VerifyVersionInfoW FakeVersion
#define GetProcAddress FakeProc
#define SetProcessDPIAware FakeDpi
#define CreateProcessW FakeCreate
#define AssignProcessToJobObject FakeAssign
#define IsProcessInJob FakeInJob
#define TerminateProcess FakeTerminate
#define CloseHandle FakeClose
#include "../Platform.c"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s (error %lu)\n",__LINE__,#x,GetLastError());return 1;}}while(0)
static void Reset(void){creates=assigns=terminates=closes=queryFail=terminateFail=createFail=versionFail=0;inJob=1;assignFail=1;assignError=ERROR_ACCESS_DENIED;major=6;minor=1;sp=1;}
static BOOL Launch(void){WCHAR line[64]=L"dummy original";STARTUPINFOEXW startup={0};PROCESS_INFORMATION process={0};startup.StartupInfo.cb=sizeof(startup);return WvCreateJobProcessW(NULL,line,&startup.StartupInfo,(HANDLE)99,&process);}
int main(void){int i,j;DWORD versions[][3]={{6,0,2},{6,1,0},{6,1,1},{6,2,0},{6,3,0},{10,0,0}};
 for(i=0;i<6;i++)for(j=0;j<3;j++){
  major=versions[i][0];minor=versions[i][1];sp=(WORD)versions[i][2];dpiMode=j;dllCalls=dpiFallback=dpiEnhanced=0;
  CHECK(InitializeWindowsPlatform()==(i>=2));CHECK(dllCalls==(i>=2));
  CHECK(dpiFallback==((i>=2 && j!=2)?1:0));CHECK(dpiEnhanced==((i>=2 && j)?1:0));
 }
 Reset();missing=1;CHECK(!InitializeWindowsPlatform());CHECK(GetLastError()==ERROR_PROC_NOT_FOUND);missing=0;
 dllFail=1;CHECK(!InitializeWindowsPlatform());CHECK(GetLastError()==ERROR_ACCESS_DENIED);dllFail=0;
 versionFail=1;CHECK(!InitializeWindowsPlatform());Reset();assignFail=0;CHECK(Launch());CHECK(creates==1 && assigns==1 && !terminates);
 Reset();CHECK(Launch());CHECK(creates==2 && assigns==2 && terminates==1 && closes==2);CHECK(!(flagsSeen[0]&CREATE_BREAKAWAY_FROM_JOB));CHECK(flagsSeen[1]&CREATE_BREAKAWAY_FROM_JOB);
 Reset();createFail=1;CHECK(!Launch());CHECK(GetLastError()==ERROR_ACCESS_DENIED && creates==1 && !assigns);
 Reset();createFail=2;CHECK(!Launch());CHECK(creates==2 && assigns==1 && closes==2);
 Reset();assignFail=2;CHECK(!Launch());CHECK(creates==2 && assigns==2 && closes==2);
 Reset();assignError=ERROR_INVALID_HANDLE;CHECK(!Launch());CHECK(GetLastError()==ERROR_INVALID_HANDLE && creates==1 && !terminates);
 Reset();inJob=0;CHECK(!Launch());CHECK(creates==1 && !terminates);
 Reset();queryFail=1;CHECK(!Launch());CHECK(creates==1 && !terminates);
 Reset();terminateFail=1;CHECK(!Launch());CHECK(GetLastError()==ERROR_INVALID_HANDLE && creates==1 && !closes);
 Reset();minor=2;sp=0;CHECK(!Launch());CHECK(creates==1 && !terminates);
 Reset();major=10;minor=0;sp=0;CHECK(!Launch());CHECK(creates==1 && !terminates);
 Reset();versionFail=1;CHECK(!Launch());CHECK(creates==1 && !terminates);
 puts("PASS 18 startup/version/DPI combinations, 3 startup failures, 12 suspended-job success/failure scenarios");return 0;
}
