#ifndef WINVI_HEXPRINT_H
#define WINVI_HEXPRINT_H
/* Raw-byte print rows. ASCII is deliberately independent of file encoding. */
#define WV_HEX_PRINT_CAP 88
static int WvHexPrintColumns(int bytes) { return 21 + 4 * bytes + (bytes > 8); }
static int WvHexPrintBytes(int columns)
{
 int bytes;
 for(bytes=16;bytes;bytes/=2)if(WvHexPrintColumns(bytes)<=columns)return bytes;
 return 0;
}
static int WvHexPrintRow(WCHAR *out, WV_ULONG offset,
 const BYTE *data, int count, int width)
{
 static const WCHAR hex[]=L"0123456789ABCDEF";
 int i,n=0;
 if(width<1 || width>16 || count<0 || count>width)return 0;
 for(i=15;i>=0;i--)out[n++]=hex[(offset>>(i*4))&15];
 out[n++]=L':';out[n++]=L' ';
 for(i=0;i<width;i++){
  if(i==8)out[n++]=L' ';
  out[n++]=i<count?hex[data[i]>>4]:L' ';
  out[n++]=i<count?hex[data[i]&15]:L' ';
  out[n++]=L' ';
 }
 out[n++]=L' ';out[n++]=L'|';
 for(i=0;i<width;i++)out[n++]=i>=count?L' ':
  (data[i]>=32 && data[i]<=126?(WCHAR)data[i]:L'.');
 out[n++]=L'|';out[n]=0;return n;
}
#endif
