#include <windows.h>
#include <stdlib.h>
static int FailEvent,FailThread,EventCount,ThreadCount;
static BOOL ReenterShutdown;
static HANDLE WINAPI TestCreateEventA(LPSECURITY_ATTRIBUTES sa,BOOL manual,BOOL initial,LPCSTR name){
 if(++EventCount==FailEvent){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}return CreateEventA(sa,manual,initial,name);
}
static HANDLE WINAPI TestCreateThread(LPSECURITY_ATTRIBUTES sa,SIZE_T stack,LPTHREAD_START_ROUTINE start,LPVOID data,DWORD flags,LPDWORD id){
 if(++ThreadCount==FailThread){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}return CreateThread(sa,stack,start,data,flags,id);
}
#define CreateEventA TestCreateEventA
#define CreateThread TestCreateThread
#define InsertBuffer TestInsertBuffer
#include "../../Exec.c"
#undef CreateEventA
#undef CreateThread
#undef InsertBuffer
extern BOOL InsertBuffer(LPBYTE,WV_ULONG,WORD);
BOOL TestInsertBuffer(LPBYTE bytes,WV_ULONG size,WORD flags){
 if(ReenterShutdown){ExecShutdown();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 return InsertBuffer(bytes,size,flags);
}
int BugCommandUiShutdown(void){
 WV_COMMAND *s=calloc(1,sizeof(*s));WV_OUTPUT *packet=malloc(sizeof(*packet));PAGE page={0};BYTE bytes[1]={'x'};DWORD before,after;
 if(!s||!packet||!GetProcessHandleCount(GetCurrentProcess(),&before))return 1;
 s->References=1;s->Generation=++CommandGeneration;
 s->Stop=CreateEventA(NULL,TRUE,FALSE,NULL);s->Start=CreateEventA(NULL,TRUE,FALSE,NULL);
 s->Continue=CreateEventA(NULL,FALSE,TRUE,NULL);s->Run=CreateEventA(NULL,TRUE,TRUE,NULL);
 if(!s->Stop||!s->Start||!s->Continue||!s->Run)return 1;
 page.PageBuf=bytes;page.Fill=1;page.PageNo=1;FirstPage=LastPage=CurrPos.p=ScreenStart.p=&page;CurrPos.i=0;ExecInsertPos=CurrPos;
 packet->Size=1;packet->Bytes[0]='x';ActiveCommand=s;ViewOnlyFlag=TRUE;ReenterShutdown=TRUE;
 ExecInput(s->Generation,(LPARAM)packet);ReenterShutdown=FALSE;
 if(ExecRunning()||ViewOnlyFlag||!GetProcessHandleCount(GetCurrentProcess(),&after)||after!=before)return 1;
 puts("PASS shutdown reentered from output insertion: session survives callback, UI recovers, handles return to baseline");return 0;
}
int BugCommandFaults(void){
 DWORD before,after;int test,round;LPSTR previousShell=Shell,previousFormat=ShellCommand;
 Shell="cmd.exe";ShellCommand="%0 /d /c %1";
 if(!GetProcessHandleCount(GetCurrentProcess(),&before))return 1;
 for(round=0;round<3;round++){
 for(test=0;test<6;test++){
  WCHAR command[]=L"exit 0";EventCount=ThreadCount=0;
  FailEvent=test<4?test+1:0;FailThread=test>=4?test-3:0;
  if(ExecPiped(command,3)||ExecRunning()||(FailEvent && EventCount<FailEvent)||(FailThread && ThreadCount<FailThread)){printf("FAIL resource case %d events=%d threads=%d active=%d\n",test,EventCount,ThreadCount,ExecRunning());return 1;}
 }
 FailEvent=FailThread=0;ExecShutdown();
 if(!GetProcessHandleCount(GetCurrentProcess(),&after))return 1;
 printf("Resource round %d: handles before=%lu after=%lu\n",round,before,after);
 if(round && before!=after)return 1;before=after;
 }
 Shell=previousShell;ShellCommand=previousFormat;
 if(!GetProcessHandleCount(GetCurrentProcess(),&after)||after!=before){printf("FAIL handle count before=%lu after=%lu\n",before,after);return 1;}
 puts("PASS four event and two worker-creation failures unwind without active sessions or handle leaks");return 0;
}
