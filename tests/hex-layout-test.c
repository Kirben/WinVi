#include <windows.h>
#include <stdio.h>
#include "../WinVi.h"
#include "../Page.h"
extern BOOL HexEditTextSide;
int main(void) {
 PAGE page={0};BYTE data[32];POSITION p;int cell;
 memset(data,'a',sizeof(data));page.PageBuf=data;page.Fill=32;page.PageNo=1;
 FirstPage=LastPage=&page;HexEditMode=TRUE;UtfEncoding=0;Mode=CommandMode;AveCharWidth=8;
 for(cell=14;cell<=15;cell++) {
  p.p=&page;p.i=p.f=0;CurrCol.PixelOffset=20*8+((51*8*cell)>>4)+2;
  AdvanceToCurr(&p,0);
  if(HexEditTextSide || p.i!=(UINT)cell) {printf("FAIL hex cell %d maps to side=%d byte=%u\n",cell,HexEditTextSide,p.i);return 1;}
 }
 for(cell=0;cell<2;cell++) {
  p.p=&page;p.i=p.f=0;CurrCol.PixelOffset=(72+cell)*8+4;
  AdvanceToCurr(&p,0);
  if(!HexEditTextSide || p.i!=(UINT)cell) {printf("FAIL ASCII cell %d maps to side=%d byte=%u\n",cell,HexEditTextSide,p.i);return 1;}
 }
 puts("PASS actual hex/ASCII hit mapping at the widened address-column boundary");return 0;
}
