/* Command-bar input and document encoding for ex. GPL-2.0-or-later. */
#include <windows.h>
#include <commctrl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"

static BOOL InputError(LPCWSTR text)
{NewStatus(0,(PWSTR)text,NS_ERROR);return FALSE;}
typedef struct {PWSTR text;size_t used,capacity;BOOL done,busy;WV_ULONG lines;} EX_INPUT;
static EX_INPUT *ActiveInput;
static void InputMessage(MSG *message)
{
 if((message->hwnd==hwndMain || message->hwnd==hwndCmd) &&
    ((message->message==WM_CHAR && (message->wParam==27 || message->wParam==3)) ||
     (message->message==WM_KEYDOWN && message->wParam==VK_CANCEL))){WvExInputDone(TRUE);return;}
 TranslateMessage(message);DispatchMessageW(message);
}
static UINT InputScalar(LPCWSTR,UINT,BYTE[8]);
static UINT InputUnits(LPCWSTR text)
{
 if(*text>=0xd800 && *text<=0xdbff)return text[1]>=0xdc00 && text[1]<=0xdfff?2:0;
 return *text>=0xdc00 && *text<=0xdfff?0:1;
}
/* Validate before accepting a line: never silently replace unencodable text.
   Pump bounded batches so interruption remains available on long pasted lines. */
static BOOL InputRepresentable(EX_INPUT *input,LPCWSTR text)
{
 BYTE encoded[8];UINT units,work=0;
 while(*text){
  if(*text==L'\r' || *text==L'\n')return FALSE;
  units=InputUnits(text);if(!units || !InputScalar(text,units,encoded))return FALSE;text+=units;
  if((++work&255)==0){MSG message;UINT pending=0;
   while(pending++<32 && PeekMessageW(&message,NULL,0,0,PM_REMOVE)){
    if(message.message==WM_QUIT){PostQuitMessage((INT)message.wParam);input->done=TRUE;break;}
    InputMessage(&message);
   }
   if(Interrupted){Interrupted=FALSE;input->done=TRUE;}
   if(input->done)return FALSE;
  }
 }
 return TRUE;
}
/* Called by both the edit subclass and the normal command-bar completion
   message. Returning TRUE prevents recursive execution/history recording. */
BOOL WvExInputDone(BOOL interrupt)
{
 EX_INPUT *input=ActiveInput;INT n,read;PWSTR line,grown;size_t next,capacity;
 if(!input)return FALSE;
 if(interrupt){input->done=TRUE;return TRUE;}
 if(input->done || input->busy)return TRUE;
 SetLastError(ERROR_SUCCESS);n=GetWindowTextLengthW(hwndCmd);
 if((!n && GetLastError()!=ERROR_SUCCESS) || n<0 || n>=INT_MAX || (size_t)n>SIZE_MAX/sizeof(WCHAR)-1)return TRUE;
 line=malloc(((size_t)n+1)*sizeof(WCHAR));if(!line){SetWindowTextW(hwndMain,L"WinVi - not enough memory; pending line not accepted");return TRUE;}
 SetLastError(ERROR_SUCCESS);read=GetWindowTextW(hwndCmd,line,n+1);if(read!=n || (!read && GetLastError()!=ERROR_SUCCESS)){free(line);return TRUE;}
 if(n==1 && line[0]==L'.'){input->done=TRUE;free(line);return TRUE;}
 input->busy=TRUE;EnableWindow(hwndCmd,FALSE);
 if(!InputRepresentable(input,line)){
  if(!input->done)SetWindowTextW(hwndMain,L"WinVi - line cannot be encoded exactly; correct it or interrupt");
 }else if(input->used>SIZE_MAX/sizeof(WCHAR)-3 || (size_t)n>SIZE_MAX/sizeof(WCHAR)-input->used-3){
  SetWindowTextW(hwndMain,L"WinVi - input too large; pending line not accepted");
 }else{
  next=input->used+(size_t)n+2;
  capacity=input->capacity;
  if(capacity<next+1){
   capacity=capacity>SIZE_MAX/sizeof(WCHAR)/2?SIZE_MAX/sizeof(WCHAR):capacity*2;
   if(capacity<next+1)capacity=next+1;
   grown=realloc(input->text,capacity*sizeof(WCHAR));
  }else grown=input->text;
  if(!grown)SetWindowTextW(hwndMain,L"WinVi - not enough memory; pending line not accepted");
  else{WCHAR title[160];input->text=grown;input->capacity=capacity;memcpy(grown+input->used,line,(size_t)n*sizeof(WCHAR));
   grown[next-2]=L'\r';grown[next-1]=L'\n';grown[next]=0;input->used=next;input->lines++;
   SetWindowTextW(hwndCmd,L"");
   _snwprintf(title,160,L"WinVi - ex input: %I64u line(s) accepted; . then Enter finishes; Esc/Ctrl+C interrupts",input->lines);
   SetWindowTextW(hwndMain,title);
  }
 }
 input->busy=FALSE;EnableWindow(hwndCmd,TRUE);SetFocus(hwndCmd);free(line);return TRUE;
}
/* Multiline paste follows the same Enter/dot rules. The final unterminated
   clipboard line stays editable. Ignore everything following a dot line. */
static void InputPaste(HWND edit)
{
 HANDLE data;LPCWSTR clip;PWSTR copy,p,start;SIZE_T bytes,n;
 if(!ActiveInput || ActiveInput->busy || ActiveInput->done || !OpenClipboard(edit))return;
 data=GetClipboardData(CF_UNICODETEXT);bytes=data?GlobalSize(data):0;clip=data?GlobalLock(data):NULL;
 copy=NULL;
 if(clip){
  for(n=0;n<bytes/sizeof(WCHAR) && clip[n];n++);
  if(n<bytes/sizeof(WCHAR) && n<=SIZE_MAX/sizeof(WCHAR)-1){copy=malloc((n+1)*sizeof(WCHAR));if(copy)memcpy(copy,clip,(n+1)*sizeof(WCHAR));}
  GlobalUnlock(data);
 }
 CloseClipboard();if(!copy)return;
 for(start=p=copy;!ActiveInput->done;p++){
  WCHAR c=*p;
  if(!c || c==L'\r' || c==L'\n'){
   *p=0;SendMessageW(edit,EM_REPLACESEL,TRUE,(LPARAM)start);
   if(!c)break;
   WvExInputDone(FALSE);
   /* Invalid/unallocated lines remain editable; do not merge later lines. */
   if(GetWindowTextLengthW(edit) && !ActiveInput->done)break;
   if(c==L'\r' && p[1]==L'\n')p++;
   start=p+1;
  }
 }
 free(copy);
}
static LRESULT CALLBACK InputEdit(HWND edit,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data)
{
 (void)id;(void)data;
 if(message==WM_CHAR){
  if(w==L'\t'){SendMessageW(edit,EM_REPLACESEL,TRUE,(LPARAM)L"\t");return 0;}
  if(w==L'\r'){WvExInputDone(FALSE);return 0;}
  if(w==27 || w==3){WvExInputDone(TRUE);return 0;}
 }
 if(message==WM_KEYDOWN && w==VK_CANCEL){WvExInputDone(TRUE);return 0;}
 if(message==WM_PASTE){InputPaste(edit);return 0;}
 return DefSubclassProc(edit,message,w,l);
}
BOOL WvExAskText(INT kind,PWSTR *text)
{
 EX_INPUT input={0};MSG message;BOOL quit=FALSE;INT quitCode=0;HWND edit;
 *text=NULL;if(WvExSourceActive())return WvExSourceText(text);
 if(ActiveInput)return FALSE;
 CommandEdit(0);edit=hwndCmd;
 if(!edit)return InputError(L"Cannot open the ex input bar.");
 if(!SetWindowSubclass(edit,InputEdit,1,0)){CommandDone(TRUE);return InputError(L"Cannot initialize the ex input bar.");}
 input.text=malloc(sizeof(WCHAR));if(!input.text){RemoveWindowSubclass(edit,InputEdit,1);CommandDone(TRUE);return InputError(L"Not enough memory for ex input.");}
 input.text[0]=0;input.capacity=1;ActiveInput=&input;Disable(3);DragAcceptFiles(hwndMain,FALSE);
 SendMessageW(edit,EM_LIMITTEXT,INT_MAX-1,0);
 SendMessageW(edit,EM_SETCUEBANNER,FALSE,(LPARAM)L"Enter text; . alone finishes; Esc/Ctrl+C keeps accepted lines");
 SetWindowTextW(hwndMain,kind==0?L"WinVi - ex append input; . then Enter finishes":kind==1?L"WinVi - ex insert input; . then Enter finishes":L"WinVi - ex change input; . then Enter finishes");
 while(!input.done){
  INT result=GetMessageW(&message,NULL,0,0);
  if(result<=0){quit=result==0;quitCode=quit?(INT)message.wParam:0;break;}
  InputMessage(&message);
  if(Interrupted){Interrupted=FALSE;input.done=TRUE;}
 }
 RemoveWindowSubclass(edit,InputEdit,1);ActiveInput=NULL;
 if(hwndCmd==edit){hwndCmd=NULL;DestroyWindow(edit);}
 Enable();DragAcceptFiles(hwndMain,TRUE);GenerateTitle(TRUE);
 if(quit)PostQuitMessage(quitCode);
 *text=input.text;return TRUE;
}
/* Convert one validated scalar without silently substituting a default/best-fit
   byte in legacy encodings. */
static UINT InputScalar(LPCWSTR text,UINT units,BYTE bytes[8])
{
 extern INT AnsiCodePage,OemCodePage;
 BOOL used=FALSE;INT n;UINT i;
 if(UtfEncoding==16){for(i=0;i<units;i++){bytes[2*i]=(BYTE)(UtfLsbFirst?text[i]:text[i]>>8);bytes[2*i+1]=(BYTE)(UtfLsbFirst?text[i]>>8:text[i]);}return units*2;}
 if(CharSet==CS_EBCDIC){if(units!=1 || *text>255)return 0;bytes[0]=MapAnsiToEbcdic[*text];return MapEbcdicToAnsi[bytes[0]]==*text?1:0;}
 {UINT cp=UtfEncoding==8?CP_UTF8:CharSet==CS_OEM?OemCodePage:AnsiCodePage;
  n=WideCharToMultiByte(cp,cp==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS,text,units,(LPSTR)bytes,8,NULL,cp==CP_UTF8?NULL:&used);
 }
 return n>0 && !used?(UINT)n:0;
}
BOOL WvExEncodeLines(LPCWSTR text,BYTE **out,WV_ULONG *length,WV_ULONG *lastStart)
{
 extern INT AnsiCodePage,OemCodePage;
 INT encoding=UtfEncoding,ansi=AnsiCodePage,oem=OemCodePage,newline=DefaultNewLine;UINT charset=CharSet;BOOL little=UtfLsbFirst;
 WV_ULONG size=0,last=0,work=0,capacity=0;BYTE *buffer=NULL,eol[4],encoded[8];UINT eolSize=WvEncodedEol(eol),pass;
 *out=NULL;*length=*lastStart=0;
 for(pass=0;pass<2;pass++){
  LPCWSTR p=text;BOOL lineStart=TRUE,ended=TRUE;size=last=0;
  while(*p){
   UINT n,units=1;BOOL newline=*p==L'\r' || *p==L'\n';WV_ULONG next;
   if(lineStart){last=size;lineStart=FALSE;}
   if(newline){if(*p==L'\r' && p[1]==L'\n')p++;p++;n=eolSize;memcpy(encoded,eol,n);lineStart=ended=TRUE;}
   else{
    units=InputUnits(p);if(!units)goto invalid;
    n=InputScalar(p,units,encoded);if(!n)goto invalid;p+=units;ended=FALSE;
   }
   if(!WvSizeAdd(size,n,&next) || (pass && next>capacity))goto large;
   if(pass)memcpy(buffer+size,encoded,n);size=next;
   if((++work&255)==0 && CheckInterrupt())goto cancelled;
  }
  if(encoding!=UtfEncoding || ansi!=AnsiCodePage || oem!=OemCodePage || newline!=DefaultNewLine || charset!=CharSet || little!=UtfLsbFirst)goto invalid;
  if(!ended){WV_ULONG next;if(!WvSizeAdd(size,eolSize,&next) || (pass && next>capacity))goto large;if(pass)memcpy(buffer+size,eol,eolSize);size=next;}
  if(!pass){capacity=size;if(size){buffer=malloc((size_t)size);if(!buffer)return InputError(L"Not enough memory for the entered lines.");}}
 }
 *out=buffer;*length=size;*lastStart=last;return TRUE;
invalid:free(buffer);return InputError(L"The entered text cannot be represented exactly in this document's encoding.");
large:free(buffer);return InputError(L"The entered lines exceed the document size limit.");
cancelled:free(buffer);return InputError(L"Line input cancelled; document unchanged.");
}
