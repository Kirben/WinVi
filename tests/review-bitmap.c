#include <windows.h>
#include <stdio.h>
static int dibCalls;
static int WINAPI SecurityCaps(HDC h,int n){(void)h;(void)n;return 0;}
static HBITMAP WINAPI SecurityDIB(HDC h,const BITMAPINFOHEADER *b,DWORD f,const VOID *p,const BITMAPINFO *i,UINT u) {
 (void)h;(void)f;(void)p;(void)i;(void)u;dibCalls++;
 if(b->biSize!=40 || b->biSizeImage!=4)return NULL;return (HBITMAP)1;
}
#define GetDeviceCaps SecurityCaps
#define CreateDIBitmap SecurityDIB
#include "../Bmp.c"
int SecurityBMP(void) {
 int which;WvCopyString(BitmapFilename,L"security-bitmap.tmp");
 for(which=0;which<11;which++) {
  BITMAPFILEHEADER f={0};BITMAPINFOHEADER b={0};BYTE zeros[256]={0};HANDLE h;DWORD n,header=which==1?124:40,pixel=4;BOOL expect=which<=2,ok;
  f.bfType=0x4d42;f.bfOffBits=14+header;f.bfSize=f.bfOffBits+4;
  b.biSize=header;b.biWidth=1;b.biHeight=which==2?-1:1;b.biPlanes=1;b.biBitCount=24;
  if(which==3)b.biClrUsed=0x40000001;
  if(which==4)b.biCompression=BI_RLE8;
  if(which==5)pixel=1;
  if(which==6)b.biWidth=2147483647;
  if(which==7)b.biHeight=(-2147483647-1);
  if(which==8)b.biBitCount=65535;
  if(which==9)b.biPlanes=0;
  if(which==10)f.bfOffBits=4;
  h=CreateFileW(BitmapFilename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);if(h==INVALID_HANDLE_VALUE)return 1;
  WriteFile(h,&f,sizeof(f),&n,NULL);WriteFile(h,&b,sizeof(b),&n,NULL);if(header>40)WriteFile(h,zeros,header-40,&n,NULL);WriteFile(h,zeros,pixel,&n,NULL);CloseHandle(h);
  dibCalls=0;hBkgndBmp[0]=NULL;ok=InitBmp(NULL,0);hBkgndBmp[0]=NULL;
  if(ok!=expect || dibCalls!=(expect?1:0)){printf("FAIL BMP case %d ok=%d calls=%d\n",which,ok,dibCalls);return 1;}
 }
 DeleteFileW(BitmapFilename);puts("PASS valid/top-down/V5 BMPs and malformed palette, compression, dimensions and extents");return 0;
}
