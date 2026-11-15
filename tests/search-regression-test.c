/* Production matcher regressions, with no dialogs or alert sounds. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BOOL WINAPI QuietBeep(UINT type){(void)type;return TRUE;}
#define MessageBeep QuietBeep
#include "../Search.c"
static int failures,checks;
static void Expect(const char *name,const WCHAR *pattern,const BYTE *bytes,int length,int utf,BOOL yes){
 PAGE pages[2]={{0}};POSITION p;SEARCHPART sentinel={0};WCHAR cmd[512];INT error=0;BOOL result;int direction;
 if(length>8192)abort();pages[0].PageBuf=(BYTE*)bytes;pages[0].Fill=length>4096?4096:length;pages[0].PageNo=1;
 FirstPage=LastPage=&pages[0];
 if(length>4096){pages[1].PageBuf=(BYTE*)bytes+4096;pages[1].Fill=length-4096;pages[1].PageNo=2;pages[1].Prev=&pages[0];pages[0].Next=&pages[1];LastPage=&pages[1];}
 CurrPos.p=ScreenStart.p=FirstPage;CurrPos.i=CurrPos.f=ScreenStart.i=ScreenStart.f=0;
 UtfEncoding=utf;UtfLsbFirst=TRUE;CharSet=utf==8?CS_UTF8:utf==16?CS_UTF16LE:CS_ANSI;
 AnsiCodePage=1252;OemCodePage=437;MagicFlag=TRUE;IgnoreCaseFlag=FALSE;WrapScanFlag=FALSE;hwndMain=NULL;Disabled=0;
 wcscpy(cmd,pattern);BuildMatchList(cmd,L'/',&error);
 if(!MatchValid){printf("FAIL %s: compile error %d\n",name,error);failures++;return;}
 sentinel.Prev=LastMatch;
 for(direction=0;direction<2;direction++){
  Interrupted=FALSE;RecursiveBranch=0;MinimumMatchBkwd=FALSE;StartTime=GetCurrentTime();
  p.p=direction?LastPage:FirstPage;p.i=direction?LastPage->Fill:0;p.f=0;
  result=direction?CheckBkwd(&p,&sentinel):CheckFwd(&p,FirstMatch);checks++;
  if(result!=yes || (yes && CountBytes(&p)!=(direction?0:(WV_ULONG)length))){
   printf("FAIL %s %s: matched=%d offset=%llu expected=%d/%d\n",name,direction?"backward":"forward",result,(unsigned long long)CountBytes(&p),yes,direction?0:length);failures++;
  }
 }
}
static void ByteClasses(void){
 BYTE bullet[]={0xe2,0x80,0xa2},continuations[]={0x80,0xbf,0x85},lambda[]={0xce,0xbb},wide[]={0xbb,3},zero[]={0};BYTE boundary[4098];
 Expect("bullet-byte-classes",L"/[\\%e2][\\%80][\\%a2]/",bullet,3,8,TRUE);
 Expect("bullet-byte-literal-control",L"/\\%e2\\%80\\%a2/",bullet,3,8,TRUE);
 Expect("bullet-octal-classes",L"/[\\342][\\200][\\242]/",bullet,3,8,TRUE);
 Expect("bullet-byte-ranges",L"/[\\%e0-\\%ef][\\%80-\\%bf][\\%80-\\%bf]/",bullet,3,8,TRUE);
 Expect("raw-byte-star",L"/[\\%80-\\%bf]*/",continuations,3,8,TRUE);
 Expect("raw-inverse-star",L"/[^\\%00]*/",bullet,3,8,TRUE);
 Expect("unicode-class-control",L"/[\x03bb]/",lambda,2,8,TRUE);
 Expect("unicode-dot-control",L"/./",lambda,2,8,TRUE);
 Expect("UTF16-literal-unit",L"/[\\%03bb]/",wide,2,16,TRUE);
 Expect("literal-NUL",L"/[\\%00]/",zero,1,8,TRUE);
 Expect("negative-byte",L"/[\\%e3][\\%80][\\%a2]/",bullet,3,8,FALSE);
 memset(boundary,'a',4095);memcpy(boundary+4095,bullet,3);
 Expect("page-boundary",L"/[\\%61]*[\\%e2][\\%80][\\%a2]/",boundary,sizeof(boundary),8,TRUE);
}
static void RepeatAlternatives(void){
 Expect("repeat-alternative-suffix",L"/\\(.*\\|.*\\);/",(BYTE*)"a;",2,0,TRUE);
 Expect("repeat-second-branch",L"/^\\(x*\\|a*\\);$/",(BYTE*)"aaa;",4,0,TRUE);
 Expect("repeat-with-prefix",L"/z\\(a*\\|b*\\);/",(BYTE*)"zaaa;",5,0,TRUE);
 Expect("repeat-empty",L"/\\(a*\\|b*\\);/",(BYTE*)";",1,0,TRUE);
 Expect("repeat-negative",L"/\\(a*\\|b*\\);/",(BYTE*)"aaa!",4,0,FALSE);
 Expect("repeat-anchored",L"/^\\(.*\\|.*\\);$/",(BYTE*)"a;",2,0,TRUE);
 Expect("repeat-nested",L"/\\(\\(a*\\|b*\\)\\|c*\\);/",(BYTE*)"aaa;",4,0,TRUE);
}
int main(int argc,char **argv){extern BOOL SuppressStatusBeep;(void)argc;(void)argv;SuppressStatusBeep=TRUE;ByteClasses();RepeatAlternatives();printf("%s %d search regression checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;}
