#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static int checks,allocations,failAt,outputFail,running;
static HWND focus=(HWND)1;
static WCHAR *listing;
static void *Allocate(size_t n){return ++allocations==failAt?NULL:malloc(n);}
static void *ZeroAllocate(size_t n,size_t size){return ++allocations==failAt?NULL:calloc(n,size);}
static HWND WINAPI ProbeGetFocus(void){return focus;}
static SHORT WINAPI KeyStateForTest(int key){(void)key;return 0;}
static BOOL WINAPI KeyboardStateForTest(PBYTE state){memset(state,0,256);return TRUE;}
static BOOL WINAPI SetKeyboardForTest(LPBYTE state){(void)state;return TRUE;}
#define malloc Allocate
#define calloc ZeroAllocate
#define GetFocus ProbeGetFocus
#define GetKeyState KeyStateForTest
#define GetKeyboardState KeyboardStateForTest
#define SetKeyboardState SetKeyboardForTest
#define AllocStringA ProbeString
#define NewStatus ProbeNotice
#define WvExOutput ProbeOutput
#define ExecRunning ProbeRunning
#include "../Map.c"
#include "../ExMap.c"
#undef malloc
#undef calloc
void ProbeNotice(INT field,PWSTR text,INT type){(void)field;(void)text;(void)type;}
BOOL ProbeRunning(void){return running;}
BOOL ProbeString(LPSTR *dst,LPCSTR text){size_t n=strlen(text)+1;LPSTR p=Allocate(n);if(!p)return FALSE;memcpy(p,text,n);free(*dst);*dst=p;return TRUE;}
BOOL ProbeOutput(PWSTR text){size_t n=(wcslen(text)+1)*sizeof(WCHAR);free(listing);listing=malloc(n);memcpy(listing,text,n);return !outputFail;}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void Reset(void){
 failAt=0;FreeMappings(FirstMapping);FirstMapping=NULL;allocations=outputFail=running=0;
 hwndMain=(HWND)1;hwndCmd=hwndSettings=NULL;focus=hwndMain;Mode=CommandMode;WvExGlobalActive=FALSE;StopMap(TRUE);
}
static int Feed(WPARAM c,WCHAR *output){
 MSG msg={0};int n=0;msg.hwnd=focus;msg.message=WM_CHAR;msg.wParam=c;
 if(!IsKeyboardMapMsg(&msg)){output[0]=(WCHAR)c;return 1;}
 while(KeyboardMapDequeue(&msg)){CHECK(n<200);if(msg.message==WM_CHAR)output[n++]=(WCHAR)msg.wParam;}
 return n;
}
int main(void){
 WCHAR out[256];DWORD tokens[2],*decoded=NULL;LPSTR text;int n,i;
 Reset();CHECK(WvExMap(L"Q x",FALSE,FALSE));CHECK(FirstMapping->Flags==M_COMMANDMODE);
 CHECK(Feed('Q',out)==1 && out[0]=='x');CHECK(WvExMap(L"Q y",FALSE,FALSE));CHECK(!FirstMapping->Next);CHECK(Feed('Q',out)==1 && out[0]=='y');
 CHECK(WvExMap(L"Q",TRUE,FALSE));CHECK(!FirstMapping);CHECK(Feed('Q',out)==1 && out[0]=='Q');
 Reset();CHECK(WvExMap(L"jj <Esc>",FALSE,TRUE));Mode=InsertMode;
 CHECK(Feed('j',out)==0);CHECK(Feed('j',out)==1 && out[0]==27);
 Mode=CommandMode;CHECK(Feed('j',out)==1 && out[0]=='j');
 hwndCmd=(HWND)2;focus=hwndCmd;CHECK(Feed('j',out)==0);CHECK(Feed('j',out)==1 && out[0]==27);
 Reset();CHECK(WvExMap(L"- x",FALSE,FALSE));CHECK(Feed(0x4e2d,out)==1 && out[0]==0x4e2d);
 CHECK(WvExMap(L"\x4e2d \xd83d\xde00",FALSE,FALSE));CHECK(Feed(0x4e2d,out)==2 && out[0]==0xd83d && out[1]==0xde00);
 CHECK(WvExMap(L"\xd83d\xde00 Z",FALSE,FALSE));CHECK(Feed(0xd83d,out)==0);CHECK(Feed(0xde00,out)==1 && out[0]=='Z');
 Reset();CHECK(WvExMap(L"zz x",FALSE,FALSE));CHECK(Feed('z',out)==0);CHECK(Feed('a',out)==2 && out[0]=='z' && out[1]=='a');
 CHECK(WvExMap(L"Q <Nop>",FALSE,FALSE));CHECK(Feed('Q',out)==0);
 Reset();CHECK(WvExMap(L"Q abc",FALSE,FALSE));FirstMapping->Flags|=M_INSERTMODE|M_COMMANDLINE;
 CHECK(WvExMap(L"Q x",FALSE,FALSE));CHECK(FirstMapping->Next->Flags==(M_INSERTMODE|M_COMMANDLINE));
 Mode=InsertMode;CHECK(Feed('Q',out)==3 && out[0]=='a');Mode=CommandMode;CHECK(Feed('Q',out)==1 && out[0]=='x');
 CHECK(WvExMap(L"Q",TRUE,FALSE));CHECK(FirstMapping->Flags==(M_INSERTMODE|M_COMMANDLINE));CHECK(WvExMap(L"Q",TRUE,TRUE));CHECK(!FirstMapping);
 Reset();CHECK(WvExMap(L"Q xyz",FALSE,FALSE));
 {MSG msg={0};msg.message=WM_CHAR;msg.wParam='Q';msg.hwnd=focus;CHECK(IsKeyboardMapMsg(&msg));CHECK(SequenceLen==3);CHECK(WvExMap(L"Q",TRUE,FALSE));CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='x');CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='y');CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='z');CHECK(!KeyboardMapDequeue(&msg));}
 /* Physical read-ahead must not replace a macro already being emitted. */
 Reset();CHECK(WvExMap(L"Q abc",FALSE,FALSE));CHECK(WvExMap(L"x Z",FALSE,TRUE));
 {MSG msg={0};msg.message=WM_CHAR;msg.wParam='Q';msg.hwnd=focus;CHECK(IsKeyboardMapMsg(&msg));
  msg.wParam='x';CHECK(IsKeyboardMapMsg(&msg));
  CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='a');Mode=InsertMode;
  CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='b');CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='c');
  CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='Z');CHECK(!KeyboardMapDequeue(&msg));}
 /* A failed replay snapshot leaves both the table and active expansion intact. */
 Reset();CHECK(WvExMap(L"Q old",FALSE,FALSE));
 {MSG msg={0};PMAP old=FirstMapping;msg.message=WM_CHAR;msg.wParam='Q';msg.hwnd=focus;CHECK(IsKeyboardMapMsg(&msg));
  allocations=0;failAt=7;CHECK(!WvExMap(L"Q new",FALSE,FALSE));CHECK(FirstMapping==old);failAt=0;
  CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='o');CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='l');
  CHECK(KeyboardMapDequeue(&msg) && msg.wParam=='d');CHECK(!KeyboardMapDequeue(&msg));}
 for(i=1;i<=6;i++){
  PMAP old;Reset();CHECK(WvExMap(L"Q old",FALSE,FALSE));old=FirstMapping;allocations=0;failAt=i;
  CHECK(!WvExMap(L"Q replacement",FALSE,FALSE));CHECK(FirstMapping==old && !old->Next);failAt=0;CHECK(Feed('Q',out)==3 && out[0]=='o');
 }
 {WCHAR *bad[]={L"",L"Q extra",L"missing"};Reset();CHECK(!WvExMap(bad[0],TRUE,FALSE));CHECK(!WvExMap(bad[1],TRUE,FALSE));CHECK(!WvExMap(bad[2],TRUE,FALSE));}
 Reset();CHECK(!WvExMap(L"\xd800 x",FALSE,FALSE));CHECK(!WvExMap(L"Q \xdc00",FALSE,FALSE));CHECK(!WvExMap(L"Q \x16",FALSE,FALSE));
 Reset();CHECK(WvExMapping(L"btw by-the-way",FALSE,TRUE,TRUE));Mode=InsertMode;
 CHECK(Feed('b',out)==1 && out[0]=='b');CHECK(Feed('t',out)==1 && out[0]=='t');CHECK(Feed('w',out)==1 && out[0]=='w');
 CHECK(Feed(' ',out)==14 && out[0]==8 && out[1]==8 && out[2]==8 && out[3]=='b' && out[13]==' ');
 Reset();CHECK(WvExMapping(L"\xd83d\xde00 X",FALSE,TRUE,TRUE));Mode=InsertMode;
 CHECK(Feed(0xd83d,out)==1);CHECK(Feed(0xde00,out)==1);CHECK(Feed(' ',out)==3 && out[0]==8 && out[1]=='X' && out[2]==' ');
 Reset();CHECK(WvExMapping(L"btw X",FALSE,TRUE,TRUE));Mode=InsertMode;
 CHECK(Feed(0x4e2d,out)==1);CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);CHECK(Feed('w',out)==1);CHECK(Feed(' ',out)==1 && out[0]==' ');
 Reset();CHECK(WvExMapping(L"btw X",FALSE,TRUE,TRUE));Mode=InsertMode;
 CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);CHECK(Feed('w',out)==1);
 MapInterrupted=FALSE;CHECK(Feed(3,out)==1 && out[0]==3);CHECK(MapInterrupted);CHECK(!AbbrevDeletes);
 CHECK(!TranslateMapping(&decoded,"U+D800"));CHECK(!TranslateMapping(&decoded,"U+DC00"));
 CHECK(TranslateMapping(&decoded,"U+D83D U+DE00"));CHECK(WvMapLength(decoded)==2);free(decoded);decoded=NULL;
 {WCHAR longInput[70];for(i=0;i<64;i++)longInput[i]='x';wcscpy(longInput+64,L" y");CHECK(!WvExMap(longInput,FALSE,FALSE));}
 Reset();CHECK(WvExMap(L"Q <Esc>",FALSE,FALSE));CHECK(WvExMap(L"",FALSE,FALSE));CHECK(wcsstr(listing,L"Q") && wcsstr(listing,L"Ctrl+"));
 outputFail=1;CHECK(!WvExMap(L"Q",FALSE,FALSE));CHECK(FirstMapping!=NULL);outputFail=0;
 WvExGlobalActive=TRUE;CHECK(!WvExMap(L"Q x",FALSE,FALSE));WvExGlobalActive=FALSE;running=1;CHECK(!WvExMap(L"Q",TRUE,FALSE));running=0;
 Reset();CHECK(WvExAbbreviate(L"btw X",FALSE));CHECK(Feed('i',out)==1);Mode=InsertMode;
 CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);CHECK(Feed('w',out)==1);CHECK(Feed(' ',out)==5 && out[3]=='X' && out[4]==' ');
 Reset();CHECK(WvExAbbreviate(L"abc LONG",FALSE));CHECK(WvExAbbreviate(L"ab SHORT",FALSE));Mode=InsertMode;
 CHECK(Feed('a',out)==1);CHECK(Feed('b',out)==1);CHECK(Feed('c',out)==1);CHECK(Feed(' ',out)==8 && out[3]=='L' && out[7]==' ');
 Reset();CHECK(WvExAbbreviate(L"btw X",FALSE));Mode=InsertMode;
 CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);CHECK(Feed('w',out)==1);
 {MSG msg={0};WCHAR *keys=L" btw ";n=0;msg.hwnd=focus;msg.message=WM_CHAR;
  for(i=0;keys[i];i++){msg.message=WM_CHAR;msg.wParam=keys[i];CHECK(IsKeyboardMapMsg(&msg));}
  while(KeyboardMapDequeue(&msg)){CHECK(n<200);if(msg.message==WM_CHAR)out[n++]=(WCHAR)msg.wParam;}
  CHECK(n==13 && out[3]=='X' && out[4]==' ' && out[5]=='b' && out[11]=='X' && out[12]==' ');}
 Reset();CHECK(WvExAbbreviate(L"btw by the way",FALSE));CHECK(FirstMapping->Flags==(M_ABBREVIATION|M_INSERTMODE|M_COMMANDLINE));
 Mode=InsertMode;CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);CHECK(Feed('w',out)==1);
 CHECK(Feed(' ',out)==14 && out[0]==8 && out[3]=='b' && out[13]==' ');
 CHECK(WvExAbbreviate(L"btw X",FALSE));CHECK(!FirstMapping->Next);CHECK(WvExAbbreviate(L"btw",TRUE));CHECK(!FirstMapping);
 Reset();CHECK(WvExMap(L"btw Z",FALSE,TRUE));CHECK(WvExAbbreviate(L"btw X",FALSE));CHECK(FirstMapping->Next!=NULL);
 CHECK(WvExAbbreviate(L"btw",TRUE));CHECK(FirstMapping->Flags==(M_INSERTMODE|M_COMMANDLINE));CHECK(!FirstMapping->Next);
 Reset();CHECK(!WvExAbbreviate(L"",TRUE));CHECK(!WvExAbbreviate(L"unknown",TRUE));CHECK(!WvExAbbreviate(L"btw extra",TRUE));
 CHECK(WvExAbbreviate(L"btw X",FALSE));CHECK(WvExAbbreviate(L"",FALSE));CHECK(wcsstr(listing,L"b"));
 /* Queued physical input grows beyond the old 63-message queue without loss. */
 Reset();CHECK(WvExMap(L"Q X",FALSE,FALSE));
 {MSG msg={0};msg.hwnd=focus;msg.message=WM_CHAR;msg.wParam='Q';CHECK(IsKeyboardMapMsg(&msg));
  for(i=0;i<1000;i++){msg.message=WM_CHAR;msg.wParam='a';CHECK(IsKeyboardMapMsg(&msg));}
  n=0;while(KeyboardMapDequeue(&msg)){CHECK(msg.message==WM_CHAR && msg.wParam==(n?'a':'X'));n++;}CHECK(n==1001);}
 Reset();CHECK(WvExMap(L"Q X",FALSE,FALSE));
 {MSG msg={0};msg.hwnd=focus;msg.message=WM_CHAR;msg.wParam='Q';CHECK(IsKeyboardMapMsg(&msg));
  allocations=0;failAt=1;msg.wParam='a';CHECK(!IsKeyboardMapMsg(&msg));failAt=0;CHECK(!SequenceLen && !DeferredCount);CHECK(FirstMapping!=NULL);}
 Reset();CHECK(WvExAbbreviate(L"#1 X",FALSE));CHECK(FirstMapping->InputMatch[0]==WvMapChar(L'#'));
 Reset();{WCHAR definition[70];for(i=0;i<63;i++)definition[i]='a';wcscpy(definition+63,L" X");CHECK(WvExAbbreviate(definition,FALSE));Mode=InsertMode;
  for(i=0;i<64;i++)CHECK(Feed('a',out)==1 && out[0]=='a');CHECK(Feed(' ',out)==1 && out[0]==' ');}
 Reset();CHECK(WvExAbbreviate(L"btw X",FALSE));Mode=InsertMode;
 CHECK(Feed('b',out)==1);CHECK(Feed('t',out)==1);hwndCmd=(HWND)2;focus=hwndCmd;
 CHECK(Feed('w',out)==1);CHECK(Feed(' ',out)==1 && out[0]==' ');
 /* Every non-surrogate UTF-16 value survives Settings/profile text roundtrip. */
 for(i=0;i<=65535;i++)if(i<0xd800 || i>0xdfff){
  tokens[0]=WvMapChar((WCHAR)i);tokens[1]=0;text=NativeKeys(tokens);CHECK(text!=NULL);CHECK(TranslateMapping(&decoded,text));CHECK(IsMappingEqual(tokens,decoded));free(text);free(decoded);decoded=NULL;
 }
 {char *large=malloc(10004);large[0]='"';memset(large+1,'a',10000);large[10001]='"';large[10002]=0;CHECK(TranslateMapping(&decoded,large));CHECK(WvMapLength(decoded)==10000);free(decoded);free(large);}
 Reset();CHECK(WvExMap(L"#24 <F24>",FALSE,FALSE));CHECK(FirstMapping->InputMatch[0]==(WV_MAP_KEYDOWN|VK_F24));
 {MSG msg={0};msg.message=WM_KEYDOWN;msg.wParam=VK_F24;msg.hwnd=focus;CHECK(IsKeyboardMapMsg(&msg));CHECK(KeyboardMapDequeue(&msg));CHECK(msg.message==WM_KEYDOWN && msg.wParam==VK_F24);}
 Reset();free(listing);printf("PASS %d ex map/Unicode/engine checks\n",checks);return 0;
}
