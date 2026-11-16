/* Quiet native display/parser/failure probes; output windows are mocked. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static WCHAR captured[16384];static int checks,failAlloc,allocCalls,failWindow,cancel,readFault,running,shown;
static void *ProbeAlloc(size_t n){return failAlloc && ++allocCalls==failAlloc?NULL:malloc(n);}
static void *ProbeRealloc(void *p,size_t n){return failAlloc && ++allocCalls==failAlloc?NULL:realloc(p,n);}
static ATOM WINAPI ProbeClass(const WNDCLASSW *c){(void)c;if(failWindow==2){SetLastError(ERROR_ACCESS_DENIED);return 0;}return 1;}
static BOOL WINAPI ProbeShow(HWND h,int n){(void)h;(void)n;return TRUE;}
static HWND WINAPI ProbeCreate(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE inst,LPVOID text){
 (void)ex;(void)cls;(void)title;(void)style;(void)x;(void)y;(void)w;(void)h;(void)parent;(void)menu;(void)inst;
 if(failWindow)return NULL;wcsncpy(captured,(LPCWSTR)text,16383);captured[16383]=0;shown++;return (HWND)1;
}
#define malloc ProbeAlloc
#define realloc ProbeRealloc
#define RegisterClassW ProbeClass
#define CreateWindowExW ProbeCreate
#define ShowWindow ProbeShow
#define NewStatus ProbeNotice
#define Disable ProbeDisable
#define Enable ProbeEnable
#define NewPosition ProbePosition
#define FindValidPosition ProbeValid
#define ShowEditCaret ProbeCaret
#define Jump ProbeJump
#define CheckInterrupt ProbeInterrupt
#define StartInterruptCheck ProbeStart
#define ExecRunning ProbeExec
#define CharAndAdvance ProbeAdvance
#include "../ExGlobal.c"
#undef malloc
#undef realloc
#undef CharAndAdvance
INT CharAndAdvance(PPOSITION);
void ProbeNotice(INT f,PWSTR t,INT s){(void)f;(void)t;(void)s;}
void ProbeDisable(WORD f){(void)f;Disabled=TRUE;}
void ProbeEnable(void){Disabled=FALSE;}
WV_LONG ProbePosition(PPOSITION p){CurrPos=*p;return 0;}
void ProbeValid(PPOSITION p,WORD f){(void)p;(void)f;}
void ProbeCaret(void){}void ProbeJump(PPOSITION p){(void)p;}
BOOL ProbeInterrupt(void){if(cancel)Interrupted=TRUE;return Interrupted;}
void ProbeStart(void){Interrupted=FALSE;}
BOOL ProbeExec(void){return running;}
INT ProbeAdvance(PPOSITION p){return readFault?C_ERROR:CharAndAdvance(p);}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void Document(const BYTE *bytes,int n,int utf,BOOL little){
 static PAGE page;static BYTE data[4096];memset(&page,0,sizeof(page));memcpy(data,bytes,n);page.PageBuf=data;page.Fill=n;page.PageNo=1;
 FirstPage=LastPage=&page;CurrPos.p=ScreenStart.p=&page;CurrPos.i=CurrPos.f=ScreenStart.i=ScreenStart.f=0;
 UtfEncoding=utf;UtfLsbFirst=little;CharSet=utf==16?(little?CS_UTF16LE:CS_UTF16BE):CS_UTF8;
 LinesInFile=page.Newlines=CountNewlinesInBuf(data,n,NULL,NULL);HexEditMode=ViewOnlyFlag=ReadOnlyFlag=ReadOnlyFile=FALSE;
 WvExGlobalActive=Interrupted=Disabled=failAlloc=allocCalls=failWindow=cancel=readFault=running=shown=0;captured[0]=0;
}
int main(void){
 extern BOOL SuppressStatusBeep;INT first,last,kind;SuppressStatusBeep=TRUE;
 for(first=1;first<=3;first++)for(last=first;last<=3;last++)for(kind=1;kind<=3;kind++){
  WCHAR want[100]=L"",part[30];int line;Document((BYTE*)"a\nb\nc",5,8,TRUE);
  for(line=first;line<=last;line++){if(kind==2)swprintf(part,30,L"%d\t%c\r\n",line,L'a'+line-1);else swprintf(part,30,L"%c%s\r\n",L'a'+line-1,kind==3?L"$":L"");wcscat(want,part);}
  CHECK(WvExDisplay(first,last,L"",kind));CHECK(!wcscmp(captured,want));CHECK(CountLines(&CurrPos)+1==last);CHECK(!Disabled && !WvExGlobalActive && FirstPage->Fill==5);
 }
 Document((BYTE*)"\t\x01\x7f\\\n\n",6,8,TRUE);CHECK(WvExDisplay(0,(WV_ULONG)-2,L"",3));CHECK(!wcscmp(captured,L"^I^A^?\\$\r\n$\r\n"));
 Document((BYTE*)"a\nb\nc",5,8,TRUE);CHECK(WvExDisplay(1,2,L"2",2));CHECK(!wcscmp(captured,L"2\tb\r\n3\tc\r\n"));
 CHECK(WvExDisplay(2,(WV_ULONG)-1,L"999",1));CHECK(!wcscmp(captured,L"b\r\nc\r\n"));
 Document((BYTE*)"a\nb\nc",5,8,TRUE);Advance(&CurrPos,2);CHECK(WvExDisplay((WV_ULONG)-1,(WV_ULONG)-1,L"",1));CHECK(!wcscmp(captured,L"b\r\n"));
 {const WCHAR *bad[]={L"0",L"-1",L"x",L"1 garbage",L"1|write",L"9223372036854775808"};int i;for(i=0;i<6;i++){Document((BYTE*)"a\n",2,8,TRUE);CHECK(!WvExDisplay(1,1,(PWSTR)bad[i],1));CHECK(!shown && !CurrPos.i);}}
 Document((BYTE*)"a\nb\n",4,8,TRUE);CHECK(!WvExDisplay(0,1,L"",1));CHECK(!WvExDisplay(2,1,L"",1));CHECK(!WvExDisplay(1,3,L"",1));
 failAlloc=1;CHECK(!WvExDisplay(1,2,L"",1));CHECK(!shown && !CurrPos.i && !Disabled && !WvExGlobalActive);failAlloc=0;
 cancel=1;CHECK(!WvExDisplay(1,2,L"",1));CHECK(!shown && !CurrPos.i && !Disabled && !WvExGlobalActive);cancel=0;
 readFault=1;CHECK(!WvExDisplay(1,2,L"",1));CHECK(!shown && !CurrPos.i && !Disabled && !WvExGlobalActive);readFault=0;
 failWindow=1;CHECK(!WvExDisplay(1,2,L"",1));CHECK(!shown && !CurrPos.i && !Disabled && !WvExGlobalActive);failWindow=0;
 failWindow=2;CHECK(!WvExDisplay(1,2,L"",1));CHECK(!shown && !CurrPos.i && !Disabled && !WvExGlobalActive);failWindow=0;
 running=1;CHECK(!WvExDisplay(1,2,L"",1));running=0;HexEditMode=TRUE;CHECK(!WvExDisplay(1,2,L"",1));HexEditMode=FALSE;
 ViewOnlyFlag=TRUE;CHECK(WvExDisplay(1,2,L"",1));ViewOnlyFlag=FALSE;
 {BYTE le[]={0xff,0xfe,0xe9,0,13,0,10,0,0x2d,0x4e,9,0},be[sizeof(le)];int i;for(i=0;i<sizeof(le);i+=2){be[i]=le[i+1];be[i+1]=le[i];}
  for(i=0;i<2;i++){Document(i?be:le,sizeof(le),16,!i);CHECK(WvExDisplay(0,(WV_ULONG)-2,L"",3));CHECK(!wcscmp(captured,L"\xe9$\r\n\x4e2d^I$\r\n"));}}
 {WCHAR *bad[]={L":print 0",L":list!",L":number x",L":1undo",L":undo 1",L":undo!"};int i;for(i=0;i<6;i++){WCHAR cmd[64];Document((BYTE*)"a\n",2,8,TRUE);wcscpy(cmd,bad[i]);CHECK(!CommandExec(cmd));CHECK(!shown && FirstPage->Fill==2);}}
 Document((BYTE*)"",0,8,TRUE);CHECK(!WvExDisplay((WV_ULONG)-1,(WV_ULONG)-1,L"",1));
 {BYTE utf8[]="\xef\xbb\xbf\xc3\xa9\n\xe4\xb8\xad\xf0\x9f\x98\x80\n";Document(utf8,sizeof(utf8)-1,8,TRUE);CHECK(WvExDisplay(0,(WV_ULONG)-2,L"",1));CHECK(!wcscmp(captured,L"\xe9\r\n\x4e2d\xd83d\xde00\r\n"));}
 {BYTE edges[2][4]={{0xf0,0x90,0x80,0x80},{0xf4,0x8f,0xbf,0xbf}};int i;for(i=0;i<2;i++){
  POSITION p;INT high=i?0xdbff:0xd800,low=i?0xdfff:0xdc00;Document(edges[i],4,8,TRUE);p=CurrPos;
  CHECK(CharAt(&p)==high);CHECK(CharAndAdvance(&p)==high && CountBytes(&p)==0);CHECK(CharAt(&p)==low);CHECK(CharAndAdvance(&p)==low && CountBytes(&p)==4);CHECK(CharAt(&p)==C_EOF);
 }}
 {extern INT AnsiCodePage,OemCodePage;BYTE sjis[]={0x93,0xfa,0x96,0x7b,10};
  Document((BYTE*)"\x80\n",2,0,TRUE);CharSet=CS_ANSI;AnsiCodePage=1252;CHECK(WvExDisplay(1,1,L"",1));CHECK(!wcscmp(captured,L"\x20ac\r\n"));
  Document((BYTE*)"\x82\n",2,0,TRUE);CharSet=CS_OEM;OemCodePage=437;CHECK(WvExDisplay(1,1,L"",1));CHECK(!wcscmp(captured,L"\xe9\r\n"));
  Document(sjis,sizeof(sjis),0,TRUE);CharSet=CS_ANSI;AnsiCodePage=932;CHECK(WvExDisplay(1,1,L"",1));CHECK(!wcscmp(captured,L"\x65e5\x672c\r\n"));
  Document((BYTE*)"a\0b\n",4,0,TRUE);CharSet=CS_OEM;CHECK(WvExDisplay(1,1,L"",1));CHECK(!wcscmp(captured,L"a\x2400" L"b\r\n"));
  CHECK(WvExDisplay(1,1,L"",3));CHECK(!wcscmp(captured,L"a^@b$\r\n"));AnsiCodePage=1252;
 }
 {BYTE bytes[1500];memset(bytes,'a',sizeof(bytes));Document(bytes,sizeof(bytes),8,TRUE);failAlloc=2;CHECK(!WvExDisplay(1,1,L"",1));CHECK(allocCalls==2 && !shown && !CurrPos.i && !Disabled && !WvExGlobalActive);}
 {WCHAR *text=NULL;size_t capacity=(size_t)INT_MAX/sizeof(WCHAR),length=capacity-1;CHECK(!AppendOutput(&text,&length,&capacity,L'x') && !text);}
 Document((BYTE*)"a\n",2,8,TRUE);CHECK(!WvExUndo());CHECK(!CurrPos.i && FirstPage->Fill==2);
 printf("PASS %d ex display/parser/failure checks\n",checks);return 0;
}
