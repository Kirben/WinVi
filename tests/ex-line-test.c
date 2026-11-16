/* Parser/register probes. Document edit bytes are checked by ex-line.ps1. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failSnapshot;
static void *ProbeAllocate(size_t n){return failSnapshot?NULL:malloc(n);}
#define malloc ProbeAllocate
#define NewStatus ProbeNotice
#define NewPosition ProbePosition
#define FindValidPosition ProbeValid
#define InvalidateText ProbePaint
#define NewScrollPos ProbeScroll
#define ShowEditCaret ProbeCaret
#define InsertBuffer ProbeInsert
#define StartUndoSequence ProbeUndo
#define IsViewOnly ProbeReadOnly
#define CheckInterrupt ProbeInterrupt
#define StartInterruptCheck ProbeStart
#define ExecRunning ProbeExec
#define Reserve ProbeReserve
#define SetUnsafe ProbeUnsafe
#define CharAt ProbeCharAt
#define CharAndAdvance ProbeAdvance
#define GoBackAndChar ProbeBack
#define EnterDeleteForUndo ProbeDeleteUndo
#define DeleteSelected ProbeDelete
#include "../ExLine.c"
#undef malloc
#undef CharAt
#undef CharAndAdvance
#undef GoBackAndChar
INT CharAt(PPOSITION);INT CharAndAdvance(PPOSITION);INT GoBackAndChar(PPOSITION);
static int checks,readOnly,outputLength,undoCalls,reserves,cancel,dialogsFrozen,readFault;
static int transferModel,failInsert,failDeleteUndo,deleteCalls;
static BYTE output[16384];
void ProbeNotice(INT field,PWSTR text,INT flags){(void)field;(void)text;(void)flags;}
WV_LONG ProbePosition(PPOSITION p){CurrPos=*p;return 0;}
void ProbeValid(PPOSITION p,WORD flags){(void)p;(void)flags;}
void ProbePaint(void){} void ProbeScroll(void){} void ProbeCaret(void){} void ProbeUnsafe(void){}
void ProbeUndo(void){undoCalls++;}
BOOL ProbeReadOnly(void){return readOnly;}
BOOL ProbeInterrupt(void){if(hwndSearch && hwndSettings && !IsWindowEnabled(hwndSearch) && !IsWindowEnabled(hwndSettings))dialogsFrozen=1;if(cancel)Interrupted=TRUE;return Interrupted;}
void ProbeStart(void){Interrupted=FALSE;}
BOOL ProbeExec(void){return FALSE;}
BOOL ProbeReserve(PPOSITION p,WV_LONG n,WORD flags){(void)p;(void)n;(void)flags;reserves++;return FALSE;}
INT ProbeCharAt(PPOSITION p){return readFault==1?C_ERROR:CharAt(p);}
INT ProbeAdvance(PPOSITION p){return readFault==2?C_ERROR:CharAndAdvance(p);}
INT ProbeBack(PPOSITION p){return readFault==3 || (readFault==4 && outputLength)?C_ERROR:GoBackAndChar(p);}
BOOL ProbeInsert(LPBYTE bytes,WV_ULONG length,WORD flags){(void)flags;if(failInsert || length>sizeof(output)-outputLength)return FALSE;memcpy(output+outputLength,bytes,(size_t)length);outputLength+=(int)length;
 if(transferModel){UINT at=CurrPos.i;PAGE *p=CurrPos.p;if(p->Fill+length>4096)return FALSE;memmove(p->PageBuf+at+length,p->PageBuf+at,p->Fill-at);memcpy(p->PageBuf+at,bytes,(size_t)length);p->Fill+=(UINT)length;CurrPos.i+=(UINT)length;LinesInFile=p->Newlines=CountNewlinesInBuf(p->PageBuf,p->Fill,NULL,NULL);}
 return TRUE;}
BOOL ProbeDeleteUndo(PPOSITION p,WV_ULONG length,WORD flags){(void)p;(void)length;(void)flags;return !failDeleteUndo;}
void ProbeDelete(INT flags){PAGE *p=SelectStart.p;UINT at=SelectStart.i;(void)flags;deleteCalls++;memmove(p->PageBuf+at,p->PageBuf+at+SelectCount,(size_t)(p->Fill-at-SelectCount));p->Fill-=(UINT)SelectCount;SelectCount=0;CurrPos=SelectStart;LinesInFile=p->Newlines=CountNewlinesInBuf(p->PageBuf,p->Fill,NULL,NULL);}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
extern INT YankSubIndex[10];
extern INT AnsiCodePage,OemCodePage;
static void FreeRegisters(void){int i,j;for(i=0;i<36;i++)if(YankHandles[i]){for(j=0;j<i;j++)if(YankHandles[j]==YankHandles[i])break;if(j==i)GlobalFree(YankHandles[i]);}memset(YankHandles,0,sizeof(HGLOBAL)*36);memset(YankLength,0,sizeof(WV_ULONG)*36);memset(YankSubIndex,0,sizeof(INT)*10);YankIndex=DblQuoteChar=0;}
static void Document(const BYTE *bytes,int length,int utf,BOOL little){
 static PAGE page;static BYTE data[4096];
 memset(&page,0,sizeof(page));memcpy(data,bytes,length);page.PageBuf=data;page.Fill=length;page.PageNo=1;
 FirstPage=LastPage=&page;CurrPos.p=ScreenStart.p=&page;CurrPos.i=CurrPos.f=ScreenStart.i=ScreenStart.f=0;
 UtfEncoding=utf;UtfLsbFirst=little;CharSet=utf==16?(little?CS_UTF16LE:CS_UTF16BE):utf==8?CS_UTF8:CS_ANSI;
 HexEditMode=FALSE;DefaultNewLine='\n';LinesInFile=page.Newlines=CountNewlinesInBuf(data,length,NULL,NULL);
 AnsiCodePage=1252;OemCodePage=437;readOnly=outputLength=undoCalls=reserves=cancel=Interrupted=Disabled=readFault=0;
 transferModel=failInsert=failDeleteUndo=deleteCalls=failSnapshot=0;SelectCount=0;
}
/* Exhaustive source/destination permutations against a separate line-array
   oracle. The model edits one page; real paging/undo is exercised by the UI. */
static void Transfers(void){
 int utf,eol,total,first,last,dest,move,line,j,n,wantn;BYTE input[128],want[256];
 for(utf=0;utf<3;utf++)for(eol=0;eol<2;eol++)for(total=1;total<=6;total++)
 for(first=1;first<=total;first++)for(last=first;last<=total;last++)
 for(dest=0;dest<=total;dest++)for(move=0;move<=1;move++){
  if(move && dest>=first && dest<=last)continue;
  n=wantn=0;
  if(utf){input[n++]=want[wantn++]=(BYTE)(utf==1?0xff:0xfe);input[n++]=want[wantn++]=(BYTE)(utf==1?0xfe:0xff);}
  for(line=1;line<=total;line++){
   BYTE chars[3]={(BYTE)('a'+line-1),'\r','\n'};
   for(j=0;j<(eol?3:2);j++){BYTE c=j==0?chars[0]:eol?chars[j]:'\n';if(utf==2)input[n++]=0;input[n++]=c;if(utf==1)input[n++]=0;}
  }
  for(line=0;line<=total;line++){
   int lo=0,hi=-1,pass;
   for(pass=0;pass<2;pass++){
    if(pass==0){lo=hi=line;if(!line || (move && line>=first && line<=last))continue;}
    else{if(line!=dest)continue;lo=first;hi=last;}
    for(j=lo;j<=hi;j++){
     BYTE c=(BYTE)('a'+j-1);if(utf==2)want[wantn++]=0;want[wantn++]=c;if(utf==1)want[wantn++]=0;
     if(eol){if(utf==2)want[wantn++]=0;want[wantn++]='\r';if(utf==1)want[wantn++]=0;}
     if(utf==2)want[wantn++]=0;want[wantn++]='\n';if(utf==1)want[wantn++]=0;
    }
   }
  }
  Document(input,n,utf?16:0,utf!=2);DefaultNewLine=eol?C_CRLF:'\n';transferModel=1;
  CHECK(WvExTransfer(first,last,dest,move));CHECK(FirstPage->Fill==wantn && !memcmp(FirstPage->PageBuf,want,wantn));
  CHECK(undoCalls==!(move && dest==first-1));CHECK(!YankHandles[0] && !YankLength[0]);
 }
}
int main(void){
 INT reg;WV_ULONG n;BOOL have;POSITION begin,end;BYTE wide[]={0xff,0xfe,'a',0,'\n',0,'b',0},big[]={0xfe,0xff,0,'a',0,'\n',0,'b'};
 extern BOOL SuppressStatusBeep;SuppressStatusBeep=TRUE;
 CHECK(LineArgs(1,L"A 17",&reg,&n,&have) && reg=='A' && have && n==17);
 CHECK(LineArgs(1,L"\"1 2",&reg,&n,&have) && reg=='1' && n==2);
 CHECK(LineArgs(2,L"a",&reg,&n,&have) && reg=='a' && !have);
 CHECK(!LineArgs(2,L"aa",&reg,&n,&have));CHECK(!LineArgs(2,L"1 2",&reg,&n,&have));
 CHECK(!LineArgs(0,L"0",&reg,&n,&have));CHECK(!LineArgs(1,L"9223372036854775808",&reg,&n,&have));
 CHECK(!LineArgs(0,L"2|write",&reg,&n,&have));CHECK(!LineArgs(1,L"a 2 garbage",&reg,&n,&have));
 Document((BYTE*)"a\nb",3,0,TRUE);CHECK(LineSpan(2,2,&begin,&end) && CountBytes(&begin)==2 && CountBytes(&end)==3);
 CHECK(WvExLineCommand(2,2,1,L"a",FALSE));CHECK(YankLength[10]==2);
 CHECK(WvExLineCommand(1,1,1,L"A",FALSE));CHECK(YankLength[10]==4);
 CHECK(WvExLineCommand(0,(WV_ULONG)-1,2,L"a",FALSE));CHECK(outputLength==4 && !memcmp(output,"b\na\n",4));
 CHECK(undoCalls==1);FreeRegisters();
 Document((BYTE*)"a\nb",3,0,TRUE);DblQuoteChar='a';begin=CurrPos;CHECK(Yank(&begin,1,0));DblQuoteChar=0;
 CHECK(WvExLineCommand(2,2,1,L"A",FALSE));CHECK(WvExLineCommand(0,(WV_ULONG)-1,2,L"a",FALSE));CHECK(outputLength==4 && !memcmp(output,"a\nb\n",4));FreeRegisters();
 Document(wide,sizeof(wide),16,TRUE);CHECK(WvExLineCommand(2,2,1,L"",FALSE));CHECK(YankLength[0]==4);
 CHECK(WvExLineCommand(2,(WV_ULONG)-1,2,L"",FALSE));CHECK(outputLength==6 && !memcmp(output,"\n\0b\0\n\0",6));FreeRegisters();
 Document(big,sizeof(big),16,FALSE);CHECK(WvExLineCommand(2,2,1,L"",FALSE));CHECK(WvExLineCommand(0,(WV_ULONG)-1,2,L"",FALSE));CHECK(outputLength==4 && !memcmp(output,"\0b\0\n",4));FreeRegisters();
 Document((BYTE*)"a\n",2,0,TRUE);CHECK(!WvExLineCommand(0,0,1,L"",FALSE));CHECK(!WvExLineCommand(2,2,1,L"",FALSE));
 CHECK(!WvExLineCommand(0,(WV_ULONG)-1,2,L"",FALSE));CHECK(!WvExLineCommand(1,1,2,L"",FALSE));
 readOnly=TRUE;CHECK(WvExLineCommand(1,1,1,L"",FALSE));CHECK(!WvExLineCommand(1,(WV_ULONG)-1,2,L"",FALSE));FreeRegisters();
 Document((BYTE*)"",0,0,TRUE);CHECK(!WvExLineCommand((WV_ULONG)-1,(WV_ULONG)-1,1,L"",FALSE));
 Document((BYTE*)"a\nb\n",4,0,TRUE);CHECK(!WvExLineCommand(1,2,0,L"",FALSE) && reserves==1);
 {BYTE data[512];memset(data,'a',511);data[511]='\n';Document(data,512,0,TRUE);cancel=1;dialogsFrozen=0;
 hwndSearch=CreateWindowW(L"STATIC",L"search probe",0,0,0,10,10,NULL,NULL,GetModuleHandle(NULL),NULL);
 hwndSettings=CreateWindowW(L"STATIC",L"settings probe",0,0,0,10,10,NULL,NULL,GetModuleHandle(NULL),NULL);
 CHECK(hwndSearch && hwndSettings);CHECK(!WvExLineCommand(1,1,1,L"",FALSE));
 CHECK(dialogsFrozen && IsWindowEnabled(hwndSearch) && IsWindowEnabled(hwndSettings));
 DestroyWindow(hwndSearch);DestroyWindow(hwndSettings);hwndSearch=hwndSettings=NULL;}
 Document((BYTE*)"a\nb\n",4,0,TRUE);readFault=1;CHECK(!WvExLineCommand(1,2,0,L"",FALSE) && !reserves);
 Document((BYTE*)"a\nb\n",4,0,TRUE);readFault=2;CHECK(!WvExLineCommand(1,1,1,L"",FALSE));
 Document((BYTE*)"a\n",2,0,TRUE);CHECK(WvExLineCommand(1,1,1,L"",FALSE));readFault=3;
 CHECK(!WvExLineCommand(1,(WV_ULONG)-1,2,L"",FALSE) && !outputLength);
 Document((BYTE*)"a\n",2,0,TRUE);readFault=4;CHECK(!WvExLineCommand(0,(WV_ULONG)-1,2,L"",FALSE) && outputLength>0);
 FreeRegisters();
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);
 CHECK(!WvExTransfer(0,1,2,FALSE));CHECK(!WvExTransfer(2,1,0,FALSE));CHECK(!WvExTransfer(1,4,0,FALSE));CHECK(!WvExTransfer(1,1,4,FALSE));
 CHECK(!WvExTransfer(1,2,1,TRUE));CHECK(!WvExTransfer(1,2,2,TRUE));CHECK(!undoCalls && !outputLength && !deleteCalls);
 CHECK(WvExTransfer(2,3,1,TRUE) && !undoCalls && !outputLength);
 readOnly=1;CHECK(!WvExTransfer(1,1,0,FALSE));readOnly=0;HexEditMode=TRUE;CHECK(!WvExTransfer(1,1,0,FALSE));HexEditMode=FALSE;
 failSnapshot=1;CHECK(!WvExTransfer(1,1,3,TRUE) && !undoCalls && !deleteCalls);failSnapshot=0;
 cancel=1;CHECK(!WvExTransfer(1,1,3,TRUE) && !undoCalls && !deleteCalls);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);readFault=1;CHECK(!WvExTransfer(1,1,3,TRUE) && !undoCalls && !deleteCalls);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);readFault=2;CHECK(!WvExTransfer(1,1,3,TRUE) && !undoCalls && !deleteCalls);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);readFault=3;CHECK(!WvExTransfer(1,1,3,TRUE) && !undoCalls && !deleteCalls);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);failInsert=1;CHECK(!WvExTransfer(1,1,3,TRUE) && !deleteCalls && FirstPage->Fill==6);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);CHECK(!WvExTransfer(1,1,3,TRUE) && !deleteCalls && outputLength==2 && FirstPage->Fill==6);
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);transferModel=failDeleteUndo=1;
 CHECK(!WvExTransfer(1,1,3,TRUE) && !deleteCalls && FirstPage->Fill==8 && !memcmp(FirstPage->PageBuf,"a\nb\nc\na\n",8));
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);transferModel=1;
 CHECK(WvExTransfer(1,1,3,TRUE) && deleteCalls==1 && undoCalls==1 && !memcmp(FirstPage->PageBuf,"b\nc\na\n",6));
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);transferModel=1;
 CHECK(WvExTransfer(2,3,0,TRUE) && deleteCalls==1 && !memcmp(FirstPage->PageBuf,"b\nc\na\n",6));
 Document((BYTE*)"a\nb\nc\n",6,0,TRUE);transferModel=1;
 CHECK(WvExTransfer(1,2,1,FALSE) && !deleteCalls && FirstPage->Fill==10 && !memcmp(FirstPage->PageBuf,"a\na\nb\nb\nc\n",10));
 CHECK(!YankHandles[0] && !YankLength[0]);
 {static const WCHAR *bad[]={L":1move",L":1copy",L":1t",L":1m1",L":1,2m2",L":0t1",L":3,1t0",L":1t999",L":1t-9",L":1t9223372036854775808",L":1t1 garbage",L":1t1,2",L":1t1|write",L":1move! 2",L":1copy! 2"};int i;
  for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++){WCHAR cmd[128];wcscpy(cmd,bad[i]);Document((BYTE*)"a\nb\nc\n",6,0,TRUE);transferModel=1;CHECK(!CommandExec(cmd));CHECK(!undoCalls && !deleteCalls && FirstPage->Fill==6);}
 }
 Transfers();
 {extern POSITION MarkPos[27];int first,last,right,ts,sw;char input[]="a\n  b\n\tc\n";
  for(ts=1;ts<=8;ts*=2)for(sw=1;sw<=4;sw++)for(first=1;first<=3;first++)for(last=first;last<=3;last++)for(right=0;right<2;right++){
   BYTE want[100];int n=0,line,col,tabs,k;Document((BYTE*)input,sizeof(input)-1,0,TRUE);transferModel=1;TabStop=ts;ShiftWidth=sw;
   for(line=1;line<=3;line++){
    col=line==1?0:line==2?2:ts;
    if(line>=first && line<=last){col=right?col+sw:col>sw?col-sw:0;tabs=ts>1?col/ts:0;for(k=0;k<tabs;k++)want[n++]='\t';for(k=0;k<(ts>1?col%ts:col);k++)want[n++]=' ';}
    else if(line==2){want[n++]=' ';want[n++]=' ';}else if(line==3)want[n++]='\t';
    want[n++]=(BYTE)('a'+line-1);want[n++]='\n';
   }
   CHECK(WvExIndent(first,last,L"",right));CHECK(FirstPage->Fill==n && !memcmp(FirstPage->PageBuf,want,n));CHECK(CountLines(&CurrPos)+1==last && !WvExGlobalActive);
  }
  TabStop=8;ShiftWidth=4;
  Document((BYTE*)" a\nb\n",5,0,TRUE);transferModel=1;CHECK(WvExIndent(1,1,L"> 2",TRUE));CHECK(FirstPage->Fill==7 && !memcmp(FirstPage->PageBuf,"\t a\n\tb\n",7) && undoCalls==1);
  Document((BYTE*)" \t\na\n",5,0,TRUE);transferModel=1;CHECK(WvExIndent(1,1,L"",TRUE));CHECK(FirstPage->Fill==3 && !memcmp(FirstPage->PageBuf,"\na\n",3));
  Document((BYTE*)"  a\nb\n",6,0,TRUE);failSnapshot=1;CHECK(!WvExIndent(1,1,L"",TRUE) && !undoCalls && !deleteCalls && FirstPage->Fill==6);
  Document((BYTE*)"  a\nb\n",6,0,TRUE);failInsert=1;CHECK(!WvExIndent(1,1,L"",TRUE) && !deleteCalls && FirstPage->Fill==6);
  Document((BYTE*)"  a\nb\n",6,0,TRUE);transferModel=failDeleteUndo=1;CHECK(!WvExIndent(1,1,L"",TRUE) && !deleteCalls && FirstPage->Fill==12 && !memcmp(FirstPage->PageBuf,"        a\nb\n",12));
  Document((BYTE*)"  a\nb\n",6,0,TRUE);cancel=1;CHECK(!WvExIndent(1,1,L"",TRUE) && !undoCalls && !WvExGlobalActive && FirstPage->Fill==6);
  Document((BYTE*)"  a\nb\n",6,0,TRUE);readFault=2;CHECK(!WvExIndent(1,1,L"",TRUE) && !undoCalls && !WvExGlobalActive);
  Document((BYTE*)"a\nb\n",4,0,TRUE);readOnly=1;CHECK(!WvExIndent(1,1,L"",TRUE));readOnly=0;
  memset(MarkPos,0,sizeof(POSITION)*27);CHECK(WvExMark(2,L"a") && CountBytes(&MarkPos[1])==2 && !CurrPos.i && !undoCalls);
  CHECK(!WvExMark(1,L"") && CountBytes(&MarkPos[1])==2);CHECK(!WvExMark(1,L"A"));CHECK(!WvExMark(1,L"aa"));CHECK(!WvExMark(0,L"a"));CHECK(!WvExMark(3,L"a"));
  cancel=1;CHECK(!WvExMark(1,L"a") && CountBytes(&MarkPos[1])==2 && !WvExGlobalActive);cancel=0;
  readFault=2;CHECK(!WvExMark(1,L"a") && CountBytes(&MarkPos[1])==2 && !WvExGlobalActive);readFault=0;
  CHECK(CommandExec(L":1ka") && CountBytes(&MarkPos[1])==0);CHECK(CommandExec(L":2ma z") && CountBytes(&MarkPos[26])==2);
  {static const WCHAR *bad[]={L":>0",L":< -1",L":>>garbage",L":>1|write",L":>9223372036854775808",L":0>",L":3,1<",L":mark",L":ka extra",L":1,2ka",L":mark! a",L":k A"};int i;for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++){WCHAR cmd[100];Document((BYTE*)"a\nb\n",4,0,TRUE);wcscpy(cmd,bad[i]);CHECK(!CommandExec(cmd));CHECK(!undoCalls && !deleteCalls && FirstPage->Fill==4);}}
 }
 {BYTE le[]={0xe9,0,13,0,0x2d,0x4e,10,0,10,0,'x',0,10,0};BYTE be[sizeof(le)];DWORD flags;WV_LONG counts[9];int i,order;
  for(i=0;i<sizeof(le);i+=2){be[i]=le[i+1];be[i+1]=le[i];}
  for(order=0;order<2;order++){
   BYTE *bytes=order?be:le;Document(bytes,sizeof(le),16,!order);CHECK(LinesInFile==4);
   memset(counts,0,sizeof(counts));flags=0;CHECK(CountNewlinesInBuf(bytes,sizeof(le),counts,&flags)==4 && counts[0]==0 && counts[1]==1 && counts[2]==3);
   flags=CNLB_LASTWASCR;CHECK(CountNewlinesInBuf(bytes+4,4,NULL,&flags)==1 && !(flags&CNLB_LASTWASCR));
   transferModel=1;CHECK(WvExTransfer(2,2,4,TRUE) && FirstPage->Fill==16);
  }
 }
 printf("PASS %d ex line parser/register/error checks\n",checks);return 0;
}
