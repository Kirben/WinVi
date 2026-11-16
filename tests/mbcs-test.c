#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../WinVi.h"
#include "../Page.h"
extern INT AnsiCodePage,OemCodePage;
extern BOOL IsFixedFont;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s (error %lu)\n",__LINE__,#x,GetLastError());exit(1);}}while(0)
static void Document(const BYTE *bytes,size_t length)
{
 if(FirstPage)WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;RemoveTmpFile();
 size_t at=0;do{
  LPPAGE p=NewPage();CHECK(p!=NULL);p->Prev=LastPage;p->PageNo=LastPage?LastPage->PageNo+1:1;
  if(LastPage)LastPage->Next=p;else FirstPage=p;LastPage=p;
  p->Fill=(UINT)(length-at>PAGESIZE?PAGESIZE:length-at);if(p->Fill)memcpy(p->PageBuf,bytes+at,p->Fill);at+=p->Fill;
 }while(at<length);CurrPos.p=FirstPage;CurrPos.i=CurrPos.f=0;WvDbcsInvalidate();
}
static WV_ULONG Offset(const POSITION *p){return (p->p->PageNo-1)*PAGESIZE+p->i;}
int wmain(void)
{
 WCHAR directory[WV_PATH_CAP];BYTE *bytes=malloc(200000),encoded[4];WCHAR *oracle=malloc(200000*sizeof(WCHAR));
 CHECK(bytes && oracle);hInst=GetModuleHandleW(NULL);PageThreshold=16;UtfEncoding=0;HexEditMode=FALSE;CharSet=CS_ANSI;
 CHECK(GetCurrentDirectoryW(WV_PATH_CAP,directory)>0);wcscat(directory,L"\\tests\\cases");CreateDirectoryW(directory,NULL);
 TmpDirectory=directory;DefaultTmpFlag=FALSE;
 for(INT mode=0;mode<2;mode++)for(INT code=0;code<2;code++){
  UINT cp=code?950:932;size_t used=0,characters=0,pairs=0;CharSet=mode?CS_OEM:CS_ANSI;AnsiCodePage=OemCodePage=cp;
  for(INT lead=128;lead<256;lead++)if(IsDBCSLeadByteEx(cp,(BYTE)lead))for(INT trail=0;trail<256;trail++){
   CHAR pair[2]={(CHAR)lead,(CHAR)trail};WCHAR value;
   if(MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,pair,2,&value,1)!=1)continue;
   bytes[used++]=(BYTE)lead;bytes[used++]=(BYTE)trail;bytes[used++]='|';oracle[characters++]=value;oracle[characters++]='|';pairs++;
  }
  {WCHAR src[2]={oracle[0],oracle[2]},dst[3]={0,0,0x1234};HDC dc=CreateCompatibleDC(NULL);SIZE extent;
   CHECK(WvDbcsText());CHECK(dc!=NULL);UnicodeConvert(dst,src,2);CHECK(dst[0]==src[0] && dst[1]==src[1] && dst[2]==0x1234);
   IsFixedFont=TRUE;AveCharWidth=1;CHECK(GetTextExtentPoint32W(dc,src,2,&extent));CHECK(GetTextWidth(dc,src,2)==extent.cx);DeleteDC(dc);
   HexEditMode=TRUE;src[0]=0x81;src[1]='X';UnicodeConvert(dst,src,2);CHECK(dst[0]==0xfffd && dst[1]=='X' && dst[2]==0x1234);if(mode){src[0]=1;UnicodeConvert(dst,src,1);CHECK(dst[0]==0x263a && dst[2]==0x1234);}HexEditMode=FALSE;
  }
  Document(bytes,used);CHECK(WvDbcsText());POSITION p=CurrPos;
  for(size_t i=0;i<characters;i++){
   CHECK(CharAt(&p)==oracle[i]);CHECK(WvDbcsBoundary(&p));CHECK(CharAndAdvance(&p)==oracle[i]);
   CHECK(Offset(&p)==(i/2)*3+(i%2?3:2));
  }
  CHECK(CharAt(&p)==C_EOF);
  for(size_t i=characters;i;i--){CHECK(GoBackAndChar(&p)==oracle[i-1]);CHECK(Offset(&p)==((i-1)/2)*3+((i-1)%2?2:0));}
  CHECK(GoBackAndChar(&p)==C_EOF);
  for(INT value=0;value<65536;value++)if(value<0xd800 || value>0xdfff){
   WCHAR wide=(WCHAR)value;BOOL usedDefault=FALSE;CHAR expected[4];
   INT n=WideCharToMultiByte(cp,WC_NO_BEST_FIT_CHARS,&wide,1,expected,4,NULL,&usedDefault);
   INT got=WvLegacyEncode(value,encoded);CHECK(got==(usedDefault?0:n));if(got)CHECK(!memcmp(encoded,expected,got));
  }
  for(INT b=0;b<256;b++){
   BYTE byte=(BYTE)b;WCHAR expected;INT value=MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,(LPCSTR)&byte,1,&expected,1)==1?expected:C_UNDEF;
   Document(&byte,1);p=CurrPos;CHECK(CharAndAdvance(&p)==value);CHECK(Offset(&p)==1);CHECK(GoBackAndChar(&p)==value);CHECK(Offset(&p)==0);
  }
  {BYTE crlf[]={0x41,0x0d,0x0a,0x42};Document(crlf,sizeof(crlf));p=CurrPos;
   CHECK(CharAndAdvance(&p)=='A');CHECK(CharAt(&p)==C_CRLF);CHECK(CharAndAdvance(&p)==C_CRLF);CHECK(Offset(&p)==3);
   CHECK(GoBackAndChar(&p)==C_CRLF);CHECK(Offset(&p)==1);
  }
  printf("PASS CP%u %s: %zu valid pairs across pages and strict encoding\n",cp,mode?"OEM":"ANSI",pairs);
 }
 CharSet=CS_ANSI;AnsiCodePage=950;
 {size_t length=200000;memset(bytes,0xa4,length);Document(bytes,length);POSITION p={LastPage,LastPage->Fill,0};
  ULONGLONG start=GetTickCount64();for(size_t i=0;i<length/2;i++){CHECK(GoBackAndChar(&p)==0x4e2d);CHECK(Offset(&p)==length-2*(i+1));}
  printf("Big5 ambiguous-byte backward scan: %llu ms\n",GetTickCount64()-start);CHECK(GetTickCount64()-start<30000);
  CHECK(FirstPage->PageBuf || ReloadPage(FirstPage));FirstPage->PageBuf[0]='A';WvDbcsDirty(FirstPage);
  p.p=LastPage;p.i=LastPage->Fill;CHECK(GoBackAndChar(&p)==C_UNDEF);CHECK(Offset(&p)==length-1);
  CHECK(GoBackAndChar(&p)==0x4e2d);CHECK(Offset(&p)==length-3);
 }
 {BYTE invalid[]={0x81,0x20,0x81};AnsiCodePage=932;Document(invalid,sizeof(invalid));POSITION p=CurrPos;
  CHECK(CharAndAdvance(&p)==C_UNDEF);CHECK(CharAndAdvance(&p)==' ');CHECK(CharAndAdvance(&p)==C_UNDEF);
  CHECK(GoBackAndChar(&p)==C_UNDEF);CHECK(GoBackAndChar(&p)==' ');CHECK(GoBackAndChar(&p)==C_UNDEF);
 }
 WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;RemoveTmpFile();TmpDirectory=NULL;free(bytes);free(oracle);
 printf("PASS %u MBCS codec/boundary/paging checks\n",checks);return 0;
}
