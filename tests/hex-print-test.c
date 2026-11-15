/* Exercise the real PrintIt dispatch and hex print loop without a printer. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
static int WINAPI ProbeStartDoc(HDC,const DOCINFOW*);
static int WINAPI ProbeStartPage(HDC);
static int WINAPI ProbeEndPage(HDC);
static int WINAPI ProbeEndDoc(HDC);
static int WINAPI ProbeAbort(HDC);
static int WINAPI ProbeCaps(HDC,int);
static BOOL WINAPI ProbeMetrics(HDC,LPTEXTMETRICA);
static BOOL WINAPI ProbeOut(HDC,int,int,UINT,const RECT*,LPCWSTR,UINT,const INT*);
static BOOL WINAPI ProbeText(HDC,int,int,LPCWSTR,int);
static BOOL (WINAPI *ActualOut)(HDC,int,int,UINT,const RECT*,LPCWSTR,UINT,const INT*)=ExtTextOutW;
static BOOL (WINAPI *ActualText)(HDC,int,int,LPCWSTR,int)=TextOutW;
static BOOL (WINAPI *ActualMetrics)(HDC,LPTEXTMETRICA)=GetTextMetricsA;
#define StartDocW ProbeStartDoc
#define StartPage ProbeStartPage
#define EndPage ProbeEndPage
#define EndDoc ProbeEndDoc
#define AbortDoc ProbeAbort
#define GetDeviceCaps ProbeCaps
#undef GetTextMetrics
#define GetTextMetrics ProbeMetrics
#define ExtTextOutW ProbeOut
#define TextOutW ProbeText
#define MessageLoop ProbeMessages
#define Disable ProbeDisable
#define Enable ProbeEnable
#define NewStatus ProbeStatus
#define ByteAt ProbeByteAt
#define ExecRunning ProbeExecRunning
#include "../Print.c"
#undef ByteAt

#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {WCHAR text[WV_HEX_PRINT_CAP];int page,x,y;} OUTPUT;
static OUTPUT output[2048];static PAGE pages[4];static BYTE data[16384];
static int printed,started,ended,docs,finishes,aborts,messages,cancelAt,errorAt,failRead,reads,statuses,disables;
static int paperWidth=700,paperHeight=52,headings,raster;
static HDC dc;
static WCHAR textBody[1024];static int textLength;
static BOOL commandRunning;
extern LRESULT CALLBACK MainProc(HWND,UINT,WPARAM,LPARAM);
BOOL ProbeExecRunning(VOID){return commandRunning;}
static int WINAPI ProbeStartDoc(HDC h,const DOCINFOW *d){docs++;return errorAt==1?-1:1;}
static int WINAPI ProbeStartPage(HDC h){started++;return errorAt==2?-1:1;}
static int WINAPI ProbeEndPage(HDC h){ended++;return errorAt==4?-1:1;}
static int WINAPI ProbeEndDoc(HDC h){finishes++;return errorAt==5?-1:1;}
static int WINAPI ProbeAbort(HDC h){aborts++;return 1;}
static int WINAPI ProbeCaps(HDC h,int n){return n==HORZRES?paperWidth:n==VERTRES?paperHeight:96;}
static BOOL WINAPI ProbeMetrics(HDC h,LPTEXTMETRICA m){if(raster)return ActualMetrics(h,m);memset(m,0,sizeof(*m));m->tmHeight=10;m->tmAveCharWidth=m->tmMaxCharWidth=8;return TRUE;}
static BOOL WINAPI ProbeOut(HDC h,int x,int y,UINT flags,const RECT *r,LPCWSTR s,UINT n,const INT *dx)
{
 unsigned i;CHECK(n<WV_HEX_PRINT_CAP);CHECK(printed<2048);
 CHECK(x>=0 && x+(int)n*dx[0]<=paperWidth);CHECK(y>=0 && y+10<=paperHeight);
 for(i=0;i<n;i++)CHECK(dx[i]==dx[0]);
 memcpy(output[printed].text,s,n*sizeof(WCHAR));output[printed].text[n]=0;
 output[printed].page=started;output[printed].x=x;output[printed].y=y;printed++;
 return errorAt==3?FALSE:raster?ActualOut(h,x,y,flags,r,s,n,dx):TRUE;
}
static BOOL WINAPI ProbeText(HDC h,int x,int y,LPCWSTR s,int n){headings++;if(n>0 && textLength+n<1024){memcpy(textBody+textLength,s,n*sizeof(WCHAR));textLength+=n;textBody[textLength]=0;}return raster?ActualText(h,x,y,s,n):TRUE;}
INT ProbeMessages(BOOL wait){messages++;if(cancelAt && messages>=cancelAt)PrintCancelled=TRUE;return 0;}
void ProbeDisable(WORD flags){disables++;}
void ProbeEnable(void){disables--;}
void ProbeStatus(INT field,PWSTR text,INT flags){statuses++;}
INT ProbeByteAt(PPOSITION p){if(failRead && ++reads>=failRead)return C_ERROR;return ByteAt(p);}
static void Setup(int size)
{
 int i,n=(size+4095)/4096;if(!n)n=1;
 for(i=0;i<16384;i++)data[i]=(BYTE)i;
 for(i=0;i<n;i++){
  pages[i].PageBuf=data+i*4096;pages[i].Fill=(UINT)(size-i*4096>4096?4096:size-i*4096);
  pages[i].PageNo=i+1;pages[i].Prev=i?&pages[i-1]:NULL;pages[i].Next=i+1<n?&pages[i+1]:NULL;
 }
 FirstPage=pages;LastPage=pages+n-1;CurrFile=L"dummy.bin";
 CurrPos.p=pages;CurrPos.i=CurrPos.f=0;SelectStart=CurrPos;SelectBytePos=SelectCount=0;
 pd.Flags=PD_ALLPAGES;pd.nFromPage=1;pd.nToPage=0xffff;
 HexEditMode=TRUE;UtfEncoding=0;HexPrintInProgress=PrintCancelled=Interrupted=FALSE;
 nPrtLeft=nPrtRight=nPrtTop=nPrtBottom=0;
 Header=Footer=NULL;Frame=Frame2=Frame3=DoubleColumn=LineNumbering=FALSE;
 printed=started=ended=docs=finishes=aborts=messages=cancelAt=errorAt=failRead=reads=statuses=disables=headings=0;
 textLength=0;textBody[0]=0;
 paperWidth=700;paperHeight=52;
}
static void VerifyRow(int index,WV_ULONG address,int source,int count,int width)
{
 WCHAR expected[32];int i,ascii=18+3*width+(width>8)+2;
 _snwprintf(expected,32,L"%016I64X: ",address);
 CHECK(!wcsncmp(output[index].text,expected,18));
 CHECK((int)wcslen(output[index].text)==WvHexPrintColumns(width));
 for(i=0;i<count;i++){
  int at=18+i*3+(i>=8);BYTE b=data[source+i];
  _snwprintf(expected,32,L"%02X",(unsigned)b);
  CHECK(output[index].text[at]==expected[0] && output[index].text[at+1]==expected[1]);
  CHECK(output[index].text[ascii+i]==(b>=32 && b<=126?b:'.'));
 }
 for(;i<width;i++)CHECK(output[index].text[ascii+i]==' ');
}
static void Render(int copies){PrintIt(dc,copies,TRUE,TRUE);CHECK(!HexPrintInProgress && !disables);}
int main(void)
{
 int sizes[]={0,1,15,16,17,79,80,81,255,256,4095,4096,4097,8193};int s,i,n;
 DWORD handlesBefore,handlesAfter,gdiBefore,gdiAfter;
 dc=CreateCompatibleDC(NULL);CHECK(dc);
 for(s=0;s<(int)(sizeof(sizes)/sizeof(sizes[0]));s++){
  Setup(sizes[s]);Render(1);CHECK(!PrintCancelled && !aborts && docs==1 && finishes==1);
  CHECK(printed==(sizes[s]+15)/16);CHECK(started==(printed?((printed+4)/5):1));CHECK(ended==started);
  for(i=0;i<printed;i++)VerifyRow(i,(WV_ULONG)i*16,i*16,sizes[s]-i*16<16?sizes[s]-i*16:16,16);
  for(i=0;i<16384;i++)CHECK(data[i]==(BYTE)i);
 }
 puts("PASS empty/partial rows, all byte values, page and storage boundaries");
 Setup(20);data[0]=0xef;data[1]=0xbb;data[2]=0xbf;UtfEncoding=8;Render(1);VerifyRow(0,0,0,16,16);
 Setup(20);data[0]=0xff;data[1]=0xfe;UtfEncoding=16;Render(1);VerifyRow(0,0,0,16,16);
 puts("PASS UTF-8/UTF-16 BOM and control bytes preserved as raw bytes");
 Setup(128);pd.Flags=PD_SELECTION;SelectStart.i=3;SelectBytePos=0x100000003ULL;SelectCount=17;Render(1);
 CHECK(printed==2);VerifyRow(0,0x100000003ULL,3,16,16);VerifyRow(1,0x100000013ULL,19,1,16);
 Setup(128);pd.Flags=PD_SELECTION;SelectStart.i=7;SelectBytePos=0x400000007ULL;SelectCount=1;Render(1);VerifyRow(0,0x400000007ULL,7,1,16);
 puts("PASS unaligned selection and offsets beyond 4/16 GiB (synthetic offsets)");
 Setup(161);pd.nFromPage=pd.nToPage=2;Render(1);CHECK(started==1 && printed==5);VerifyRow(0,80,80,16,16);
 Setup(161);Render(2);CHECK(started==6 && printed==22);VerifyRow(0,0,0,16,16);VerifyRow(5,0,0,16,16);VerifyRow(10,80,80,16,16);
 Setup(81);PrintIt(dc,1,TRUE,FALSE);PrintIt(dc,1,FALSE,TRUE);CHECK(docs==1 && finishes==1 && printed==12 && started==4);
 puts("PASS page ranges and collated/uncollated copy sequencing");
 Setup(81);paperWidth=350;Render(1);CHECK(printed==21);VerifyRow(0,0,0,4,4);
 /* 350 pixels fits only four bytes per row at eight pixels per cell. */
 Setup(32);paperWidth=220;Render(1);VerifyRow(0,0,0,1,1);
 Setup(161);paperWidth=1500;DoubleColumn=TRUE;Frame=Frame2=Frame3=TRUE;paperHeight=250;Header=L"&n";Footer=L"&p";Render(1);CHECK(headings==2 && started==1);
 puts("PASS narrow paper, fixed cell placement, headers/footers and framed columns");
 Setup(4097);cancelAt=3;Render(1);CHECK(PrintCancelled && aborts==1 && printed==2 && finishes==0);
 for(n=1;n<=5;n++){Setup(81);errorAt=n;Render(1);CHECK(PrintCancelled && statuses==1 && (n==1 || aborts==1));}
 Setup(81);failRead=18;Render(1);CHECK(PrintCancelled && aborts==1 && printed==1);
 Setup(1);paperWidth=100;Render(1);CHECK(PrintCancelled && aborts==1 && printed==0);
 Setup(1);paperHeight=5;Render(1);CHECK(PrintCancelled && aborts==1 && printed==0);
 puts("PASS cancellation, spool/output/read failures and impossible geometry");
 Setup(11);memcpy(data,"alpha\r\nbeta",11);HexEditMode=FALSE;Render(1);
 CHECK(!PrintCancelled && printed==0 && !wcscmp(textBody,L"alphabeta"));
 puts("PASS existing text print dispatch remains text, not hexadecimal");
 Setup(1);HexPrintInProgress=TRUE;
 CHECK(MainProc(NULL,WM_CLOSE,0,0)==0 && PrintCancelled && FirstPage==pages);
 PrintCancelled=FALSE;CHECK(!MainProc(NULL,WM_QUERYENDSESSION,0,0) && PrintCancelled && FirstPage==pages);
 HexPrintInProgress=FALSE;PrintCancelled=FALSE;commandRunning=TRUE;
 CHECK(!PrintDoc(NULL,TRUE) && statuses==1 && docs==0);commandRunning=FALSE;
 puts("PASS main-window close/session guards and active-command exclusion");
 GetProcessHandleCount(GetCurrentProcess(),&handlesBefore);
 gdiBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 for(n=0;n<100;n++){Setup(17);Render(1);CHECK(!PrintCancelled);}
 GetProcessHandleCount(GetCurrentProcess(),&handlesAfter);CHECK(handlesAfter==handlesBefore);
 gdiAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);CHECK(gdiBefore==gdiAfter);
 {
  BITMAPINFO info={0};BITMAPFILEHEADER file={0};HBITMAP bitmap,old;void *pixels;FILE *f;
  Setup(256);paperWidth=1200;paperHeight=500;raster=1;
  info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1200;info.bmiHeader.biHeight=-500;
  info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=24;info.bmiHeader.biCompression=BI_RGB;
  bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);CHECK(bitmap);
  old=SelectObject(dc,bitmap);PatBlt(dc,0,0,1200,500,WHITENESS);
  Header=L"WinVi hexadecimal print - &n";Footer=L"Page &p";Frame=TRUE;nPrtLeft=nPrtRight=5;Render(1);
  CHECK(!PrintCancelled && started==1 && printed==16);GdiFlush();
  file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+1200*500*3;
  f=fopen("hex-print-preview.bmp","wb");CHECK(f);CHECK(fwrite(&file,sizeof(file),1,f)==1);
  CHECK(fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,f)==1);CHECK(fwrite(pixels,1200*500*3,1,f)==1);CHECK(!fclose(f));
  SelectObject(dc,old);DeleteObject(bitmap);puts("PASS real GDI rendering to hex-print-preview.bmp");
 }
 CHECK(DeleteDC(dc));puts("PASS repeated printing handle cleanup");return 0;
}
