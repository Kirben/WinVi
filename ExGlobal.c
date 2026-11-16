/* Synchronous, two-pass ex global commands. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include <limits.h>
#include "winvi.h"
#include "page.h"

BOOL WvExGlobalActive, WvExGlobalUndo;
extern WCHAR BufferW[WV_TEXT_CAP];
extern HWND hwndSearch,hwndSettings;
static WV_ULONG *Targets, TargetCount, NextTarget;
static WV_LONG TargetShift;
static BOOL Tracking, TrackingError;

/* Bodies operate on the current original line, in ascending order. Future
   targets therefore share one lazy offset adjustment, avoiding O(n*n) updates.
   A multiline replacement can consume future original lines: drop their marks. */
void WvExGlobalEdit(WV_ULONG at, WV_ULONG removed, WV_ULONG added)
{
 WV_ULONG target;
 if(!Tracking)return;
 while(NextTarget<TargetCount){
  target=TargetShift<0?Targets[NextTarget]-(WV_ULONG)(-TargetShift):Targets[NextTarget]+(WV_ULONG)TargetShift;
  if(target<at){TrackingError=TRUE;Interrupted=TRUE;return;}
  if(!removed || target-at>=removed)break;
  NextTarget++;
 }
 /* Actual document-size differences remain within the signed position range. */
 if(added>=removed){
  WV_ULONG delta=added-removed;
  if(TargetShift>0 && delta>WV_MAX_POSITION-(WV_ULONG)TargetShift){TrackingError=TRUE;Interrupted=TRUE;return;}
  TargetShift+=(WV_LONG)delta;
 }else{
  WV_ULONG delta=removed-added;
  if(TargetShift<0 && delta>WV_MAX_POSITION-(WV_ULONG)(-TargetShift)){TrackingError=TRUE;Interrupted=TRUE;return;}
  TargetShift-=(WV_LONG)delta;
 }
}
static BOOL GlobalError(LPCWSTR message)
{NewStatus(0,(PWSTR)message,NS_ERROR);return FALSE;}
static BOOL BodyIs(LPCWSTR body,LPCWSTR name)
{
 size_t n=wcslen(name);if(wcsncmp(body,name,n))return FALSE;
 body+=n;while(*body==L' ' || *body==L'\t')body++;return !*body;
}

static LRESULT CALLBACK OutputProc(HWND window,UINT message,WPARAM w,LPARAM l)
{
 HWND edit=GetDlgItem(window,1);
 switch(message){
 case WM_CREATE:{CREATESTRUCTW *create=(CREATESTRUCTW*)l;
  edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",NULL,WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,0,0,0,0,window,(HMENU)1,hInst,NULL);
  if(!edit)return -1;
  SendMessage(edit,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),FALSE);
  SendMessage(edit,EM_LIMITTEXT,INT_MAX-1,0);
  if(!SetWindowTextW(edit,(LPCWSTR)create->lpCreateParams))return -1;
  return 0;}
 case WM_SIZE:MoveWindow(edit,0,0,LOWORD(l),HIWORD(l),TRUE);return 0;
 case WM_SETFOCUS:SetFocus(edit);return 0;
 }
 return DefWindowProcW(window,message,w,l);
}
static BOOL ShowOutput(WCHAR *text)
{
 WNDCLASSW cls={0};HWND window;
 cls.lpfnWndProc=OutputProc;cls.hInstance=hInst;cls.lpszClassName=L"WinViExOutput";
 cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return FALSE;
 window=CreateWindowExW(0,cls.lpszClassName,L"WinVi - ex command output",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,800,500,hwndMain,NULL,hInst,text);
 if(!window)return FALSE;ShowWindow(window,SW_SHOWNORMAL);return TRUE;
}
static BOOL AppendOutput(WCHAR **text,size_t *length,size_t *capacity,WCHAR c)
{
 if(*length+2>*capacity){
  size_t next=*capacity?*capacity*2:1024;WCHAR *grown;
  if(next>(size_t)INT_MAX/sizeof(WCHAR))next=(size_t)INT_MAX/sizeof(WCHAR);
  if(next<=*length+1)return FALSE;
  grown=realloc(*text,next*sizeof(WCHAR));if(!grown)return FALSE;
  *text=grown;*capacity=next;
 }
 (*text)[(*length)++]=c;(*text)[*length]=0;return TRUE;
}
/* Page readers return bytes in legacy encodings. Convert those to the wide
   output window, keeping a valid DBCS pair together. EBCDIC is already mapped
   to ANSI by CharAndAdvance. */
static INT DisplayChar(PPOSITION pos)
{
 extern INT AnsiCodePage,OemCodePage;
 INT c=CharAndAdvance(pos),codepage;CHAR bytes[2];WCHAR wide;
 if(UtfEncoding || c<128)return c;
 codepage=CharSet==CS_OEM?OemCodePage:AnsiCodePage;bytes[0]=(CHAR)c;
 if(CharSet!=CS_EBCDIC && IsDBCSLeadByteEx(codepage,(BYTE)c)){
  INT next=ByteAt(pos);if(next==C_ERROR)return C_ERROR;bytes[1]=(CHAR)next;
  if(next>=0 && MultiByteToWideChar(codepage,MB_ERR_INVALID_CHARS,bytes,2,&wide,1)==1){Advance(pos,1);return wide;}
  return 0xfffd;
 }
 return MultiByteToWideChar(codepage,0,bytes,1,&wide,1)==1?wide:0xfffd;
}
/* Standalone display commands share the global output window, but do not
   compile a search, edit the document, or start an undo sequence. */
static BOOL OutputDivider(WCHAR **text,size_t *used,size_t *capacity)
{
 INT i;for(i=0;i<40;i++)if(!AppendOutput(text,used,capacity,L'-'))return FALSE;
 return AppendOutput(text,used,capacity,L'\r') && AppendOutput(text,used,capacity,L'\n');
}
static BOOL DisplayRange(WV_ULONG first,WV_ULONG last,INT flags,WV_ULONG target,WV_ULONG divider)
{
 WV_ULONG line=1,work=0;POSITION pos={FirstPage,0,0},cursor=CurrPos;
 WCHAR *output=NULL;size_t used=0,capacity=0;INT c,i;
 BOOL ok=FALSE,enabled[2]={FALSE,FALSE};HWND dialogs[2]={hwndSearch,hwndSettings};
 for(i=0;i<2;i++)if(dialogs[i] && IsWindowEnabled(dialogs[i])){enabled[i]=TRUE;EnableWindow(dialogs[i],FALSE);}
 WvExGlobalActive=TRUE;StartInterruptCheck();Disable(1);DragAcceptFiles(hwndMain,FALSE);
 if(CharAt(&pos)==C_BOM)CharAndAdvance(&pos);
 while(line<=last){
  if(CheckInterrupt())goto done;
  if(line==target)cursor=pos;
  if(line>=first && divider && line==divider)if(!OutputDivider(&output,&used,&capacity))goto noMemory;
  if(line>=first && (flags&1)){WCHAR number[32],*p;_snwprintf(number,32,L"%I64u\t",line);for(p=number;*p;p++)if(!AppendOutput(&output,&used,&capacity,*p))goto noMemory;}
  for(;;){
   c=DisplayChar(&pos);
   if(c==C_ERROR){GlobalError(L"Cannot read the display input.");goto done;}
   if(CharFlags(c)&1)break;
   if(line>=first){
    if((flags&2) && ((c>=0 && c<32) || c==127)){
     if(!AppendOutput(&output,&used,&capacity,L'^') || !AppendOutput(&output,&used,&capacity,c==127?L'?':(WCHAR)(c+64)))goto noMemory;
    }else if(!AppendOutput(&output,&used,&capacity,c<0?0xfffd:c==0?0x2400:(WCHAR)c))goto noMemory;
   }
   if((++work&255)==0 && CheckInterrupt())goto done;
  }
  if(line>=first){
   if((flags&2) && !AppendOutput(&output,&used,&capacity,L'$'))goto noMemory;
   if(!AppendOutput(&output,&used,&capacity,L'\r') || !AppendOutput(&output,&used,&capacity,L'\n'))goto noMemory;
  }
  if(divider && line==divider)if(!OutputDivider(&output,&used,&capacity))goto noMemory;
  if(line==last)break;
  if(c==C_EOF){GlobalError(L"Display range ended unexpectedly.");goto done;}
  line++;
 }
 if(CheckInterrupt())goto done;
 if(!ShowOutput(output)){GlobalError(L"Cannot display ex output.");goto done;}
 NewPosition(&cursor);ok=TRUE;goto done;
noMemory:
 GlobalError(L"Not enough memory for ex output.");
done:
 free(output);if(Disabled)Enable();DragAcceptFiles(hwndMain,TRUE);
 for(i=0;i<2;i++)if(enabled[i] && IsWindow(dialogs[i]))EnableWindow(dialogs[i],TRUE);
 Jump(&CurrPos);FindValidPosition(&CurrPos,0);ShowEditCaret();WvExGlobalActive=FALSE;
 if(Interrupted)return GlobalError(L"Display cancelled; document unchanged.");
 return ok;
}
BOOL WvExDisplay(WV_ULONG first,WV_ULONG last,PWSTR args,INT kind)
{
 WV_ULONG total=LinesInFile+IncompleteLastLine(),count=0;
 if(WvExGlobalActive || HexEditMode || ExecRunning())return GlobalError(L"Display commands require text mode and no running external command.");
 while(*args==L' ' || *args==L'\t')args++;
 if(*args){
  if(*args<L'0' || *args>L'9')return GlobalError(L"Display accepts only a positive line count.");
  do{UINT digit=*args++-L'0';if(count>(WV_MAX_POSITION-digit)/10)return GlobalError(L"Display count is too large.");count=count*10+digit;}while(*args>=L'0' && *args<=L'9');
  while(*args==L' ' || *args==L'\t')args++;
  if(!count || *args)return GlobalError(L"Display accepts only a positive line count.");
 }
 if(first==(WV_ULONG)-1)first=CountLines(&CurrPos)+1;
 if(!first && last==(WV_ULONG)-2)first=1;
 if(last==(WV_ULONG)-1)last=first;else if(last==(WV_ULONG)-2)last=total;
 if(!first || first>last || last>total || !total)return GlobalError(L"Invalid display line range.");
 if(count){first=last;last=count-1>total-first?total:first+count-1;}
 return DisplayRange(first,last,kind==2?1:kind==3?2:0,last,0);
}
/* Counts are clipped before addition/subtraction, including near 64-bit EOF.
   A bare z advances past dot; an explicit mode starts from dot itself. */
BOOL WvExZ(WV_ULONG address,PWSTR args)
{
 WV_ULONG total=LinesInFile+IncompleteLastLine(),count=0,first,last,before,after;
 WCHAR mode=0;INT flags=0;
 if(WvExGlobalActive || HexEditMode || ExecRunning())return GlobalError(L"Display commands require text mode and no running external command.");
 while(*args==L' ' || *args==L'\t')args++;
 if(*args && wcschr(L"+-.^=",*args))mode=*args++;
 while(*args==L' ' || *args==L'\t')args++;
 if(*args>=L'0' && *args<=L'9'){
  do{UINT digit=*args++-L'0';if(count>(WV_MAX_POSITION-digit)/10)return GlobalError(L"Display count is too large.");count=count*10+digit;}while(*args>=L'0' && *args<=L'9');
  if(!count)return GlobalError(L"z requires a positive count.");
 }else count=CrsrLines>1?(WV_ULONG)CrsrLines-1:1;
 while(*args){
  if(*args==L'#')flags|=1;else if(*args==L'l')flags|=2;
  else if(*args!=L'p' && *args!=L' ' && *args!=L'\t')return GlobalError(L"Use z [type] [count] [p l #].");
  args++;
 }
 if(address==(WV_ULONG)-1){address=CountLines(&CurrPos)+1;if(!mode && address<WV_MAX_POSITION)address++;}
 if(!address || address>total)return GlobalError(L"Invalid z line address.");
 first=last=address;
 switch(mode){
 case L'-': first=count>address?1:address-count+1;break;
 case L'^':
  first=count>(address-1)/2?1:address-count-count+1;
  last=count-1>total-first?total:first+count-1;break;
 case L'.':case L'=':
  /* '=' reserves two output rows for the separators. Small counts still
     show the addressed line and both separators. Even counts favor above. */
  before=(mode==L'=' && count>2?count-2:mode==L'='?1:count)-1;
  after=before/2;before-=after;
  first=before>=address?1:address-before;
  last=after>total-address?total:address+after;break;
 default:last=count-1>total-address?total:address+count-1;break;
 }
 return DisplayRange(first,last,flags,mode==L'='?address:last,mode==L'='?address:0);
}

BOOL WvExGlobal(WV_ULONG first,WV_ULONG last,PWSTR args,BOOL inverse)
{
 WCHAR delimiter,*end,*classStart=NULL,*pattern=NULL,*body,*action=NULL,*output=NULL;
 size_t used=0,space=0,capacity=0;WV_ULONG line=1,total,offset;POSITION pos,start,finish,outputPosition;
 INT c,error=0,kind=0;BOOL ok=FALSE,bracket=FALSE,haveOutputPosition=FALSE;WV_ULONG visited=0;
 HWND dialogs[2];BOOL dialogEnabled[2]={FALSE,FALSE};INT dialogIndex;
 if(WvExGlobalActive)return GlobalError(L"Nested global commands are not supported.");
 Interrupted=FALSE;
 if(HexEditMode || ExecRunning())return GlobalError(L"Global commands require text mode and no running external command.");
 delimiter=*args;
 if(delimiter<=L' ' || iswalnum(delimiter) || delimiter==L'\\')return GlobalError(L"Global requires a delimited search pattern.");
 for(end=args+1;*end;end++){
  if(*end==L'\\' && end[1]){end++;continue;}
  if(bracket){if(*end==L']' && end>classStart+1 && !(end==classStart+2 && classStart[1]==L'^'))bracket=FALSE;}
  else if(*end==delimiter)break;
  else if(*end==L'['){bracket=TRUE;classStart=end;}
 }
 if(!*end)return GlobalError(L"Missing closing global-pattern delimiter.");
 body=end+1;while(*body==L' ' || *body==L'\t')body++;
 if(!*body || BodyIs(body,L"p") || BodyIs(body,L"print"))kind=1;
 else if(BodyIs(body,L"#") || BodyIs(body,L"number"))kind=2;
 else if(BodyIs(body,L"="))kind=3;
 else if(BodyIs(body,L"d") || BodyIs(body,L"delete"))kind=4;
 else if(*body==L's' && (!iswalpha(body[1]) || !wcsncmp(body,L"substitute",10))){kind=5;body+=!wcsncmp(body,L"substitute",10)?10:1;}
 else return GlobalError(L"Global supports one current-line command: p, #, =, d or s. Command lists, addresses, shells and nested globals are not supported.");
 if(kind==5)while(*body==L' ' || *body==L'\t')body++;
 if(kind>=4 && IsViewOnly())return FALSE;
 pattern=malloc(((size_t)(end-args)+2)*sizeof(WCHAR));
 if(!pattern)return GlobalError(L"Not enough memory for the global pattern.");
 memcpy(pattern,args,((size_t)(end-args)+1)*sizeof(WCHAR));pattern[end-args+1]=0;pattern[0]=L'/';
 if(!BuildMatchList(pattern,delimiter,&error) || !StartSearch())goto done;
 total=LinesInFile+IncompleteLastLine();if(!total)total=1;
 if(first==(WV_ULONG)-1){first=1;last=total;}
 else {if(!first)first=1;if(last==(WV_ULONG)-1)last=first;else if(last==(WV_ULONG)-2)last=total;}
 if(first>last || last>total){GlobalError(L"Invalid global line range.");goto done;}
 WvExGlobalActive=TRUE;
 /* These modeless dialogs can replace the compiled search or change encoding
    while CheckInterrupt pumps messages. Keep their existing state until done. */
 dialogs[0]=hwndSearch;dialogs[1]=hwndSettings;
 for(dialogIndex=0;dialogIndex<2;dialogIndex++)if(dialogs[dialogIndex] && IsWindowEnabled(dialogs[dialogIndex])){
  dialogEnabled[dialogIndex]=TRUE;EnableWindow(dialogs[dialogIndex],FALSE);
 }
 Disable(1);DragAcceptFiles(hwndMain,FALSE);
 pos.p=FirstPage;pos.i=pos.f=0;
 if(CharAt(&pos)==C_BOM)CharAndAdvance(&pos);
 /* Pass one never changes the buffer. Allocation/cancellation errors therefore
    cannot leave a partially selected command execution. No phantom EOF line. */
 for(;;){
  start=finish=pos;
  for(;;){c=CharAt(&pos);if(c==C_ERROR){GlobalError(L"Cannot read the global input.");goto done;}if(CharFlags(c)&1)break;CharAndAdvance(&pos);if(CheckInterrupt())goto done;}
  finish=pos;
  if(line>=first && line<=last){
   INT match=WvExMatchLine(&start,&finish);
   if(match<0)goto done;
   if((match!=0)!=inverse){
    if(TargetCount==capacity){
     size_t next=capacity?capacity*2:256;WV_ULONG *grown;
     if(next<capacity || next>SIZE_MAX/sizeof(*Targets)){GlobalError(L"Too many global targets.");goto done;}
     grown=realloc(Targets,next*sizeof(*Targets));if(!grown){GlobalError(L"Not enough memory for global targets.");goto done;}
     Targets=grown;capacity=next;
    }
    Targets[TargetCount++]=CountBytes(&start);
   }
  }
  if(c==C_EOF || line>=last)break;
  CharAndAdvance(&pos);if(CharAt(&pos)==C_EOF)break;line++;
 }
 if(!TargetCount){GlobalError(L"No lines selected by the global command.");goto done;}
 if(kind>=4){StartUndoSequence();WvExGlobalUndo=TRUE;}
 Tracking=TRUE;WvExGlobalEditHook=WvExGlobalEdit;TargetShift=0;
 while(NextTarget<TargetCount){
  WV_ULONG index=NextTarget++;
  offset=TargetShift<0?Targets[index]-(WV_ULONG)(-TargetShift):Targets[index]+(WV_ULONG)TargetShift;
  if(CheckInterrupt())goto done;
  pos.p=FirstPage;pos.i=pos.f=0;if(Advance(&pos,offset)!=offset){GlobalError(L"A global target is no longer available.");goto done;}
  line=CountLines(&pos)+1;
  if(kind<=3){
   WCHAR number[32],*p;
   outputPosition=pos;haveOutputPosition=TRUE;
   if(kind!=1){_snwprintf(number,32,L"%I64u",line);for(p=number;*p;p++)if(!AppendOutput(&output,&used,&space,*p))goto noMemory;
    if(kind==2 && !AppendOutput(&output,&used,&space,L'\t'))goto noMemory;}
   if(kind!=3)while(!(CharFlags(c=CharAt(&pos))&1)){
    c=CharAndAdvance(&pos);if(c==C_ERROR)goto done;
    if(!AppendOutput(&output,&used,&space,c<0?0xfffd:(WCHAR)c))goto noMemory;
    if(CheckInterrupt())goto done;
   }
   if(!AppendOutput(&output,&used,&space,L'\r') || !AppendOutput(&output,&used,&space,L'\n'))goto noMemory;
  }else{
   NewPosition(&pos);FindValidPosition(&CurrPos,0);StartUndoSequence();
   if(kind==4){
    finish=pos;while(!(CharFlags(c=CharAndAdvance(&finish))&1));
    SelectStart=pos;SelectCount=CountBytes(&finish)-offset;
    if(SelectCount)DeleteSelected(1);
   }else{
    action=_wcsdup(body);if(!action)goto noMemory;
    if(!SubstituteCmd(line,line,action)){free(action);action=NULL;goto done;}
    free(action);action=NULL;
   }
   if(TrackingError || Interrupted)goto done;
  }
  visited++;
  if(!Disabled)Disable(1);
 }
 Tracking=FALSE;
 if(haveOutputPosition)NewPosition(&outputPosition);
 if(output && !ShowOutput(output)){GlobalError(L"Cannot display global output.");goto done;}
 _snwprintf(BufferW,WSIZE(BufferW),L"Global: %I64u original line(s) processed.",visited);NewStatus(0,BufferW,NS_NORMAL);ok=TRUE;
 goto done;
noMemory:
 GlobalError(L"Not enough memory for global command output.");
done:
 Tracking=FALSE;WvExGlobalEditHook=NULL;WvExGlobalUndo=FALSE;
 free(action);free(pattern);free(output);free(Targets);Targets=NULL;TargetCount=NextTarget=0;TargetShift=0;TrackingError=FALSE;
 if(WvExGlobalActive){
  if(Disabled)Enable();DragAcceptFiles(hwndMain,TRUE);
  for(dialogIndex=0;dialogIndex<2;dialogIndex++)if(dialogEnabled[dialogIndex] && IsWindow(dialogs[dialogIndex]))EnableWindow(dialogs[dialogIndex],TRUE);
  Jump(&CurrPos);FindValidPosition(&CurrPos,0);ShowEditCaret();WvExGlobalActive=FALSE;
 }
 if(Interrupted)GlobalError(L"Global command cancelled; completed edits can be undone.");
 return ok;
}
