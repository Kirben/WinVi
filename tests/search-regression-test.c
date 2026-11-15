/* Production matcher regressions, with no dialogs or alert sounds. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BOOL WINAPI QuietBeep(UINT type){(void)type;return TRUE;}
#define MessageBeep QuietBeep
static int allocationCountdown=-1;
static void *TestCalloc(size_t count,size_t size){if(allocationCountdown==0){allocationCountdown=-2;return NULL;}if(allocationCountdown>0)allocationCountdown--;return calloc(count,size);}
#define calloc TestCalloc
static int interruptCountdown=-1;
extern UINT Interrupted;
static DWORD TestTime(void){if(interruptCountdown==0)Interrupted=TRUE;else if(interruptCountdown>0)interruptCountdown--;return 0;}
#undef GetCurrentTime
#define GetCurrentTime TestTime
#include "../Search.c"
#undef calloc
static int failures,checks;
static void Expect(const char *name,const WCHAR *pattern,const BYTE *bytes,int length,int utf,BOOL yes){
 static PAGE pages[2];POSITION p;SEARCHPART sentinel={0};WCHAR cmd[512];INT error=0;BOOL result;int direction;
 memset(pages,0,sizeof(pages));
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
static void RootAlternatives(void){
 Expect("root-second",L"/a\\|b/",(BYTE*)"b",1,0,TRUE);
 Expect("root-first",L"/a\\|b/",(BYTE*)"a",1,0,TRUE);
 Expect("root-third",L"/a\\|b\\|cat/",(BYTE*)"cat",3,0,TRUE);
 Expect("root-negative",L"/a\\|b/",(BYTE*)"c",1,0,FALSE);
 Expect("root-empty-first",L"/\\|a/",(BYTE*)"",0,0,TRUE);
 Expect("root-empty-last",L"/a\\|/",(BYTE*)"",0,0,TRUE);
 Expect("literal-pipe",L"/a|b/",(BYTE*)"a|b",3,0,TRUE);
 Expect("root-nested",L"/a\\|\\(b\\|c\\)d/",(BYTE*)"cd",2,0,TRUE);
 Expect("root-capture",L"/a\\|\\(bc\\)/",(BYTE*)"bc",2,0,TRUE);
 checks++;if(!CheckParenPairs() || nParenPairs!=1){printf("FAIL root capture count\n");failures++;}
 {WCHAR replacement[]=L"\\1";WV_LONG length=0;POSITION pos={FirstPage,0,0};
 checks++;FreeRepList();if(!BuildReplaceString(replacement,&length,&pos,2,0) || length!=2 || RepList.Buf[0]!='b' || RepList.Buf[1]!='c'){printf("FAIL root capture replacement\n");failures++;}FreeRepList();}
}
static void AlternativeContinuation(void){
 Expect("repeat-backtracks-for-prefix",L"/^aa*$/",(BYTE*)"aaa",3,0,TRUE);
 Expect("repeat-backtracks-for-group-prefix",L"/^a\\(a*\\|q\\)$/",(BYTE*)"aaa",3,0,TRUE);
 Expect("boundary-after-alternative",L"/\\(x\\|xy\\)\\> /",(BYTE*)"xy ",3,0,TRUE);
 Expect("boundary-before-alternative",L"/ \\<\\(x\\|yx\\)/",(BYTE*)" yx",3,0,TRUE);
 Expect("literal-suffix-overlap",L"/\\(x\\|xy\\)z/",(BYTE*)"xyz",3,0,TRUE);
 Expect("literal-prefix-overlap",L"/z\\(x\\|yx\\)/",(BYTE*)"zyx",3,0,TRUE);
 Expect("nested-suffix-overlap",L"/\\(\\(x\\|xy\\)\\|q\\)z/",(BYTE*)"xyz",3,0,TRUE);
 Expect("boundary-negative",L"/\\(x\\|xy\\)\\> /",(BYTE*)"xyz ",4,0,FALSE);
 Expect("suffix-capture",L"/\\(x\\|xy\\)z/",(BYTE*)"xyz",3,0,TRUE);
 {WCHAR replacement[]=L"\\1";WV_LONG length=0;POSITION pos={FirstPage,0,0};
 checks++;FreeRepList();if(!CheckParenPairs() || !BuildReplaceString(replacement,&length,&pos,3,0) || length!=2 || RepList.Buf[0]!='x' || RepList.Buf[1]!='y'){printf("FAIL branch capture replacement\n");failures++;}FreeRepList();}
}
static void ContinuationOracle(void){
 BYTE text[10],page[4098];int length,bits,i;BOOL yes;
 for(length=1;length<=6;length++)for(bits=0;bits<(1<<length);bits++){
  for(i=0;i<length;i++)text[i]=(bits&(1<<i))?'b':'a';
  yes=text[0]=='a';for(i=1;i<length;i++)if(text[i]!='b')yes=FALSE;
  Expect("overlap-oracle",L"/^\\(a\\|ab\\)b*$/",text,length,0,yes);
  memmove(text+1,text,length);text[0]='z';text[length+1]=';';
  yes=TRUE;for(i=1;i<=length;i++)if(text[i]!='b')yes=FALSE;
  if(text[1]=='a' && (length==1 || (length==2 && text[2]=='b')))yes=TRUE;
  Expect("nested-oracle",L"/^z\\(\\(a\\|ab\\)\\|b*\\);$/",text,length+2,0,yes);
 }
 memset(page,'a',4095);memcpy(page+4095,"xy ",3);
 Expect("boundary-page",L"/^a*\\(x\\|xy\\)\\> /",page,sizeof(page),0,TRUE);
 {BYTE utf[]={0xce,0xbb,'y',' '};Expect("unicode-alternative",L"/\\(\x03bb\\|\x03bb" L"y\\)\\> /",utf,4,8,TRUE);}
 for(i=0;i<12;i++){
  WCHAR pattern[]=L"/a\\|\\(bc\\)/";INT error=0;
  allocationCountdown=i;BuildMatchList(pattern,'/',&error);checks++;
  if(allocationCountdown==-2 && MatchValid){printf("FAIL failed allocation accepted\n");failures++;}
  allocationCountdown=-1;
 }
}
static void SearchApi(void){
 BYTE text[]={0xef,0xbb,0xbf,'a',';','\n'};PAGE page={0};POSITION p;WCHAR pattern[]=L"/\\(.*\\|.*\\);/";INT error=0;BOOL result;
 page.PageBuf=text;page.Fill=sizeof(text);page.PageNo=1;FirstPage=LastPage=&page;
 CurrPos.p=ScreenStart.p=&page;CurrPos.i=ScreenStart.i=3;CurrPos.f=ScreenStart.f=0;
 UtfEncoding=8;CharSet=CS_UTF8;WrapScanFlag=FALSE;Interrupted=FALSE;StartTime=GetCurrentTime();
 BuildMatchList(pattern,'/',&error);p=CurrPos;result=SearchIt(&p,3);checks++;
 if(!result || p.i!=3 || EndMatch.i!=5){printf("FAIL repeat SearchIt: result=%d start=%d end=%d\n",result,p.i,EndMatch.i);failures++;}
}
static void SearchGuards(void){
 BYTE text[34];PAGE page={0};POSITION p;WCHAR pattern[100],*deep;INT error=0,i;
 memset(text,'a',sizeof(text));text[33]='c';page.PageBuf=text;page.Fill=sizeof(text);page.PageNo=1;
 FirstPage=LastPage=CurrPos.p=ScreenStart.p=&page;CurrPos.i=CurrPos.f=ScreenStart.i=ScreenStart.f=0;UtfEncoding=0;CharSet=CS_ANSI;
 wcscpy(pattern,L"/\\(");for(i=0;i<20;i++)wcscat(pattern,L"a*");wcscat(pattern,L"\\|q\\)b/");
 BuildMatchList(pattern,'/',&error);Interrupted=FALSE;interruptCountdown=1;p=CurrPos;checks++;
 if(CheckFwd(&p,FirstMatch) || !Interrupted || RecursiveBranch || ForwardBranchEnd || BackwardBranchStart || SearchDepth){printf("FAIL cancellation/unwind\n");failures++;}
 interruptCountdown=-1;Interrupted=FALSE;deep=malloc(24004*sizeof(WCHAR));if(!deep)abort();
 deep[0]='/';for(i=0;i<12000;i++){deep[1+2*i]='.';deep[2+2*i]='*';}deep[24001]='b';deep[24002]='/';deep[24003]=0;
 BuildMatchList(deep,'/',&error);p=CurrPos;checks++;
 if(CheckFwd(&p,FirstMatch) || !Interrupted || SearchDepth){printf("FAIL depth guard/unwind\n");failures++;}free(deep);Interrupted=FALSE;
 Expect("after-guard-recovery",L"/\\(x\\|xy\\)z/",(BYTE*)"xyz",3,0,TRUE);
}
int main(int argc,char **argv){extern BOOL SuppressStatusBeep;(void)argc;(void)argv;SuppressStatusBeep=TRUE;ByteClasses();RepeatAlternatives();RootAlternatives();AlternativeContinuation();ContinuationOracle();SearchApi();SearchGuards();printf("%s %d search regression checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;}
