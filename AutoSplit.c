/* Automatic hard line splitting for typed insert-mode text. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include "winvi.h"
#include "page.h"
#include "status.h"

/* A bounded look-back keeps typing in existing enormous lines responsive.
   Such lines are left intact; this feature is not paragraph reformatting. */
#define SPLIT_SCAN_LIMIT 8192
static WV_ULONG SplitDistance(PPOSITION from,PPOSITION to)
{
 POSITION p=*from;WV_ULONG distance=0;
 while(p.p && p.p!=to->p){distance+=p.p->Fill-p.i;p.p=p.p->Next;p.i=0;}
 return distance+to->i-p.i;
}
static INT SplitScalar(PPOSITION p)
{
 if(UtfEncoding==8 && ByteAt(p)>=0x80){
  POSITION q=*p;INT first=ByteAndAdvance(&q),count,i,value,min;
  if(first>=0xc2 && first<=0xdf){count=1;value=first&31;min=0x80;}
  else if(first>=0xe0 && first<=0xef){count=2;value=first&15;min=0x800;}
  else if(first>=0xf0 && first<=0xf4){count=3;value=first&7;min=0x10000;}
  else return C_UNDEF;
  for(i=0;i<count;i++){INT b=ByteAndAdvance(&q);if(b<0x80 || b>0xbf)return C_UNDEF;value=(value<<6)|(b&63);}
  if(value<min || value>0x10ffff || (value>=0xd800 && value<=0xdfff))return C_UNDEF;
  q.f=0;*p=q;return value;
 }
 INT c=CharAndAdvance(p);
 if(UtfEncoding && c>=0xd800 && c<=0xdbff){
  INT low=CharAt(p);
  if(low>=0xdc00 && low<=0xdfff){CharAndAdvance(p);return 0x10000+((c-0xd800)<<10)+(low-0xdc00);}
 }
 return c;
}
void WvAutoSplitLine(INT typed)
{
 POSITION start,p,before,gapStart,gapEnd,indentEnd;
 WV_ULONG gapOffset,gapBytes,indentBytes,tail,oldLines,inserted;
 INT c,n=0,column=0,indentColumn=0,next,tab=TabStop>0?TabStop:1;
 BOOL word=FALSE,inGap=FALSE,candidate=FALSE,leading=TRUE,ok;
 BYTE *buffer;size_t size=0;INT eol=DefaultNewLine;
 if(!AutoSplitFlag || AutoSplitWidth<1 || AutoSplitWidth>1000 || Mode!=InsertMode ||
    HexEditMode || ViewOnlyFlag || typed<32 || typed==' ' || typed=='\t')return;
 start=CurrPos;
 for(;;){
  before=start;
  if(WvDbcsText()){if(!WvDbcsPrevious(&before))break;}
  else if(!GoBack(&before,UtfEncoding==16?2:1))break;
  if(UtfEncoding==8){
   INT bytes=0,b=ByteAt(&before);
   while(b>=0x80 && b<0xc0 && bytes++<3){if(!GoBack(&before,1))return;b=ByteAt(&before);}
  }
  before.f=0;p=before;c=SplitScalar(&p);
  if(c<0 && c!=C_CRLF)return;
  if(CharFlags(c)&1)break;
  if(++n>SPLIT_SCAN_LIMIT)return;
  start=before;
 }
 if(start.p==FirstPage && !start.i && CharAt(&start)==C_BOM)SplitScalar(&start);
 p=start;indentEnd=start;
 while(ComparePos(&p,&CurrPos)<0){
  before=p;c=SplitScalar(&p);if(c<0)return;
  if(c==C_BOM && before.p==FirstPage && before.i==0)continue;
  next=c=='\t'?column+tab-column%tab:column+1;
  if(c==' ' || c=='\t'){
   if(leading){indentEnd=p;indentColumn=next;}
   if(word && !inGap && (column<=AutoSplitWidth || !candidate)){gapStart=before;gapEnd=p;candidate=TRUE;inGap=TRUE;}
   else if(inGap)gapEnd=p;
  }else{leading=FALSE;word=TRUE;inGap=FALSE;}
  column=next;
 }
 if(column<=AutoSplitWidth || !candidate)return;
 if(AutoIndentFlag && indentColumn>=AutoSplitWidth)return;
 gapOffset=SplitDistance(&start,&gapStart);gapBytes=SplitDistance(&gapStart,&gapEnd);
 tail=SplitDistance(&gapEnd,&CurrPos);
 indentBytes=AutoIndentFlag?SplitDistance(&start,&indentEnd):0;
 buffer=(BYTE*)malloc((size_t)indentBytes+4);
 if(!buffer)return; /* Typed character remains intact if planning allocation fails. */
 if(eol!=C_CRLF && eol!='\r' && eol!='\n')eol=C_CRLF;
 {INT chars[2],i,count=eol==C_CRLF?2:1;chars[0]=eol==C_CRLF?'\r':eol;chars[1]='\n';
  for(i=0;i<count;i++){
   INT value=chars[i];if(CharSet==CS_EBCDIC)value=MapAnsiToEbcdic[value];
   if(UtfEncoding==16){buffer[size++]=UtfLsbFirst?(BYTE)value:0;buffer[size++]=UtfLsbFirst?0:(BYTE)value;}
   else buffer[size++]=(BYTE)value;
  }
 }
 p=start;while(indentBytes--){c=ByteAndAdvance(&p);if(c<0){free(buffer);return;}buffer[size++]=(BYTE)c;}
 /* Insert before deleting the separator: allocation failure must not lose words.
    InsertBuffer can stop partway; retain that undoable partial insertion and
    the original separator, rather than deleting any original text on failure. */
 CurrPos=gapEnd;NewPosition(&CurrPos);oldLines=LinesInFile;
 ok=InsertBuffer(buffer,(WV_ULONG)size,0);free(buffer);
 inserted=SplitDistance(&start,&CurrPos)-(gapOffset+gapBytes);
 if(ok){
  p=CurrPos;GoBack(&p,inserted+gapBytes);
  if(!Reserve(&p,-(WV_LONG)gapBytes,0)){NewPosition(&CurrPos);return;}
  CurrPos=p;Advance(&CurrPos,inserted+tail);
 }else{
  /* Reserve historically increments line metadata before a failed allocation.
     With no bytes inserted there is no new newline to count. */
  if(!inserted)LinesInFile=oldLines;
  Advance(&CurrPos,tail);
 }
 InvalidateArea(&start,0,7);FindValidPosition(&CurrPos,1);NewPosition(&CurrPos);
 NewScrollPos();ShowEditCaret();GetXPos(&CurrCol);
}
