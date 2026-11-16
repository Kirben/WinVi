#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
extern INT OemCodePage;
static BYTE captured[16384];static size_t capturedSize;static unsigned checks;
#define InsertBuffer ProbeInsert
#define NewPosition ProbePosition
#define ShowEditCaret ProbeCaret
#include "../Exec.c"
BOOL ProbeInsert(LPBYTE p,WV_ULONG size,WORD flags){(void)flags;if(size>sizeof(captured)-capturedSize)return FALSE;memcpy(captured+capturedSize,p,(size_t)size);capturedSize+=(size_t)size;return TRUE;}
WV_LONG ProbePosition(PPOSITION p){CurrPos=*p;return 0;}
VOID ProbeCaret(void){}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int wmain(void){
 PAGE page={0};WV_OUTPUT packet={0};WV_COMMAND session={0};
 page.PageNo=1;page.PageBuf=calloc(PAGESIZE,1);FirstPage=LastPage=&page;CurrPos.p=ExecInsertPos.p=&page;
 for(INT cp=932;cp<=950;cp+=18)for(INT utf=0;utf<3;utf++)for(INT split=1;split<6;split++){
  WCHAR word[4]={cp==932?0x65e5:0x4e2d,cp==932?0x672c:0x6587,'X',0};CHAR encoded[16],expected[32];
  INT size=WideCharToMultiByte(cp,0,word,3,encoded,sizeof(encoded),NULL,NULL),n;
  UtfEncoding=utf==0?8:16;UtfLsbFirst=utf!=2;session.OutputCodePage=cp;session.HasOutputLead=FALSE;capturedSize=0;
  for(INT i=0;i<size;){INT amount=size-i>split?split:size-i;memcpy(packet.Bytes,encoded+i,amount);packet.Size=amount;CHECK(InsertCommandOutput(&session,&packet,FALSE));i+=amount;}
  packet.Size=0;CHECK(InsertCommandOutput(&session,&packet,TRUE));
  if(UtfEncoding==8)n=WideCharToMultiByte(CP_UTF8,0,word,3,expected,sizeof(expected),NULL,NULL);
  else{n=6;memcpy(expected,word,n);if(!UtfLsbFirst)for(INT i=0;i<n;i+=2){CHAR b=expected[i];expected[i]=expected[i+1];expected[i+1]=b;}}
  CHECK(capturedSize==(size_t)n && !memcmp(captured,expected,n));CHECK(!session.HasOutputLead);
  /* A real orphan at EOF becomes one replacement, never silently disappears. */
  capturedSize=0;packet.Size=1;packet.Bytes[0]=(BYTE)encoded[0];CHECK(InsertCommandOutput(&session,&packet,FALSE));CHECK(!capturedSize && session.HasOutputLead);
  packet.Size=0;CHECK(InsertCommandOutput(&session,&packet,TRUE));CHECK(capturedSize==(UtfEncoding==8?3:2));CHECK(!session.HasOutputLead);
 }
 /* Exercise the actual 2048-byte pipe packet limit and the joined 2049-byte buffer. */
 for(INT cp=932;cp<=950;cp+=18){
  WCHAR value=cp==932?0x65e5:0x4e2d;CHAR pair[4],tail[8];INT n;
  CHECK(WideCharToMultiByte(cp,0,&value,1,pair,sizeof(pair),NULL,NULL)==2);
  UtfEncoding=8;session.OutputCodePage=cp;session.HasOutputLead=FALSE;capturedSize=0;
  packet.Size=2048;memset(packet.Bytes,'a',2047);packet.Bytes[2047]=pair[0];CHECK(InsertCommandOutput(&session,&packet,FALSE));CHECK(capturedSize==2047 && session.HasOutputLead);
  packet.Size=2048;packet.Bytes[0]=pair[1];memset(packet.Bytes+1,'b',2047);CHECK(InsertCommandOutput(&session,&packet,FALSE));
  n=WideCharToMultiByte(CP_UTF8,0,&value,1,tail,sizeof(tail),NULL,NULL);CHECK(capturedSize==4094+(size_t)n && !session.HasOutputLead);
  CHECK(!memcmp(captured+2047,tail,n));for(INT i=0;i<2047;i++)CHECK(captured[i]=='a' && captured[2047+n+i]=='b');
  UtfEncoding=0;capturedSize=0;packet.Size=2;memcpy(packet.Bytes,pair,2);CHECK(InsertCommandOutput(&session,&packet,FALSE));CHECK(capturedSize==2 && !memcmp(captured,pair,2) && !session.HasOutputLead);
 }
 free(page.PageBuf);FirstPage=LastPage=NULL;printf("PASS %u MBCS command-output checks\n",checks);return 0;
}
