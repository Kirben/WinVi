#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static BOOL failAllocation,failUndo;
static UINT viewChecks;
static void *JournalMalloc(size_t n){return failAllocation?NULL:malloc(n);}
static BOOL WINAPI QuietUpdate(HWND h){(void)h;return TRUE;}
#define IsViewOnly ProbeView
#define EnterDeleteForUndo ProbeUndo
#define NewStatus ProbeStatus
#define malloc JournalMalloc
#define NewPosition ProbePosition
#define InvalidateArea ProbeInvalidate
#define SetUnsafe ProbeUnsafe
#define HideEditCaret ProbeHide
#define ShowEditCaret ProbeShow
#define GetXPos ProbeX
#define UpdateWindow QuietUpdate
#include "../InsDel.c"
#undef malloc
#undef EnterDeleteForUndo
BOOL EnterDeleteForUndo(PPOSITION,WV_ULONG,WORD);
BOOL ProbeUndo(PPOSITION p,WV_ULONG n,WORD f){return failUndo?FALSE:EnterDeleteForUndo(p,n,f);}
extern INT AnsiCodePage,OemCodePage;
BOOL ProbeView(void){viewChecks++;return ViewOnlyFlag;}
void ProbeStatus(INT f,PWSTR text,INT flags){(void)f;(void)text;(void)flags;}
WV_LONG ProbePosition(PPOSITION p){CurrPos=*p;return 0;}
void ProbeInvalidate(PPOSITION p,WV_LONG n,UINT f){(void)p;(void)n;(void)f;}
void ProbeUnsafe(void){}BOOL ProbeHide(void){return TRUE;}void ProbeShow(void){}void ProbeX(PCOLUMN p){(void)p;}
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static LINEINFO lines[2];
static void Document(const BYTE *bytes,UINT n){
 WvReplaceReset();DisableUndo();if(FirstPage)WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;
 UINT at=0;do{LPPAGE p=NewPage();CHECK(p!=NULL);p->PageNo=LastPage?LastPage->PageNo+1:1;p->Prev=LastPage;if(LastPage)LastPage->Next=p;else FirstPage=p;LastPage=p;
 p->Fill=n-at>PAGESIZE?PAGESIZE:n-at;if(p->Fill)memcpy(p->PageBuf,bytes+at,p->Fill);at+=p->Fill;}while(at<n);
 memset(lines,0,sizeof(lines));LineInfo=lines;CurrLine=VisiLines=0;CurrPos.p=ScreenStart.p=FirstPage;CurrPos.i=CurrPos.f=0;ScreenStart=CurrPos;lines[0].Pos=CurrPos;
 LinesInFile=1;SelectCount=0;Mode=ReplaceMode;ReplaceSingle=FALSE;HexEditMode=FALSE;ViewOnlyFlag=FALSE;InsertCount=0;WvDbcsInvalidate();StartUndoSequence();
}
static void Equal(const BYTE *bytes,UINT length){POSITION p={FirstPage,0,0};for(UINT i=0;i<length;i++)CHECK(ByteAndAdvance(&p)==bytes[i]);CHECK(ByteAt(&p)==C_EOF);}
int wmain(void){
 hInst=GetModuleHandleW(NULL);PageThreshold=1024;UtfEncoding=0;UtfLsbFirst=FALSE;
 for(INT mode=0;mode<2;mode++)for(INT cp=932;cp<=950;cp+=18){
  CharSet=mode?CS_OEM:CS_ANSI;AnsiCodePage=OemCodePage=cp;unsigned pairs=0;
  for(INT a=128;a<256;a++)if(IsDBCSLeadByteEx(cp,(BYTE)a))for(INT b=0;b<256;b++){
   BYTE original[4]={(BYTE)a,(BYTE)b,'X','\n'};WCHAR value;if(MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,(LPCSTR)original,2,&value,1)!=1)continue;
   Document(original,4);InsertChar('Z');CHECK(Replaced!=NULL && CountBytes(&CurrPos)==1);CHECK(WvReplaceBackspace());Equal(original,4);CHECK(!Replaced && CountBytes(&CurrPos)==0);pairs++;
  }printf("PASS CP%d %s: %u raw pairs restored exactly\n",cp,mode?"OEM":"ANSI",pairs);
 }
 CharSet=CS_ANSI;AnsiCodePage=932;
 {BYTE original[]={'a','b','\n'};Document(original,sizeof(original));ViewOnlyFlag=TRUE;viewChecks=0;InsertChar('X');CHECK(viewChecks==1 && !Replaced);Equal(original,sizeof(original));ViewOnlyFlag=FALSE;}

 {BYTE original[]={0x93,0xfa,'X','\n'};Document(original,sizeof(original));InsertChar('Z');BYTE typed[]={'Z','X','\n'};failUndo=TRUE;CHECK(!WvReplaceBackspace());failUndo=FALSE;Equal(typed,sizeof(typed));}

 {BYTE original[]={0x81,' ','X','\n'};Document(original,sizeof(original));InsertChar('Z');CHECK(WvReplaceBackspace());Equal(original,sizeof(original));}
 {BYTE original[]={'a','b','\n'};Document(original,sizeof(original));InsertChar('X');failAllocation=TRUE;InsertChar('Y');failAllocation=FALSE;CHECK(CountBytes(&CurrPos)==1);CHECK(WvReplaceBackspace());Equal(original,sizeof(original));}
 {BYTE original[]={'a','b','\n'};Document(original,sizeof(original));failAllocation=TRUE;InsertChar('X');failAllocation=FALSE;Equal(original,sizeof(original));CHECK(!Replaced && CountBytes(&CurrPos)==0);}
 {BYTE original[]={'a','b','\n'};Document(original,sizeof(original));InsertChar('X');UnsafePage(&CurrPos);CHECK(!Replaced);CHECK(WvReplaceBackspace());BYTE expected[]={'X','b','\n'};Equal(expected,sizeof(expected));}
 for(INT encoding=0;encoding<3;encoding++){
  UtfEncoding=encoding?16:8;UtfLsbFirst=encoding!=2;BYTE original[12];WCHAR text[]={0xd83d,0xde00,'X','\n'};INT n;
  if(!encoding)n=WideCharToMultiByte(CP_UTF8,0,text,4,(LPSTR)original,sizeof(original),NULL,NULL);
  else{n=8;memcpy(original,text,n);if(!UtfLsbFirst)for(INT i=0;i<n;i+=2){BYTE b=original[i];original[i]=original[i+1];original[i+1]=b;}}
  Document(original,n);InsertChar('Z');CHECK(WvReplaceBackspace());Equal(original,n);
  {UINT prefix=encoding?4094:4095,total=prefix+n;BYTE *cross=malloc(total);
   CHECK(cross!=NULL);for(UINT i=0;i<prefix;i++)cross[i]=encoding?(UtfLsbFirst?(i%2?0:'a'):(i%2?'a':0)):'a';
   memcpy(cross+prefix,original,n);Document(cross,total);Advance(&CurrPos,prefix);InsertChar('Z');CHECK(WvReplaceBackspace());Equal(cross,total);free(cross);
  }
 }
 UtfEncoding=0;AnsiCodePage=950;
 {BYTE original[4100];memset(original,'a',4095);original[4095]=0xa4;original[4096]=0xa4;original[4097]='X';original[4098]='\n';Document(original,4099);Advance(&CurrPos,4095);InsertChar('Z');CHECK(WvReplaceBackspace());Equal(original,4099);}
 WvReplaceReset();DisableUndo();WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;LineInfo=NULL;
 printf("PASS %u replacement restoration/failure/boundary checks\n",checks);return 0;
}
