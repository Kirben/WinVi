/* Traditional ex command files and interactive shells. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include <limits.h>
#include "winvi.h"
#include "exec.h"

typedef struct WV_SOURCE {
 struct WV_SOURCE *parent;
 HANDLE file,mapping;
 BYTE *bytes;
 size_t length,at;
 WV_ULONG line;
 BY_HANDLE_FILE_INFORMATION id;
 BOOL utf16,little,cancelled;
} WV_SOURCE;
static WV_SOURCE *Source;
static BOOL ScriptError(LPCWSTR message)
{NewStatus(0,(PWSTR)message,NS_ERROR);return FALSE;}
BOOL WvExSourceActive(void){return Source!=NULL;}
void WvExSourceCancel(void){if(Source)Source->cancelled=TRUE;}
static UINT SourceUnit(WV_SOURCE *s,size_t at)
{
 if(!s->utf16)return s->bytes[at];
 return s->little?s->bytes[at]|s->bytes[at+1]<<8:s->bytes[at]<<8|s->bytes[at+1];
}
/* Mapping is read-only and the open handle denies concurrent writers. Decode
   one physical line at a time; file length is not limited to an INT. */
static INT SourceLineRead(WV_SOURCE *s,PWSTR *text,size_t limit)
{
 size_t start,n,i,step=s->utf16?2:1;INT units;PWSTR line;
 *text=NULL;if(s->at==s->length)return 0;start=s->at;
 while(s->at<s->length && SourceUnit(s,s->at)!=13 && SourceUnit(s,s->at)!=10){
  s->at+=step;if((s->at-start)/step>(s->utf16?limit:(limit>INT_MAX/4?INT_MAX:limit*4)))goto large;
  if(((s->at-start)&65535)==0 && CheckInterrupt()){ScriptError(L"Source interrupted.");return -1;}
 }
 n=s->at-start;
 if(s->at<s->length){UINT c=SourceUnit(s,s->at);s->at+=step;
  if(c==13 && s->at<s->length && SourceUnit(s,s->at)==10)s->at+=step;}
 s->line++;
 if(n/(s->utf16?2:1)>INT_MAX || (s->utf16 && n/2>limit))goto large;
 units=s->utf16?(INT)(n/2):n?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(LPCSTR)s->bytes+start,(INT)n,NULL,0):0;
 if((size_t)units>SIZE_MAX/sizeof(WCHAR)-1)goto large;
 if((n && !units) || (size_t)units>limit){if((size_t)units>limit)goto large;goto invalid;}
 line=malloc(((size_t)units+1)*sizeof(WCHAR));if(!line){ScriptError(L"Not enough memory for a source line.");return -1;}
 *text=line;
 if(s->utf16)for(i=0;i<(size_t)units;i++)line[i]=(WCHAR)SourceUnit(s,start+i*2);
 else if(units && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(LPCSTR)s->bytes+start,(INT)n,line,units)!=units){free(line);goto invalid;}
 line[units]=0;
 for(i=0;i<(size_t)units;i++){
  WCHAR c=line[i];if(!c){free(line);goto invalid;}
  if(c>=0xd800 && c<=0xdbff){if(i+1>=(size_t)units || line[i+1]<0xdc00 || line[i+1]>0xdfff){free(line);goto invalid;}i++;}
  else if(c>=0xdc00 && c<=0xdfff){free(line);goto invalid;}
 }
 *text=line;return 1;
large:ScriptError(L"Source line is too long.");return -1;
invalid:*text=NULL;ScriptError(L"Source files require valid UTF-8 or BOM-marked UTF-16 without NUL characters.");return -1;
}
static INT SourceLine(WV_SOURCE *s,PWSTR *text,size_t limit)
{
#if defined(_MSC_VER)
 __try{return SourceLineRead(s,text,limit);}
 __except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){
  free(*text);*text=NULL;ScriptError(L"Cannot read the mapped source file.");return -1;
 }
#else
 return SourceLineRead(s,text,limit);
#endif
}
/* append/insert/change consume following physical lines, not commands. EOF
   behaves like end-of-input; only an exact dot line is the terminator. */
BOOL WvExSourceText(PWSTR *text)
{
 PWSTR result=NULL,line=NULL;size_t used=0,capacity=0,n,next;INT read;
 *text=NULL;if(!Source)return FALSE;
 for(;;){
  read=SourceLine(Source,&line,INT_MAX-1);if(read<0)goto fail;if(!read)break;
  if(!wcscmp(line,L".")){free(line);line=NULL;break;}
  n=wcslen(line);
  if(used>SIZE_MAX/sizeof(WCHAR)-3 || n>SIZE_MAX/sizeof(WCHAR)-used-3){ScriptError(L"Source input is too large.");goto fail;}
  next=used+n+2;
  if(next+1>capacity){PWSTR grown;size_t cap=capacity?capacity:256;
   while(cap<next+1){if(cap>SIZE_MAX/sizeof(WCHAR)/2){cap=next+1;break;}cap*=2;}
   grown=realloc(result,cap*sizeof(WCHAR));if(!grown){ScriptError(L"Not enough memory for source input.");goto fail;}
   result=grown;capacity=cap;
  }
  memcpy(result+used,line,n*sizeof(WCHAR));result[next-2]=13;result[next-1]=10;result[next]=0;used=next;
  free(line);line=NULL;
 }
 if(!result){result=calloc(1,sizeof(WCHAR));if(!result)return ScriptError(L"Not enough memory for source input.");}
 *text=result;return TRUE;
fail:free(line);free(result);return FALSE;
}
BOOL WvExSource(PWSTR args)
{
 WV_SOURCE frame={0},*p;PWSTR name=NULL,line=NULL,command=NULL;LARGE_INTEGER size;
 size_t n;INT depth=0,read;BOOL ok=FALSE;WV_ULONG commandLine=0;WCHAR message[160];BYTE header[3]={0};DWORD headerSize=0;
 if(WvExGlobalActive || ExecRunning())return ScriptError(L"Source requires no active global/input or external command.");
 while(*args==L' ' || *args==L'\t')args++;n=wcslen(args);
 while(n && (args[n-1]==L' ' || args[n-1]==L'\t'))n--;
 if(n>=2 && args[0]==L'"' && args[n-1]==L'"'){args++;n-=2;}
 if(!n)return ScriptError(L"Source requires a command filename.");
 for(p=Source;p;p=p->parent)if(++depth>=32)return ScriptError(L"Source nesting exceeds 32 files.");
 name=malloc((n+1)*sizeof(WCHAR));if(!name)return ScriptError(L"Not enough memory for source filename.");
 memcpy(name,args,n*sizeof(WCHAR));name[n]=0;
 frame.file=WvCreateFileW(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);free(name);
 if(frame.file==INVALID_HANDLE_VALUE)return ScriptError(L"Cannot open source command file.");
 if(!GetFileInformationByHandle(frame.file,&frame.id) || !GetFileSizeEx(frame.file,&size) || size.QuadPart<0 || (ULONGLONG)size.QuadPart>SIZE_MAX)goto fileError;
 for(p=Source;p;p=p->parent)if(p->id.dwVolumeSerialNumber==frame.id.dwVolumeSerialNumber && p->id.nFileIndexHigh==frame.id.nFileIndexHigh && p->id.nFileIndexLow==frame.id.nFileIndexLow){ScriptError(L"Recursive source file cycle rejected.");goto done;}
 frame.length=(size_t)size.QuadPart;
 if(frame.length){
  if(!ReadFile(frame.file,header,frame.length<3?(DWORD)frame.length:3,&headerSize,NULL) || headerSize!=(frame.length<3?(DWORD)frame.length:3))goto fileError;
  frame.mapping=CreateFileMappingW(frame.file,NULL,PAGE_READONLY,0,0,NULL);if(!frame.mapping)goto fileError;
  frame.bytes=MapViewOfFile(frame.mapping,FILE_MAP_READ,0,0,0);if(!frame.bytes)goto fileError;
  if(frame.length>=2 && ((header[0]==0xff && header[1]==0xfe)||(header[0]==0xfe && header[1]==0xff))){frame.utf16=TRUE;frame.little=header[0]==0xff;frame.at=2;if(frame.length&1){ScriptError(L"Truncated UTF-16 source file.");goto done;}}
  else if(frame.length>=3 && header[0]==0xef && header[1]==0xbb && header[2]==0xbf)frame.at=3;
 }
 frame.parent=Source;Source=&frame;StartInterruptCheck();
 while((read=SourceLine(&frame,&line,WV_TEXT_CAP-2))>0){
  PWSTR body=line;commandLine=frame.line;
  while(*body==L' ' || *body==L'\t' || *body==L':')body++;
  if(*body && *body!=L'"'){
   n=wcslen(body);command=malloc((n+2)*sizeof(WCHAR));if(!command){ScriptError(L"Not enough memory for source command.");goto stopped;}
   command[0]=L':';memcpy(command+1,body,(n+1)*sizeof(WCHAR));
   if(!CommandExec(command))goto stopped;
   /* Filtered commands finish through queued output packets. Keep pumping
      those packets before the next command uses their resulting document. */
   while(ExecRunning() && hwndMain){
    MessageLoop(FALSE);
    if(Interrupted || frame.cancelled){ExecInterrupt(0);ScriptError(L"Source interrupted; successful edits remain.");goto pop;}
    if(ExecRunning())Sleep(10);
   }
   if(frame.cancelled){ScriptError(L"Source interrupted; successful edits remain.");goto pop;}
   free(command);command=NULL;
   if(!hwndMain){free(line);line=NULL;ok=TRUE;goto pop;}
  }
  free(line);line=NULL;
  if(CheckInterrupt() || (Disabled && CheckInterrupt())){ScriptError(L"Source interrupted; successful edits remain.");goto pop;}
  if(Disabled){Enable();StartInterruptCheck();}
 }
 ok=read==0;goto pop;
stopped:
 _snwprintf(message,160,L"Source stopped at command line %I64u; earlier successful edits remain.",commandLine);ScriptError(message);
pop:Source=frame.parent;if(Disabled)Enable();goto done;
fileError:ScriptError(L"Cannot map or inspect source command file.");
done:free(command);free(line);if(frame.bytes)UnmapViewOfFile(frame.bytes);if(frame.mapping)CloseHandle(frame.mapping);CloseHandle(frame.file);return ok;
}
BOOL WvExShell(void)
{
 extern LPSTR Shell;WCHAR application[WV_PATH_CAP],*line,*base;size_t n;
 STARTUPINFOW startup={0};PROCESS_INFORMATION process={0};BOOL ok;
 if(WvExGlobalActive || ExecRunning())return ScriptError(L"Shell requires no active global/input or external command.");
 if(!WvResolveShellExecutable(Shell,application,WV_PATH_CAP))return ScriptError(L"Cannot resolve the configured shell.");
 n=wcslen(application);line=malloc((n+8)*sizeof(WCHAR));if(!line)return ScriptError(L"Not enough memory to launch shell.");
 line[0]=L'"';memcpy(line+1,application,n*sizeof(WCHAR));line[n+1]=L'"';line[n+2]=0;
 base=wcsrchr(application,L'\\');base=base?base+1:application;
 if(!_wcsicmp(base,L"cmd.exe"))wcscat(line,L" /d");
 startup.cb=sizeof(startup);
 ok=WvCreateProcessW(application,line,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,NULL,&startup,&process);
 free(line);if(!ok)return ScriptError(L"Cannot start the configured interactive shell.");
 CloseHandle(process.hThread);CloseHandle(process.hProcess);return TRUE;
}
