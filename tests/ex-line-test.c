/* Parser/register probes. Document edit bytes are checked by ex-line.ps1. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "../ExLine.c"
#undef CharAt
#undef CharAndAdvance
#undef GoBackAndChar
INT CharAt(PPOSITION);INT CharAndAdvance(PPOSITION);INT GoBackAndChar(PPOSITION);
static int checks,readOnly,outputLength,undoCalls,reserves,cancel,dialogsFrozen,readFault;
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
BOOL ProbeInsert(LPBYTE bytes,WV_ULONG length,WORD flags){(void)flags;if(length>sizeof(output)-outputLength)return FALSE;memcpy(output+outputLength,bytes,(size_t)length);outputLength+=(int)length;return TRUE;}
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
 FreeRegisters();printf("PASS %d ex line parser/register/error checks\n",checks);return 0;
}
