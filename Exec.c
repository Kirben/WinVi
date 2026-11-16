/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2012 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *     29-Dec-2000	check for working dir on network path (Win32 on Win9x/ME)
 *     20-Jul-2002	new function ExecRunning 
 *     22-Jul-2002	use of private myassert.h
 *     24-Jul-2002	disable "new" and "open" toolbar buttons / menu items
 *     15-Aug-2002	check of ViewOnly flag before executing
 *     16-Aug-2002	prevent setting safe after writing when inserting
 *     29-Oct-2002	better handling of Ctrl+S/Ctrl+Q (xoff/xon)
 *     30-Oct-2002	corrected inherit attrs for shell pipes, now EOF works
 *      1-Nov-2002	Ctrl+C/Ctrl+Break handling for Windows XP
 *     10-Jan-2003	configurable shell for use of bash
 *     11-Jan-2003	problem with shell pipes fixed for win95/98/me
 *     11-Jan-2003	shell: hide for winnt family, minimize for win95 family
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     31-Aug-2010	DLL load from system directory only
 */

#include <windows.h>
#include <shellapi.h>
#include <malloc.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"
#include "exec.h"


extern LPSTR Shell,ShellCommand;
extern INT OemCodePage;
extern WCHAR BufferW[WV_TEXT_CAP];
/* Owned command sessions; workers never access editor document/UI state. */
extern PWSTR LastCmd;
extern CHAR QueryString[150];
extern DWORD QueryTime;
typedef struct {DWORD Size;BYTE Bytes[2048];} WV_OUTPUT;
typedef struct {
 volatile LONG References,Error;
 HANDLE Stop,Start,Continue,Run,Job,InputThread,OutputThread;
 HANDLE InputPipe,OutputPipe;
 PROCESS_INFORMATION Process;
 BYTE *Input;WV_ULONG InputSize;
 HWND Window;ULONG_PTR Generation;WORD Flags;
 UINT OutputCodePage;BYTE OutputLead;BOOL HasOutputLead;
} WV_COMMAND;
static WV_COMMAND *ActiveCommand;
static ULONG_PTR CommandGeneration;
BOOL XOffState=FALSE;
POSITION ExecInsertPos;

static void CommandError(WV_COMMAND *s,DWORD error){InterlockedCompareExchange(&s->Error,(LONG)error,0);}
static void ReleaseCommand(WV_COMMAND *s)
{
 if(InterlockedDecrement(&s->References))return;
 if(s->InputThread)CloseHandle(s->InputThread);if(s->OutputThread)CloseHandle(s->OutputThread);
 if(s->InputPipe)CloseHandle(s->InputPipe);if(s->OutputPipe)CloseHandle(s->OutputPipe);
 if(s->Process.hThread)CloseHandle(s->Process.hThread);if(s->Process.hProcess)CloseHandle(s->Process.hProcess);
 if(s->Job)CloseHandle(s->Job);
 if(s->Stop)CloseHandle(s->Stop);if(s->Start)CloseHandle(s->Start);
 if(s->Continue)CloseHandle(s->Continue);if(s->Run)CloseHandle(s->Run);
 free(s->Input);free(s);
}
static BOOL CommandWait(WV_COMMAND *s,HANDLE ready)
{
 HANDLE events[2]={s->Stop,ready};return WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0+1;
}
static DWORD WINAPI CommandInput(LPVOID parameter)
{
 WV_COMMAND *s=parameter;WV_ULONG offset=0;
 if(CommandWait(s,s->Start))while(offset<s->InputSize){
  DWORD amount=(DWORD)(s->InputSize-offset>1048576?1048576:s->InputSize-offset),written=0;
  if(WaitForSingleObject(s->Stop,0)==WAIT_OBJECT_0)break;
  if(!WriteFile(s->InputPipe,s->Input+offset,amount,&written,NULL)||!written)break;
  offset+=written;
 }
 CloseHandle(s->InputPipe);s->InputPipe=NULL;ReleaseCommand(s);return 0;
}
static DWORD WINAPI CommandOutput(LPVOID parameter)
{
 WV_COMMAND *s=parameter;BOOL broken=FALSE;
 if(!CommandWait(s,s->Start))goto done;
 for(;;){
  DWORD available=0,read=0;WV_OUTPUT *packet;BYTE discard[2048];
  if(WaitForSingleObject(s->Stop,0)==WAIT_OBJECT_0)break;
  if(s->Flags&2){if(!CommandWait(s,s->Continue)||!CommandWait(s,s->Run))break;}
  for(;;){
   HANDLE events[2]={s->Stop,s->Process.hProcess};
   if(WaitForSingleObject(s->Stop,0)==WAIT_OBJECT_0)goto done;
   if(!broken && !PeekNamedPipe(s->OutputPipe,NULL,0,NULL,&available,NULL)){
    DWORD error=GetLastError();broken=TRUE;
    if(error!=ERROR_BROKEN_PIPE && error!=ERROR_NO_DATA && error!=ERROR_OPERATION_ABORTED)CommandError(s,error);
   }
   if(available)break;
   {DWORD result=WaitForMultipleObjects(2,events,FALSE,25);
    if(result==WAIT_OBJECT_0)goto done;
    if(result==WAIT_OBJECT_0+1){if(!broken && PeekNamedPipe(s->OutputPipe,NULL,0,NULL,&available,NULL) && available)break;goto done;}
    if(result==WAIT_FAILED){CommandError(s,GetLastError());goto done;}
   }
  }
  if(available>sizeof(discard))available=sizeof(discard);
  packet=(s->Flags&2)?malloc(sizeof(*packet)):NULL;
  if((s->Flags&2)&&!packet){CommandError(s,ERROR_NOT_ENOUGH_MEMORY);goto done;}
  if(!ReadFile(s->OutputPipe,packet?packet->Bytes:discard,available,&read,NULL)||!read){
   free(packet);broken=TRUE;
   if(s->Flags&2)SetEvent(s->Continue);continue;
  }
  if(packet){packet->Size=read;if(!PostMessage(s->Window,WM_USER+1235,s->Generation,(LPARAM)packet)){free(packet);goto done;}}
 }
done:
 CloseHandle(s->OutputPipe);s->OutputPipe=NULL;
 PostMessage(s->Window,WM_USER+1235,s->Generation,0);ReleaseCommand(s);return 0;
}
static void StopCommand(WV_COMMAND *s)
{
 HANDLE threads[2]={s->InputThread,s->OutputThread};DWORD i,start=GetTickCount();
 SetEvent(s->Stop);SetEvent(s->Start);SetEvent(s->Continue);SetEvent(s->Run);
 if(s->Job)TerminateJobObject(s->Job,ERROR_CANCELLED);
 for(;;){
  BOOL done=TRUE;
  for(i=0;i<2;i++)if(threads[i]&&WaitForSingleObject(threads[i],0)==WAIT_TIMEOUT){done=FALSE;CancelSynchronousIo(threads[i]);}
  if(done || GetTickCount()-start>=1000)break;Sleep(10);
 }
 /* Delayed I/O completion retains its own reference, never a dangling context. */
 ReleaseCommand(s);
}
VOID ReleaseXOff(VOID)
{
 if(ActiveCommand){if(XOffState)ResetEvent(ActiveCommand->Run);else SetEvent(ActiveCommand->Run);}
}
BOOL ExecRunning(VOID){return ActiveCommand!=NULL;}
VOID ExecInterrupt(INT event)
{
 (void)event;WvExSourceCancel();if(ActiveCommand){SetEvent(ActiveCommand->Stop);SetEvent(ActiveCommand->Continue);SetEvent(ActiveCommand->Run);TerminateJobObject(ActiveCommand->Job,ERROR_CANCELLED);}
 XOffState=FALSE;
}
VOID ExecShutdown(VOID)
{
 WV_COMMAND *s=ActiveCommand;MSG message;ActiveCommand=NULL;
 if(s)StopCommand(s);
 if(hwndMain)while(PeekMessage(&message,hwndMain,WM_USER+1235,WM_USER+1235,PM_REMOVE))free((void*)message.lParam);
}
/* Packet boundaries are pipe boundaries, not character boundaries. Retain a
   final DBCS lead byte in its owning session until the next packet or EOF. */
static DWORD CommandTextBytes(WV_COMMAND *s,WV_OUTPUT *packet,BYTE bytes[2049],BOOL final)
{
 DWORD size=packet->Size,offset=0;UINT cp=s->OutputCodePage;
 if(s->HasOutputLead){bytes[offset++]=s->OutputLead;s->HasOutputLead=FALSE;}
 memcpy(bytes+offset,packet->Bytes,size);size+=offset;
 if(UtfEncoding && !final){
  for(offset=0;offset<size;){
   if(IsDBCSLeadByteEx(cp,bytes[offset])){
    WCHAR value;if(offset+1==size){s->OutputLead=bytes[offset];s->HasOutputLead=TRUE;return size-1;}
    if(MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,(LPCSTR)bytes+offset,2,&value,1)==1){offset+=2;continue;}
   }offset++;
  }
 }
 return size;
}
static BOOL InsertCommandOutput(WV_COMMAND *s,WV_OUTPUT *packet,BOOL final)
{
 BYTE joined[2049],*buffer=joined,*converted=NULL;DWORD size=CommandTextBytes(s,packet,joined,final);
 if(!size)return TRUE;
 POSITION old;INT relation=ComparePos(&CurrPos,&ExecInsertPos);WV_ULONG advance=CurrPos.i;
 BOOL lastCr=size&&buffer[size-1]=='\r',ok=FALSE;extern BOOL SetSafeAfterWrite;extern INT OemCodePage;
 if(relation){old=CurrPos;if(relation>0){while(old.p->PageNo>ExecInsertPos.p->PageNo){old.p=old.p->Prev;advance+=old.p->Fill;}advance-=ExecInsertPos.i;}CurrPos=ExecInsertPos;}
 SetSafeAfterWrite=FALSE;ViewOnlyFlag=FALSE;
 if(UtfEncoding){
  WCHAR *wide;INT count,bytes,i;wide=malloc(size*sizeof(WCHAR));
  if(!wide){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto finish;}
  count=MultiByteToWideChar(s->OutputCodePage,MB_ERR_INVALID_CHARS,(LPCSTR)buffer,size,wide,size);
  if(!count){
   /* Preserve valid neighbors and render malformed bytes consistently. */
   DWORD at=0;while(at<size){
    INT span=IsDBCSLeadByteEx(s->OutputCodePage,buffer[at]) && at+1<size?2:1;
    if(MultiByteToWideChar(s->OutputCodePage,MB_ERR_INVALID_CHARS,(LPCSTR)buffer+at,span,wide+count,1)==1)at+=span;
    else{wide[count]=0xfffd;at++;}count++;
   }
  }
  if(UtfEncoding==8){
   bytes=WideCharToMultiByte(CP_UTF8,0,wide,count,NULL,0,NULL,NULL);
   converted=bytes?malloc(bytes):NULL;
   if(!converted){free(wide);SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto finish;}
   if(!WideCharToMultiByte(CP_UTF8,0,wide,count,(LPSTR)converted,bytes,NULL,NULL)){free(wide);goto finish;}
   free(wide);size=bytes;
  } else {if(!UtfLsbFirst)for(i=0;i<count;i++)wide[i]=(wide[i]<<8)|(wide[i]>>8);converted=(BYTE*)wide;size=count*sizeof(WCHAR);}
  buffer=converted;
 }
 if(!InsertBuffer(buffer,size,0)){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto finish;}
 ExecInsertPos=CurrPos;
 if(lastCr){GoBackAndChar(&ExecInsertPos);Advance(&ExecInsertPos,UtfEncoding==16?2:1);}
 if(relation>0){old=ExecInsertPos;Advance(&old,advance);}
 NewPosition(relation?&old:&ExecInsertPos);ShowEditCaret();ok=TRUE;
finish:
 free(converted);ViewOnlyFlag=ActiveCommand!=NULL;return ok;
}
VOID ExecInput(WPARAM generation,LPARAM data)
{
 WV_COMMAND *s=ActiveCommand;WV_OUTPUT *packet=(WV_OUTPUT*)data;
 if(!s || generation!=s->Generation){free(packet);return;}
 if(packet){
  InterlockedIncrement(&s->References); /* UI callbacks can reenter shutdown. */
  if(WaitForSingleObject(s->Stop,0)!=WAIT_OBJECT_0 && !InsertCommandOutput(s,packet,FALSE)){
   CommandError(s,GetLastError());if(ActiveCommand==s)ExecInterrupt(0);
  }
  free(packet);if(ActiveCommand==s)ReleaseXOff();SetEvent(s->Continue);ReleaseCommand(s);return;
 }
 {
  WV_OUTPUT end={0};
  InterlockedIncrement(&s->References);
  if(s->HasOutputLead && WaitForSingleObject(s->Stop,0)!=WAIT_OBJECT_0 && !InsertCommandOutput(s,&end,TRUE))CommandError(s,GetLastError());
  if(ActiveCommand!=s){ReleaseCommand(s);return;}ReleaseCommand(s);
  DWORD error=(DWORD)s->Error;ActiveCommand=NULL;StopCommand(s);
  ViewOnlyFlag=FALSE;Interruptable=FALSE;XOffState=FALSE;GotChar('\033',0);
  FindValidPosition(&CurrPos,0);ShowEditCaret();EnableToolButton(IDB_NEW,TRUE);EnableToolButton(IDB_OPEN,TRUE);
  DragAcceptFiles(hwndMain,TRUE);CheckClipboard();
  if(error){_snwprintf(BufferW,WSIZE(BufferW),L"Command I/O stopped (Windows error %lu).",error);NewStatus(0,BufferW,NS_ERROR);}
 }
}
PSTR ExpandSys32(PSTR Dllname)
{	static CHAR Path[MAX_PATH];
	INT			len;

	len = GetSystemDirectory(Path, sizeof(Path));
	if (len > 0 && len + strlen(Dllname) + 2 <= sizeof(Path)) {
		snprintf(Path + len, sizeof(Path) - len - 1, "%s%s",
				 Path[len-1] == '\\' ? "" : "\\", Dllname);
		return Path;
	}
	return "";	/*better prevent DLL loading than doing it unsafe*/
}

BOOL BuildShellCommandW(LPCSTR shell,LPCSTR format,PCWSTR command,PWSTR *line,PWSTR application,DWORD cap)
{
 const char *rest=format;int count;size_t prefix,total,len;BOOL cmd;PCWSTR base;PWSTR quoted=NULL;PCWSTR argument=command;
 *line=NULL;if(!WvResolveShellExecutable(shell,application,cap))return FALSE;
 while(*rest==' '||*rest=='\t')rest++;
 if(!strncmp(rest,"\"%0\"",4))rest+=4;
 else if(!strncmp(rest,"%0",2))rest+=2;
 else {SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if(*rest&&*rest!=' '&&*rest!='\t'){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 base=wcsrchr(application,L'\\');base=base?base+1:application;cmd=!_wcsicmp(base,L"cmd.exe");
 if(cmd){
  size_t n=wcslen(command);quoted=malloc((n+3)*sizeof(WCHAR));if(!quoted){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
  quoted[0]='"';memcpy(quoted+1,command,n*sizeof(WCHAR));quoted[n+1]='"';quoted[n+2]=0;argument=quoted;
 }
 len=wcslen(application);prefix=len+2+(cmd?6:0);
 count=FormatShellW(NULL,(LPSTR)rest,(LPSTR)shell,(PWSTR)argument);
 if(count<0||prefix>=32767||(size_t)count>=32767-prefix){free(quoted);SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
 total=prefix+count+1;*line=malloc(total*sizeof(WCHAR));
 if(!*line){free(quoted);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 (*line)[0]='"';memcpy(*line+1,application,len*sizeof(WCHAR));(*line)[len+1]='"';
 if(cmd)memcpy(*line+len+2,L" /d /s",6*sizeof(WCHAR));
 FormatShellW(*line+prefix,(LPSTR)rest,(LPSTR)shell,(PWSTR)argument);free(quoted);return TRUE;
}

BOOL ExecPiped(PWSTR command,WORD flags)
{
 WV_COMMAND *s=NULL;WCHAR application[WV_PATH_CAP],*line=NULL,*history=NULL;
 HANDLE childInput=NULL,childOutput=NULL;STARTUPINFOEXW startup={0};SIZE_T attrSize=0;DWORD error;
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};extern HGLOBAL YankHandles[36];extern WV_ULONG YankLength[36];extern INT YankIndex;
 if(ViewOnlyFlag || ActiveCommand){Error(236);return FALSE;}
 if(*command=='!'){
  if(!LastCmd){Error(235);return FALSE;}
  history=malloc((wcslen(LastCmd)+wcslen(command))*sizeof(WCHAR));
  if(history){WvCopyString(history,LastCmd);wcscat(history,command+1);}
 } else {
  history=malloc((wcslen(command)+2)*sizeof(WCHAR));
  if(history){history[0]='!';WvCopyString(history+1,command);}
 }
 if(!history){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
 if(!BuildShellCommandW(Shell,ShellCommand,history+1,&line,application,WSIZE(application)))goto fail;
 s=calloc(1,sizeof(*s));if(!s){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
 s->OutputCodePage=OemCodePage?OemCodePage:GetOEMCP();
 s->References=1;s->Window=hwndMain;s->Generation=++CommandGeneration;s->Flags=flags;
 s->Stop=CreateEvent(NULL,TRUE,FALSE,NULL);s->Start=CreateEvent(NULL,TRUE,FALSE,NULL);
 s->Continue=CreateEvent(NULL,FALSE,TRUE,NULL);s->Run=CreateEvent(NULL,TRUE,TRUE,NULL);
 if(!s->Stop||!s->Start||!s->Continue||!s->Run)goto fail;
 if((flags&1)&&SelectCount){
  BYTE *input;
  if(!Yank(&SelectStart,SelectCount,0)){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
  s->InputSize=YankLength[YankIndex];s->Input=malloc((size_t)s->InputSize);
  if(!s->Input){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
  input=GlobalLock(YankHandles[YankIndex]);if(!input){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
  memcpy(s->Input,input,(size_t)s->InputSize);GlobalUnlock(YankHandles[YankIndex]);
 }
 if(!CreatePipe(&childInput,&s->InputPipe,NULL,0)||!CreatePipe(&s->OutputPipe,&childOutput,NULL,0))goto fail;
 if(!SetHandleInformation(childInput,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT)||!SetHandleInformation(childOutput,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT))goto fail;
 startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
 startup.StartupInfo.wShowWindow=SW_HIDE;startup.StartupInfo.hStdInput=childInput;startup.StartupInfo.hStdOutput=startup.StartupInfo.hStdError=childOutput;
 InitializeProcThreadAttributeList(NULL,1,0,&attrSize);
 startup.lpAttributeList=malloc(attrSize);if(!startup.lpAttributeList){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
 if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&attrSize)){free(startup.lpAttributeList);startup.lpAttributeList=NULL;goto fail;}
 {HANDLE inherit[2]={childInput,childOutput};
  if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,sizeof(inherit),NULL,NULL))goto fail;
 }
 s->Job=CreateJobObject(NULL,NULL);limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!s->Job||!SetInformationJobObject(s->Job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))goto fail;
 if(!WvCreateJobProcessW(application,line,&startup.StartupInfo,s->Job,&s->Process))goto fail;
 CloseHandle(childInput);childInput=NULL;CloseHandle(childOutput);childOutput=NULL;
 InterlockedIncrement(&s->References);s->InputThread=CreateThread(NULL,0,CommandInput,s,0,NULL);
 if(!s->InputThread){InterlockedDecrement(&s->References);goto fail;}
 InterlockedIncrement(&s->References);s->OutputThread=CreateThread(NULL,0,CommandOutput,s,0,NULL);
 if(!s->OutputThread){InterlockedDecrement(&s->References);goto fail;}
 if(ResumeThread(s->Process.hThread)==(DWORD)-1)goto fail;
 if((flags&1)&&SelectCount)DeleteSelected(27);
 free(LastCmd);LastCmd=history;history=NULL;NewStatus(0,LastCmd,NS_NORMAL);
 ActiveCommand=s;ExecInsertPos=CurrPos;XOffState=FALSE;ViewOnlyFlag=TRUE;Interruptable=TRUE;
 Mode=InsertMode;NewPosition(&CurrPos);SwitchCursor(SWC_TEXTCURSOR);ShowEditCaret();
 DragAcceptFiles(hwndMain,FALSE);EnableToolButton(IDB_NEW,FALSE);EnableToolButton(IDB_OPEN,FALSE);CheckClipboard();
 SetEvent(s->Start);
 DeleteProcThreadAttributeList(startup.lpAttributeList);free(startup.lpAttributeList);free(line);return TRUE;
fail:
 error=GetLastError();
 if(childInput)CloseHandle(childInput);if(childOutput)CloseHandle(childOutput);
 if(startup.lpAttributeList){DeleteProcThreadAttributeList(startup.lpAttributeList);free(startup.lpAttributeList);}
 if(s){if(s->Process.hProcess)TerminateProcess(s->Process.hProcess,ERROR_CANCELLED);StopCommand(s);}
 free(history);free(line);
 _snwprintf(BufferW,WSIZE(BufferW),L"Cannot start command safely (Windows error %lu).",error);NewStatus(0,BufferW,NS_ERROR);SetLastError(error);return FALSE;
}
