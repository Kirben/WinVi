/* Line-oriented ex editing commands. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"

extern INT DblQuoteChar,YankIndex;
extern HGLOBAL YankHandles[36];
extern WV_ULONG YankLength[36];
extern HWND hwndSearch,hwndSettings;

static void FreezeLineDialogs(HWND windows[2],BOOL enabled[2])
{
 INT i;windows[0]=hwndSearch;windows[1]=hwndSettings;
 for(i=0;i<2;i++)if(windows[i] && IsWindowEnabled(windows[i])){enabled[i]=TRUE;EnableWindow(windows[i],FALSE);}
}

static BOOL LineError(LPCWSTR text)
{NewStatus(0,(PWSTR)text,NS_ERROR);return FALSE;}
static BOOL Letter(WCHAR c)
{return (c>=L'a' && c<=L'z') || (c>=L'A' && c<=L'Z');}
static PWSTR SkipBlank(PWSTR p)
{while(*p==L' ' || *p==L'\t')p++;return p;}
static BOOL LineArgs(INT kind,PWSTR args,INT *reg,WV_ULONG *count,BOOL *haveCount)
{
 *reg=0;*count=0;*haveCount=FALSE;args=SkipBlank(args);
 if(kind!=0 && *args){
  BOOL quoted=*args==L'"';if(quoted)args++;
  if(Letter(*args) || ((kind==2 || quoted) && *args>=L'0' && *args<=L'9'))*reg=*args++;
  else if(quoted || kind==2)return LineError(L"Expected a register name.");
  args=SkipBlank(args);
 }
 if(kind!=2 && *args>=L'0' && *args<=L'9'){
  *haveCount=TRUE;
  do{UINT digit=*args++-L'0';if(*count>(WV_MAX_POSITION-digit)/10)return LineError(L"Line count is too large.");*count=*count*10+digit;}while(*args>=L'0' && *args<=L'9');
  if(!*count)return LineError(L"Line count must be positive.");
 }
 if(*SkipBlank(args))return LineError(L"Unexpected characters after line command.");
 return TRUE;
}
/* Walk without moving the editor cursor, excluding a Unicode BOM. */
static BOOL LineSpan(WV_ULONG first,WV_ULONG last,PPOSITION begin,PPOSITION end)
{
 POSITION p={FirstPage,0,0};WV_ULONG line=1,work=0;INT c;
 if(CharAt(&p)==C_BOM)CharAndAdvance(&p);
 *begin=p;
 for(;;){
  if(line==first)*begin=p;
  do{
   c=CharAndAdvance(&p);
   if(c==C_ERROR)return LineError(L"Cannot read the addressed lines.");
   if((++work&255)==0 && CheckInterrupt())return FALSE;
  }while(!(CharFlags(c)&1));
  if(line==last){*end=p;return TRUE;}
  if(c==C_EOF)return LineError(L"Line address is outside the file.");
  line++;
 }
}
static void FinishLineEdit(void)
{
 NewPosition(&CurrPos);FindValidPosition(&CurrPos,0);
 InvalidateText();NewScrollPos();ShowEditCaret();
}
UINT WvEncodedEol(BYTE bytes[4])
{
 WCHAR chars[2];UINT i,n=1,used=0;
 chars[0]=(WCHAR)DefaultNewLine;
 if(DefaultNewLine==C_CRLF){chars[0]=L'\r';chars[1]=L'\n';n=2;}
 for(i=0;i<n;i++){
  WCHAR c=chars[i];
  if(UtfEncoding==16){bytes[used++]=(BYTE)(UtfLsbFirst?c:c>>8);bytes[used++]=(BYTE)(UtfLsbFirst?c>>8:c);}
  else bytes[used++]=(BYTE)(CharSet==CS_EBCDIC?MapAnsiToEbcdic[c]:c);
 }
 return used;
}
static INT RegisterLast(BYTE *bytes,WV_ULONG length)
{
 if(UtfEncoding==16)return UtfLsbFirst?bytes[length-2]|bytes[length-1]<<8:bytes[length-2]<<8|bytes[length-1];
 return CharSet==CS_EBCDIC?MapEbcdicToAnsi[bytes[length-1]]:bytes[length-1];
}
static BOOL LinePut(WV_ULONG line,INT reg)
{
 INT index,c;BYTE *bytes,eol[4];UINT eolSize;BOOL prefix=FALSE,suffix,ok=FALSE;
 WV_ULONG length,growth,total;POSITION start,end,p;
 if(reg>=L'0' && reg<=L'9')index=reg-L'0';
 else if(Letter((WCHAR)reg))index=((reg|32)-L'a')+10;
 else index=YankIndex;
 if(index<0 || index>=36 || !YankHandles[index] || !(length=YankLength[index]))return LineError(L"The requested register is empty.");
 if(UtfEncoding==16 && (length&1))return LineError(L"Register contains an incomplete UTF-16 code unit.");
 bytes=GlobalLock(YankHandles[index]);if(!bytes)return LineError(L"Cannot read the requested register.");
 if(line){
  if(!LineSpan(line,line,&start,&end))goto done;
  p=end;c=GoBackAndChar(&p);if(c==C_ERROR){LineError(L"Cannot read the put destination.");goto done;}prefix=!(CharFlags(c)&1);
 }else{end.p=FirstPage;end.i=end.f=0;if(CharAt(&end)==C_BOM)CharAndAdvance(&end);}
 suffix=!(CharFlags(RegisterLast(bytes,length))&1);eolSize=WvEncodedEol(eol);
 growth=(prefix?eolSize:0)+(suffix?eolSize:0);
 p.p=LastPage;p.i=LastPage->Fill;p.f=0;
 if(!WvSizeAdd(growth,length,&growth) || !WvSizeAdd(CountBytes(&p),growth,&total)){LineError(L"Put would overflow the document size.");goto done;}
 StartUndoSequence();SelectCount=0;CurrPos=end;
 if(prefix && !InsertBuffer(eol,eolSize,0))goto edited;
 if(!InsertBuffer(bytes,length,0) || Interrupted)goto edited;
 if(suffix && !InsertBuffer(eol,eolSize,0))goto edited;
 /* ex put leaves the cursor on the last inserted line. */
 p=CurrPos;GoBackAndChar(&p);
 do{c=GoBackAndChar(&p);if(c==C_ERROR){LineError(L"Cannot read the inserted lines.");goto edited;}}while(!(CharFlags(c)&1));
 if(c!=C_EOF)CharAndAdvance(&p);CurrPos=p;ok=TRUE;
edited:
 FinishLineEdit();
done:
 GlobalUnlock(YankHandles[index]);return ok;
}

/* kind: 0 join, 1 yank, 2 put. The caller passes ex address sentinels unchanged. */
BOOL WvExLineCommand(WV_ULONG first,WV_ULONG last,INT kind,PWSTR args,BOOL force)
{
 WV_ULONG total=LinesInFile+IncompleteLastLine(),count,line;INT reg,savedReg,previous=C_UNDEF;
 BOOL haveCount,range=last!=(WV_ULONG)-1,ok=FALSE;POSITION begin,end;
 HWND windows[2]={NULL,NULL};BOOL enabled[2]={FALSE,FALSE};INT i;
 if(HexEditMode || ExecRunning())return LineError(L"Line commands require text mode and no running external command.");
 if(!LineArgs(kind,args,&reg,&count,&haveCount))return FALSE;
 if(kind!=1 && IsViewOnly())return FALSE;
 if(first==(WV_ULONG)-1)first=total?CountLines(&CurrPos)+1:0;
 if(!first && range && last==(WV_ULONG)-2)first=1; /* percent range */
 if(last==(WV_ULONG)-2)last=total;else if(!range)last=first;
 if(kind==2){
  if(range || first>total)return LineError(L"Put accepts one line address, including zero.");
  FreezeLineDialogs(windows,enabled);StartInterruptCheck();ok=LinePut(first,reg);goto done;
 }
 if(!first || first>last || last>total)return LineError(L"Invalid line range.");
 if(haveCount){
  if(kind==1 || range){first=last;count--;}
  last=count>total-first?total:first+count;
 }else if(kind==0 && !range){if(last==total)return LineError(L"No following line to join.");last++;}
 FreezeLineDialogs(windows,enabled);StartInterruptCheck();
 if(!LineSpan(first,last,&begin,&end))goto done;
 if(kind==1){
  savedReg=DblQuoteChar;DblQuoteChar=reg;
  ok=Yank(&begin,CountBytes(&end)-CountBytes(&begin),2);DblQuoteChar=savedReg;goto done;
 }
 if(first==last){ok=TRUE;goto done;}
 StartUndoSequence();SelectCount=0;CurrPos=begin;
 for(line=first;line<last;line++){
  POSITION after;INT c;WV_ULONG removed;BOOL space;
  while(!(CharFlags(c=CharAt(&CurrPos))&1)){
   if(c==C_ERROR){LineError(L"Cannot read the join input.");goto edited;}
   previous=CharAndAdvance(&CurrPos);if(previous==C_UNDEF)previous=0xfffd;
   if(previous==C_ERROR){LineError(L"Cannot read the join input.");goto edited;}
   if(CheckInterrupt())goto edited;
  }
  after=CurrPos;c=CharAndAdvance(&after);if(c==C_EOF)goto edited;
  if(c==C_ERROR){LineError(L"Cannot read the join input.");goto edited;}
  if(!force)while((c=CharAt(&after))==' ' || c=='\t'){
   CharAndAdvance(&after);if(CheckInterrupt())goto edited;
  }
  c=CharAt(&after);
  if(c==C_ERROR){LineError(L"Cannot read the join input.");goto edited;}
  space=!force && previous!=C_UNDEF && previous!=' ' && previous!='\t' && previous!='(' && previous!='[' && previous!='{' && !(CharFlags(c)&1) && c!=')' && c!=']' && c!='}' && c!='.' && c!=',' && c!=';' && c!=':';
  removed=CountBytes(&after)-CountBytes(&CurrPos);
  SetUnsafe();if(!Reserve(&CurrPos,-(WV_LONG)removed,1))goto edited;
  if(space){BYTE blank[2]={UtfLsbFirst?' ':0,UtfLsbFirst?0:' '};if(UtfEncoding!=16)blank[0]=(BYTE)(CharSet==CS_EBCDIC?MapAnsiToEbcdic[' ']:' ');
   if(!InsertBuffer(blank,UtfEncoding==16?2:1,0))goto edited;
  }
  if(CheckInterrupt())goto edited;
 }
 ok=TRUE;
edited:
 FinishLineEdit();
done:
 if(Disabled)Enable();
 for(i=0;i<2;i++)if(enabled[i] && IsWindow(windows[i]))EnableWindow(windows[i],TRUE);
 if(Interrupted)return LineError(L"Line command cancelled; completed edits can be undone.");
 return ok;
}

static WV_ULONG TransferSize(void)
{POSITION p={LastPage,LastPage->Fill,0};return CountBytes(&p);}
/* Checked, cancellable ex shifting. Follow vi Shift's tab/space and blank-line
   policy without its unchecked Reserve writes. Preserve yank/delete registers. */
BOOL WvExIndent(WV_ULONG first,WV_ULONG last,PWSTR args,BOOL right)
{
 WV_ULONG total=LinesInFile+IncompleteLastLine(),repeat=1,count=0,width;
 WV_ULONG line,columns,oldLength,newLength,chars,tabs,i,offset,size,expected,work=0;
 POSITION begin,end,p,lastCursor;BYTE *bytes=NULL;INT c,j;UINT unit=UtfEncoding==16?2:1;
 BOOL ok=FALSE,started=FALSE,enabled[2]={FALSE,FALSE};HWND windows[2]={NULL,NULL};
 if(WvExGlobalActive || HexEditMode || ExecRunning())return LineError(L"Shifting requires text mode and no running external command.");
 if(IsViewOnly())return FALSE;
 while(*args==(right?L'>':L'<')){repeat++;args++;}
 args=SkipBlank(args);
 if(*args){
  if(*args<L'0' || *args>L'9')return LineError(L"Shift accepts only a positive line count.");
  do{UINT digit=*args++-L'0';if(count>(WV_MAX_POSITION-digit)/10)return LineError(L"Shift count is too large.");count=count*10+digit;}while(*args>=L'0' && *args<=L'9');
  if(!count || *SkipBlank(args))return LineError(L"Shift accepts only a positive line count.");
 }
 if(TabStop<1 || TabStop>128 || ShiftWidth<1 || ShiftWidth>128 || repeat>WV_MAX_POSITION/(UINT)ShiftWidth)return LineError(L"Invalid shift or tab width.");
 width=repeat*(UINT)ShiftWidth;
 if(first==(WV_ULONG)-1)first=CountLines(&CurrPos)+1;
 if(!first && last==(WV_ULONG)-2)first=1;
 if(last==(WV_ULONG)-1)last=first;else if(last==(WV_ULONG)-2)last=total;
 if(!first || first>last || last>total)return LineError(L"Invalid shift line range.");
 if(count){first=last;last=count-1>total-first?total:first+count-1;}
 FreezeLineDialogs(windows,enabled);StartInterruptCheck();WvExGlobalActive=TRUE;
 if(!LineSpan(first,last,&begin,&end))goto done;
 lastCursor=begin;
 for(line=first;line<=last;line++){
  if(CheckInterrupt())goto done;
  p=begin;columns=0;offset=CountBytes(&begin);
  while((c=CharAt(&p))==' ' || c=='\t'){
   WV_ULONG add=c=='\t'?(UINT)TabStop-columns%(UINT)TabStop:1;
   if(!WvSizeAdd(columns,add,&columns)){LineError(L"Indentation width overflow.");goto done;}
   if(CharAndAdvance(&p)==C_ERROR)goto readError;
   if((++work&255)==0 && CheckInterrupt())goto done;
  }
  if(c==C_ERROR)goto readError;
  oldLength=CountBytes(&p)-offset;
  if(CharFlags(c)&1)columns=0; /* Whitespace-only lines become empty. */
  else if(right){if(!WvSizeAdd(columns,width,&columns)){LineError(L"Indentation width overflow.");goto done;}}
  else columns=columns>width?columns-width:0;
  tabs=TabStop>1?columns/(UINT)TabStop:0;
  chars=tabs+(TabStop>1?columns%(UINT)TabStop:columns);
  if(chars>WV_MAX_POSITION/unit){LineError(L"Indentation size overflow.");goto done;}
  newLength=chars*unit;size=TransferSize();
  if(!WvSizeAdd(size,newLength,&expected)){LineError(L"Indentation would exceed the document size limit.");goto done;}
  if(newLength){
   bytes=malloc((size_t)newLength);if(!bytes){LineError(L"Not enough memory for indentation.");goto done;}
   for(i=0;i<chars;i++){
    BYTE value=(BYTE)(i<tabs?'\t':' ');
    if(unit==2){bytes[2*i]=UtfLsbFirst?value:0;bytes[2*i+1]=UtfLsbFirst?0:value;}
    else bytes[i]=(BYTE)(CharSet==CS_EBCDIC?MapAnsiToEbcdic[value]:value);
    if((++work&255)==0 && CheckInterrupt())goto done;
   }
  }
  if(oldLength || newLength){
   if(!started){StartUndoSequence();started=TRUE;SelectCount=0;}
   CurrPos=begin;
   if(newLength && (!InsertBuffer(bytes,newLength,0) || Interrupted))goto done;
   if(TransferSize()!=expected){LineError(L"Shift stopped after insertion; completed edits can be undone.");goto done;}
   p.p=FirstPage;p.i=p.f=0;
   if(Advance(&p,offset+newLength)!=offset+newLength)goto readError;
   if(oldLength){
    SelectStart=p;SelectCount=oldLength;
    if(!EnterDeleteForUndo(&SelectStart,SelectCount,0)){SelectCount=0;LineError(L"Cannot record shift undo; original indentation retained.");goto done;}
    DeleteSelected(2|8|64);
    if(TransferSize()!=expected-oldLength){LineError(L"Shift did not remove its complete original indentation.");goto done;}
   }
   p.p=FirstPage;p.i=p.f=0;
   if(Advance(&p,offset+newLength)!=offset+newLength)goto readError;
  }
  free(bytes);bytes=NULL;lastCursor=p;
  if(line==last)break;
  do{
   c=CharAndAdvance(&p);if(c==C_ERROR)goto readError;
   if((++work&255)==0 && CheckInterrupt())goto done;
  }while(!(CharFlags(c)&1));
  if(c==C_EOF)goto readError;
  begin=p;
 }
 CurrPos=lastCursor;ok=TRUE;goto done;
readError:
 LineError(L"Cannot read the shift input.");
done:
 free(bytes);if(started || ok)FinishLineEdit();if(Disabled)Enable();WvExGlobalActive=FALSE;
 for(j=0;j<2;j++)if(enabled[j] && IsWindow(windows[j]))EnableWindow(windows[j],TRUE);
 if(Interrupted)return LineError(L"Shift cancelled; completed edits can be undone.");
 return ok;
}
BOOL WvExMark(WV_ULONG line,PWSTR args)
{
 extern POSITION MarkPos[27];POSITION begin,end,p;INT name,c,j;
 BOOL ok=FALSE,enabled[2]={FALSE,FALSE};HWND windows[2]={NULL,NULL};
 if(WvExGlobalActive || HexEditMode || ExecRunning())return LineError(L"Line marks require text mode and no running external command.");
 args=SkipBlank(args);name=*args;
 if(name<L'a' || name>L'z' || *SkipBlank(args+1))return LineError(L"Mark requires one lowercase letter a-z.");
 if(line==(WV_ULONG)-1)line=CountLines(&CurrPos)+1;
 if(!line || line>LinesInFile+IncompleteLastLine())return LineError(L"Invalid mark line address.");
 FreezeLineDialogs(windows,enabled);StartInterruptCheck();WvExGlobalActive=TRUE;
 if(!LineSpan(line,line,&begin,&end))goto done;
 p=begin;
 while((c=CharAt(&p))==' ' || c=='\t'){
  if(CharAndAdvance(&p)==C_ERROR){LineError(L"Cannot read the mark input.");goto done;}
  if(CheckInterrupt())goto done;
 }
 if(c==C_ERROR){LineError(L"Cannot read the mark input.");goto done;}
 if(CheckInterrupt())goto done;
 if(CharFlags(c)&1)p=begin;
 MarkPos[name&31]=p;ok=TRUE;
done:
 if(Disabled)Enable();WvExGlobalActive=FALSE;
 for(j=0;j<2;j++)if(enabled[j] && IsWindow(windows[j]))EnableWindow(windows[j],TRUE);
 if(Interrupted)return LineError(L"Mark command cancelled; previous mark retained.");
 return ok;
}
static UINT TransferLf(BYTE *bytes)
{
 if(UtfEncoding==16){bytes[0]=UtfLsbFirst?'\n':0;bytes[1]=UtfLsbFirst?0:'\n';return 2;}
 bytes[0]=(BYTE)(CharSet==CS_EBCDIC?MapAnsiToEbcdic['\n']:'\n');return 1;
}
static BOOL TransferRead(PPOSITION start,BYTE *bytes,WV_ULONG length)
{
 POSITION p=*start;
 while(length){
  WV_ULONG n;
  if(ByteAt(&p)<0 || !p.p->PageBuf)return LineError(L"Cannot read the transfer source.");
  n=p.p->Fill-p.i;if(n>length)n=length;
  if(!n)return LineError(L"Transfer source ended unexpectedly.");
  memcpy(bytes,p.p->PageBuf+p.i,(size_t)n);bytes+=n;length-=n;Advance(&p,n);
  if(CheckInterrupt())return FALSE;
 }
 return TRUE;
}

/* Snapshot first: overlap is safe for copy, and move never removes its source
   unless the complete insertion and deletion-undo allocation have succeeded. */
BOOL WvExTransfer(WV_ULONG first,WV_ULONG last,WV_ULONG destination,BOOL move)
{
 WV_ULONG lines=LinesInFile+IncompleteLastLine(),length,sourceOffset,destOffset;
 WV_ULONG oldSize,newSize,payloadSize,growth,lastInserted,removeLength;
 POSITION begin,end,dest,p,cursor;BYTE *snapshot=NULL,prefix[8],suffix[8];
 UINT prefixSize=0,suffixSize=0;INT firstChar,lastChar,previous=C_EOF,next,i;
 BOOL ok=FALSE;HWND windows[2]={NULL,NULL};BOOL enabled[2]={FALSE,FALSE};
 if(HexEditMode || ExecRunning())return LineError(L"Transfers require text mode and no running external command.");
 if(IsViewOnly())return FALSE;
 if(first==(WV_ULONG)-1)first=CountLines(&CurrPos)+1;
 if(!first && last==(WV_ULONG)-2)first=1;
 if(last==(WV_ULONG)-1)last=first;else if(last==(WV_ULONG)-2)last=lines;
 if(!first || first>last || last>lines || destination>lines)return LineError(L"Invalid transfer line address.");
 if(move && destination>=first && destination<=last)return LineError(L"Move destination is inside the source range.");
 FreezeLineDialogs(windows,enabled);StartInterruptCheck();
 if(!LineSpan(first,last,&begin,&end))goto done;
 if(move && destination==first-1){
  if(LineSpan(last,last,&cursor,&p)){CurrPos=cursor;FinishLineEdit();ok=TRUE;}goto done;
 }
 sourceOffset=CountBytes(&begin);length=CountBytes(&end)-sourceOffset;
 if(!length || (UtfEncoding==16 && (length&1))){LineError(L"Transfer source is not a complete text range.");goto done;}
 firstChar=CharAt(&begin);p=end;lastChar=GoBackAndChar(&p);
 if(firstChar==C_ERROR || lastChar==C_ERROR){LineError(L"Cannot read the transfer source.");goto done;}
 removeLength=length;
 if(move){
  INT before,after;p=begin;before=GoBackAndChar(&p);after=CharAt(&end);
  if(before==C_ERROR || after==C_ERROR){LineError(L"Cannot read the move boundary.");goto done;}
  /* Retain the final LF when deletion would combine two surviving lines.
     This LF completes the preceding CR; the following LF stays a blank line. */
  if(before=='\r' && after=='\n')removeLength-=UtfEncoding==16?2:1;
 }
 if(destination){
  if(!LineSpan(destination,destination,&p,&dest))goto done;
  p=dest;previous=GoBackAndChar(&p);
 }else{dest.p=FirstPage;dest.i=dest.f=0;if(CharAt(&dest)==C_BOM)CharAndAdvance(&dest);}
 next=CharAt(&dest);
 if(previous==C_ERROR || next==C_ERROR){LineError(L"Cannot read the transfer destination.");goto done;}
 /* Supply line boundaries at incomplete EOF, and prevent a lone CR and an
    adjacent LF from accidentally collapsing two distinct lines into one. */
 if(destination && !(CharFlags(previous)&1))prefixSize=WvEncodedEol(prefix);
 if(firstChar=='\n' && ((prefixSize && RegisterLast(prefix,prefixSize)=='\r') || (!prefixSize && previous=='\r')))
  prefixSize+=TransferLf(prefix+prefixSize);
 if(!(CharFlags(lastChar)&1))suffixSize=WvEncodedEol(suffix);
 if(next=='\n' && (suffixSize?RegisterLast(suffix,suffixSize):lastChar)=='\r')suffixSize+=TransferLf(suffix+suffixSize);
 if(!WvSizeAdd(length,prefixSize+suffixSize,&payloadSize) || !WvSizeAdd(TransferSize(),payloadSize,&newSize)){
  LineError(L"Transfer would exceed the document size limit.");goto done;
 }
 snapshot=malloc((size_t)payloadSize);
 if(!snapshot){LineError(L"Not enough memory for the transfer snapshot.");goto done;}
 memcpy(snapshot,prefix,prefixSize);
 if(!TransferRead(&begin,snapshot+prefixSize,length))goto done;
 memcpy(snapshot+prefixSize+length,suffix,suffixSize);
 if(CheckInterrupt())goto done;
 oldSize=TransferSize();destOffset=CountBytes(&dest);
 StartUndoSequence();SelectCount=0;CurrPos=dest;
 if(!InsertBuffer(snapshot,payloadSize,0) || Interrupted)goto edited;
 newSize=TransferSize();
 if(newSize<oldSize || (growth=newSize-oldSize)!=payloadSize){LineError(L"Transfer stopped after insertion; completed edits can be undone.");goto edited;}
 lastInserted=destination+(last-first+1);
 if(move){
  WV_ULONG adjusted=sourceOffset+(destOffset<=sourceOffset?growth:0);
  p.p=FirstPage;p.i=p.f=0;
  if(Advance(&p,adjusted)!=adjusted){LineError(L"Original move range is no longer available.");goto edited;}
  SelectStart=p;SelectCount=removeLength;
  if(!EnterDeleteForUndo(&SelectStart,SelectCount,0)){
   SelectCount=0;LineError(L"Cannot record move undo; the original text was retained.");goto edited;
  }
  DeleteSelected(2|8|64); /* no register update; undo was checked above */
  if(TransferSize()!=newSize-removeLength){LineError(L"Move did not remove its complete source range.");goto edited;}
  if(destination>last)lastInserted=destination;
 }
 if(LineSpan(lastInserted,lastInserted,&cursor,&p)){CurrPos=cursor;ok=TRUE;}
edited:
 FinishLineEdit();
done:
 free(snapshot);if(Disabled)Enable();
 for(i=0;i<2;i++)if(enabled[i] && IsWindow(windows[i]))EnableWindow(windows[i],TRUE);
 if(Interrupted)return LineError(L"Transfer cancelled; completed edits can be undone.");
 return ok;
}
