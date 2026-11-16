#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static HGLOBAL clipboard;
static UINT format;
static BYTE inserted[16384];static size_t insertedSize;static unsigned checks;
static BOOL WINAPI ProbeOpen(HWND h){(void)h;return TRUE;}
static BOOL WINAPI ProbeClose(void){return TRUE;}
static BOOL WINAPI ProbeEmpty(void){if(clipboard)GlobalFree(clipboard);clipboard=NULL;return TRUE;}
static HANDLE WINAPI ProbeSet(UINT f,HANDLE h){format=f;clipboard=(HGLOBAL)h;return h;}
static HANDLE WINAPI ProbeGet(UINT f){return f==format?clipboard:NULL;}
#define OpenClipboard ProbeOpen
#define CloseClipboard ProbeClose
#define EmptyClipboard ProbeEmpty
#define SetClipboardData ProbeSet
#define GetClipboardData ProbeGet
#define CheckClipboard ProbeCheck
#define InsertBuffer ProbeInsert
#define EnterDeleteForUndo ProbeUndo
#define GetXPos ProbeX
#define HideEditCaret ProbeHide
#define ShowEditCaret ProbeShow
#define IsViewOnly ProbeView
#include "../Clipbrd.c"
void ProbeCheck(void){}
BOOL ProbeInsert(LPBYTE p,WV_ULONG n,WORD flags){(void)flags;if(n>sizeof(inserted))return FALSE;memcpy(inserted,p,(size_t)n);insertedSize=(size_t)n;return TRUE;}
BOOL ProbeUndo(PPOSITION p,WV_ULONG n,WORD f){(void)p;(void)n;(void)f;return TRUE;}
void ProbeX(PCOLUMN p){(void)p;}
BOOL ProbeHide(void){return TRUE;}void ProbeShow(void){}BOOL ProbeView(void){return FALSE;}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int wmain(void)
{
 BYTE bytes[8192];WCHAR expected[8192];hInst=GetModuleHandleW(NULL);PageThreshold=1024;UtfEncoding=0;HexEditMode=FALSE;
 for(INT cp=932;cp<=950;cp+=18)for(INT mode=0;mode<2;mode++){
  WCHAR word[3]={cp==932?0x65e5:0x4e2d,cp==932?0x672c:0x6587,0};INT n;
  CharSet=mode?CS_OEM:CS_ANSI;AnsiCodePage=OemCodePage=cp;
  memset(bytes,'a',4095);n=WideCharToMultiByte(cp,0,word,2,(LPSTR)bytes+4095,8,NULL,NULL);CHECK(n==4);bytes[4095+n]='\n';
  for(INT i=0;i<4095;i++)expected[i]='a';expected[4095]=word[0];expected[4096]=word[1];expected[4097]='\n';expected[4098]=0;
  LPPAGE first=NewPage(),last=NewPage();CHECK(first && last);first->Fill=4096;last->Fill=4;
  first->PageNo=1;last->PageNo=2;first->Next=last;last->Prev=first;memcpy(first->PageBuf,bytes,4096);memcpy(last->PageBuf,bytes+4096,4);
  FirstPage=first;LastPage=last;CurrPos.p=SelectStart.p=first;CurrPos.i=CurrPos.f=SelectStart.i=SelectStart.f=0;WvDbcsInvalidate();
  SelectCount=4100;CHECK(ClipboardCopy());CHECK(format==CF_UNICODETEXT);
  WCHAR *wide=GlobalLock(clipboard);CHECK(wide && !wcscmp(wide,expected));GlobalUnlock(clipboard);
  SelectCount=0;Mode=InsertMode;insertedSize=0;CHECK(ClipboardPaste());CHECK(insertedSize==4100 && !memcmp(inserted,bytes,4100));
  CHECK((GlobalFlags(clipboard)&GMEM_LOCKCOUNT)==0);
  ProbeEmpty();clipboard=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,8);format=CF_UNICODETEXT;wide=GlobalLock(clipboard);
  wide[0]=0xd83d;wide[1]=0xde00;wide[2]=0;GlobalUnlock(clipboard);insertedSize=0;
  CHECK(!ClipboardPaste());CHECK(!insertedSize);CHECK((GlobalFlags(clipboard)&GMEM_LOCKCOUNT)==0);
  ProbeEmpty();WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;
 }
 printf("PASS %u isolated MBCS clipboard checks (system clipboard untouched)\n",checks);return 0;
}
