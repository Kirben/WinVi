#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static int checks,calls,failAt,allocs,cancel,running,launchFail,resolveFail;
static BOOL inherit;static DWORD flags;static WCHAR app[32768],launched[33000],commands[8192],entered[8192];
static void *Allocate(size_t n){return ++allocs==failAt?NULL:malloc(n);}
static void *Grow(void *p,size_t n){return ++allocs==failAt?NULL:realloc(p,n);}
#define malloc Allocate
#define realloc Grow
#define NewStatus ProbeNotice
#define CommandExec ProbeCommand
#define CheckInterrupt ProbeInterrupt
#define StartInterruptCheck ProbeStart
#define ExecRunning ProbeRunning
#define MessageLoop ProbeLoop
#define Enable ProbeEnable
#define WvResolveShellExecutable ProbeResolve
#define WvCreateProcessW ProbeCreate
#include "../ExScript.c"
#undef malloc
#undef realloc
#undef CommandExec
BOOL CommandExec(PWSTR);
void ProbeNotice(INT a,PWSTR b,INT c){(void)a;(void)b;(void)c;}
BOOL ProbeInterrupt(void){return cancel;}
void ProbeStart(void){Interrupted=FALSE;}
BOOL ProbeRunning(void){return running;}
INT ProbeLoop(BOOL wait){(void)wait;running=0;return 0;}
void ProbeEnable(void){Disabled=FALSE;}
BOOL ProbeResolve(LPCSTR name,LPWSTR out,DWORD cap){(void)name;(void)cap;if(resolveFail)return FALSE;wcscpy(out,app);return TRUE;}
BOOL ProbeCreate(LPCWSTR name,LPWSTR line,LPSECURITY_ATTRIBUTES a,LPSECURITY_ATTRIBUTES b,BOOL h,DWORD f,LPVOID env,LPCWSTR cwd,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION p){
 (void)a;(void)b;(void)env;(void)cwd;(void)startup;inherit=h;flags=f;wcscpy(launched,line);if(wcscmp(name,app))exit(2);
 if(launchFail)return FALSE;p->hProcess=CreateEvent(NULL,FALSE,FALSE,NULL);p->hThread=CreateEvent(NULL,FALSE,FALSE,NULL);return TRUE;
}
BOOL ProbeCommand(PWSTR line){
 PWSTR text=NULL;calls++;
 if(wcslen(commands)+wcslen(line)+2<8192){wcscat(commands,line);wcscat(commands,L"\n");}
 if(!wcscmp(line,L":filter"))running=1;
 if(!wcscmp(line,L":fail"))return FALSE;
 if(!wcscmp(line,L":append")){BOOL ok=WvExSourceText(&text);if(ok){wcsncpy(entered,text,8191);entered[8191]=0;}free(text);return ok;}
 if(!wcsncmp(line,L":source ",8))return WvExSource(line+8);
 if(!wcscmp(line,L":quit"))hwndMain=NULL;
 return TRUE;
}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static WCHAR file[32768];
static void Reset(void){calls=allocs=failAt=cancel=running=launchFail=resolveFail=0;commands[0]=entered[0]=launched[0]=0;WvExGlobalActive=Disabled=FALSE;hwndMain=(HWND)1;}
static void Write(const void *bytes,DWORD length){HANDLE h=WvCreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n;CHECK(h!=INVALID_HANDLE_VALUE);CHECK(WriteFile(h,bytes,length,&n,NULL)&&n==length);CloseHandle(h);}
int main(void){
 WCHAR dir[32768];int i;DWORD before,after;
 extern BOOL SuppressStatusBeep;SuppressStatusBeep=TRUE;
 CHECK(GetTempPathW(32768,dir));CHECK(GetTempFileNameW(dir,L"wvs",0,file));
 Reset();Write("\" comment\nprint\r\n:2delete\rset magic",strlen("\" comment\nprint\r\n:2delete\rset magic"));
 CHECK(WvExSource(file));CHECK(calls==3 && !wcscmp(commands,L":print\n:2delete\n:set magic\n"));CHECK(!Source);
 Reset();Write("append\n\nX\n.\nprint\n",18);CHECK(WvExSource(file));CHECK(calls==2 && !wcscmp(entered,L"\r\nX\r\n"));
 Reset();Write("append\nlast",11);CHECK(WvExSource(file));CHECK(!wcscmp(entered,L"last\r\n"));
 Reset();Write("print\nfail\ndelete\n",18);CHECK(!WvExSource(file));CHECK(calls==2 && !Source);
 Reset();Write("quit\ndelete\n",12);CHECK(WvExSource(file));CHECK(calls==1 && !Source);
 {WCHAR script[32768];int n=swprintf(script,32768,L"source %s",file);int bytes=WideCharToMultiByte(CP_UTF8,0,script,n,NULL,0,NULL,NULL);char *b=malloc(bytes);WideCharToMultiByte(CP_UTF8,0,script,n,b,bytes,NULL,NULL);Reset();Write(b,bytes);free(b);CHECK(!WvExSource(file));CHECK(calls==1 && !Source);}
 {BYTE le[]={0xff,0xfe,'p',0,'r',0,'i',0,'n',0,'t',0,10,0},be[sizeof(le)];for(i=0;i<sizeof(le);i+=2){be[i]=le[i+1];be[i+1]=le[i];}for(i=0;i<2;i++){Reset();Write(i?be:le,sizeof(le));CHECK(WvExSource(file));CHECK(calls==1 && !wcscmp(commands,L":print\n"));}}
 {BYTE bad[][6]={{0xff,0xfe,1},{0xff,0xfe,0,0},{0xff,0xfe,0,0xd8},{0xc0,0xaf},{'p',0,'r'}};int length[]={3,4,4,2,3};for(i=0;i<5;i++){Reset();Write(bad[i],length[i]);CHECK(!WvExSource(file));CHECK(!Source && !calls);}}
 Reset();Write("\xef\xbb\xbfprint\n",9);CHECK(WvExSource(file));CHECK(calls==1);
 Reset();Write("filter\nprint\n",13);CHECK(WvExSource(file));CHECK(calls==2 && !running);
 Reset();Write("",0);CHECK(WvExSource(file));CHECK(!calls);
 {char *large=malloc(WV_TEXT_CAP*4+1);memset(large,'x',WV_TEXT_CAP*4+1);Reset();Write(large,WV_TEXT_CAP*4+1);CHECK(!WvExSource(file));CHECK(!calls && !Source);free(large);}
 {WCHAR (*names)[WV_PATH_CAP]=calloc(33,sizeof(*names));WCHAR script[WV_PATH_CAP];char bytes[WV_PATH_CAP*3];int j,n;
  CHECK(names!=NULL);for(j=0;j<33;j++)CHECK(GetTempFileNameW(dir,L"wvn",0,names[j]));
  for(j=32;j>=0;j--){HANDLE h;DWORD written;
   if(j>=31)n=WideCharToMultiByte(CP_UTF8,0,L"print\n",-1,bytes,sizeof(bytes),NULL,NULL)-1;
   else{swprintf(script,WV_PATH_CAP,L"source %s\n",names[j+1]);n=WideCharToMultiByte(CP_UTF8,0,script,-1,bytes,sizeof(bytes),NULL,NULL)-1;}
   h=WvCreateFileW(names[j],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(h!=INVALID_HANDLE_VALUE);CHECK(WriteFile(h,bytes,n,&written,NULL)&&written==n);CloseHandle(h);
  }
  Reset();CHECK(WvExSource(names[0]));CHECK(calls==32 && !Source);
  {HANDLE h;DWORD written;swprintf(script,WV_PATH_CAP,L"source %s\n",names[32]);n=WideCharToMultiByte(CP_UTF8,0,script,-1,bytes,sizeof(bytes),NULL,NULL)-1;
   h=WvCreateFileW(names[31],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(h!=INVALID_HANDLE_VALUE);CHECK(WriteFile(h,bytes,n,&written,NULL)&&written==n);CloseHandle(h);}
  Reset();CHECK(!WvExSource(names[0]));CHECK(calls==32 && !Source);
  for(j=0;j<33;j++)CHECK(DeleteFileW(names[j]));free(names);
 }
 Reset();Write("print\n",6);GetProcessHandleCount(GetCurrentProcess(),&before);
 for(i=1;i<8;i++){Reset();failAt=i;(void)WvExSource(file);CHECK(!Source);}
 GetProcessHandleCount(GetCurrentProcess(),&after);CHECK(after==before);
 Reset();cancel=1;CHECK(!WvExSource(file));CHECK(!Source);
 Reset();CHECK(!WvExSource(L""));WvExGlobalActive=TRUE;CHECK(!WvExSource(file));WvExGlobalActive=FALSE;
 {extern LPSTR Shell;WCHAR cmd[]=L":set shell=\"cmd.exe\"";CHECK(CommandExec(cmd));CHECK(Shell && !strcmp(Shell,"cmd.exe"));}
 Reset();wcscpy(app,L"C:\\Windows\\System32\\cmd.exe");CHECK(WvExShell());CHECK(!inherit && flags==CREATE_NEW_CONSOLE && !wcscmp(launched,L"\"C:\\Windows\\System32\\cmd.exe\" /d"));
 wcscpy(app,L"C:\\shell folder\\fixture.exe");CHECK(WvExShell());CHECK(!wcscmp(launched,L"\"C:\\shell folder\\fixture.exe\""));
 launchFail=1;CHECK(!WvExShell());launchFail=0;resolveFail=1;CHECK(!WvExShell());resolveFail=0;running=1;CHECK(!WvExShell());running=0;WvExGlobalActive=TRUE;CHECK(!WvExShell());
 DeleteFileW(file);printf("PASS %d ex source/shell checks\n",checks);return 0;
}
