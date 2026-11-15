#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#define CommandExec SecurityCaptureCommand
#define EditCmd SecurityCaptureEdit
#include "../Tags.c"
#undef CommandExec
#undef EditCmd
extern PWSTR Compare(PWSTR,PWSTR,INT,INT);
extern INT CmdMask;
extern BOOL BuildShellCommandW(LPCSTR,LPCSTR,PCWSTR,PWSTR*,PWSTR,DWORD);
int SecurityUNC(void);int SecurityTemp(void);int SecurityPeer(void);int SecurityBMP(void);
static WCHAR captured[WV_TEXT_CAP],opened[WV_PATH_CAP];static int dispatches,opens;static BOOL editAllowed=TRUE;
BOOL SecurityCaptureCommand(PWSTR cmd){WvCopyString(captured,cmd);dispatches++;return TRUE;}
BOOL SecurityCaptureEdit(PCWSTR name){WvCopyString(opened,name);opens++;return editAllowed;}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int TagsCases(void) {
 const char *entries[]={"1","/^demo$/;\"\tkind:f","?demo?","!echo SECURITY_REVIEW_SENTINEL","write victim.txt","1|!echo bad","/demo/!echo bad","999999999999999999999999","/^unterminated","/demo/;\"!echo ignored metadata"};
 const int allowed[]={1,1,1,0,0,0,0,0,0,1};int i;WCHAR tagsName[]=L"security-tags.tmp";
 Tags=tagsName;
 for(i=0;i<10;i++) {
  FILE *f=fopen("security-tags.tmp","wb");WCHAR id[]=L"demo";BOOL ok;CHECK(f);
  fprintf(f,"demo\tfixture.txt\t%s\n",entries[i]);fclose(f);captured[0]=opened[0]=0;dispatches=opens=0;
  ok=Tag(id,0);CHECK(ok==allowed[i]);CHECK(dispatches==allowed[i]);CHECK(opens==0);
  if(allowed[i])CHECK(captured[0]==L':' && captured[1]!=L'!');
 }
 for(i=0;i<3;i++) {
  FILE *f=fopen("security-tags.tmp","wb");WCHAR id[]=L"demo";int j;CHECK(f);
  fputs("demo\t",f);if(i==1)for(j=0;j<40000;j++)fputc('a',f);else fputs("fixture.txt",f);
  fputc('\t',f);if(i==2)fputc('/',f);for(j=0;j<35000;j++)fputc('A',f);fputc('\n',f);fclose(f);
  dispatches=opens=0;CHECK(!Tag(id,0));CHECK(!dispatches && !opens);
 }
 {FILE *f=fopen("security-tags.tmp","wb");WCHAR id[]=L"demo";CHECK(f);fputs("demo\t\"folder name/target.txt\"\t1\n",f);fclose(f);dispatches=opens=0;CHECK(Tag(id,0));CHECK(opens==1 && !wcscmp(opened,L"folder name/target.txt"));}

 {static WCHAR longName[WV_PATH_CAP];int j;for(j=0;j<WV_PATH_CAP-1;j++)longName[j]=(j&1)?L'%':L'#';longName[WV_PATH_CAP-1]=0;
  while(!IsTagStackEmpty())PopTag(FALSE);CurrFile=longName;PushTag();CurrFile=L"fixture.txt";opens=dispatches=0;editAllowed=FALSE;
  PopTag(TRUE);CHECK(opens==1 && !dispatches && !wcscmp(opened,longName));editAllowed=TRUE;PopTag(FALSE);CHECK(IsTagStackEmpty());}
 {unsigned state=0x713a42;int trial,j;WCHAR input[80],out[80];const WCHAR alphabet[]=L"0123456789/?!;|\\\" ab";
  for(trial=0;trial<20000;trial++){int n=trial%79;for(j=0;j<n;j++){state=state*1664525u+1013904223u;input[j]=alphabet[state%(_countof(alphabet)-1)];}input[n]=0;
   if(ParseTagAddress(input,out,_countof(out)))CHECK(out[0]==L':' && ((out[1]>=L'0'&&out[1]<=L'9')||out[1]==L'/'||out[1]==L'?') && wcslen(out)<_countof(out));}}
 {WCHAR list[]=L"security-missing.tags,security-tags-dir/list.tags";WCHAR id[]=L"demo";FILE *f;
  CHECK(CreateDirectoryW(L"security-tags-dir",NULL)||GetLastError()==ERROR_ALREADY_EXISTS);
  f=fopen("security-tags-dir/list.tags","wb");CHECK(f);fputs("demo\tname%# with spaces.txt\t1\n",f);fclose(f);
  /* Quoting preserves spaces while the direct EditCmd call preserves % and #. */
  f=fopen("security-tags-dir/list.tags","wb");CHECK(f);fputs("demo\t\"name%# with spaces.txt\"\t1\n",f);fclose(f);
  Tags=list;opens=dispatches=0;CHECK(Tag(id,0));CHECK(opens==1&&dispatches==1&&!wcscmp(opened,L"security-tags-dir/name%# with spaces.txt"));
  DeleteFileW(L"security-tags-dir/list.tags");RemoveDirectoryW(L"security-tags-dir");Tags=tagsName;
 }
 DeleteFileW(L"security-tags.tmp");puts("PASS tag command rejection, valid addresses, quoted/relative paths, maximum return path and 20000 parser cases");return 0;
}
static int PercentCases(void) {
 static WCHAR name[1025];int i,n;PWSTR result;for(i=0;i<1024;i++)name[i]='a';name[1024]=0;CurrFile=name;
 for(n=1;n<=80;n++) {
  WvCopyString(CommandBuf,L"edit ");for(i=0;i<n;i++)wcscat(CommandBuf,L"% ");CmdMask=0;
  result=Compare(CommandBuf,L"edit",1,0x1c);if(result)CHECK(wcslen(result)<WV_PATH_CAP);
  if(n>=33)CHECK(!result);
 }
 WvCopyString(CommandBuf,L"edit %%");CmdMask=0;result=Compare(CommandBuf,L"edit",1,0x1c);CHECK(result && !wcscmp(result,L"%"));
 CurrFile=L"fixture.txt";puts("PASS bounded filename expansion across 80 expansion counts and escaped percent");return 0;
}
static int ShellCases(void) {
 WCHAR app[WV_PATH_CAP],system[WV_PATH_CAP],*line=NULL;STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;DWORD code;HANDLE h;
 CHECK(GetSystemDirectoryW(system,WV_PATH_CAP));wcscat(system,L"\\cmd.exe");
 h=CreateFileW(L"cmd.exe",GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);CHECK(h!=INVALID_HANDLE_VALUE);CloseHandle(h);
 CHECK(BuildShellCommandW("cmd.exe","%0 /c %1",L"exit 0",&line,app,WV_PATH_CAP));CHECK(!_wcsicmp(app,system));CHECK(line[0]==L'"' && wcsstr(line,L" /d"));
 si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
 CHECK(CreateProcessW(app,line,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi));CHECK(WaitForSingleObject(pi.hProcess,5000)==WAIT_OBJECT_0);CHECK(GetExitCodeProcess(pi.hProcess,&code)&&code==0);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);free(line);line=NULL;DeleteFileW(L"cmd.exe");
 CHECK(BuildShellCommandW("\"C:\\Folder With Spaces\\shell.exe\"","\"%0\" -c %1",L"trusted command",&line,app,WV_PATH_CAP));CHECK(line[0]==L'"' && wcsstr(line,L"Folder With Spaces"));free(line);line=NULL;
 CHECK(!BuildShellCommandW("cmd.exe","unsafe-prefix %1",L"exit 0",&line,app,WV_PATH_CAP));
 puts("PASS explicit shell image, working-directory shadow rejection, quoted paths and actual child launch");return 0;
}
int SecurityTempCrashChild(void);int SecurityTempCrash(void);
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--temp-crash-child"))return SecurityTempCrashChild();
 PAGE page={0};BYTE data[32];memset(data,'a',sizeof(data));page.PageBuf=data;page.Fill=sizeof(data);page.PageNo=1;
 FirstPage=LastPage=CurrPos.p=ScreenStart.p=&page;CurrFile=L"fixture.txt";hwndMain=NULL;
 CHECK(!TagsCases());CHECK(!PercentCases());CHECK(!SecurityUNC());puts("PASS UNC allocation and separator restoration (network APIs mocked)");
 CHECK(!SecurityPeer());CHECK(!SecurityTemp());CHECK(!SecurityTempCrash());CHECK(!SecurityBMP());CHECK(!ShellCases());
 puts("PASS all security regression groups; no untrusted shell command was executed");return 0;
}
