/* Real local process/job integration, in addition to the mocked policy tests. */
#include "../Platform.c"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s error=%lu\n",__LINE__,#x,GetLastError());return 1;}}while(0)
int main(void){
 STARTUPINFOEXW startup={0};PROCESS_INFORMATION p={0};JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};
 HANDLE job,parent;WCHAR app[MAX_PATH],line[MAX_PATH+64];DWORD code;BOOL contained=FALSE;int i;
 CHECK(InitializeWindowsPlatform());CHECK(GetSystemDirectoryW(app,MAX_PATH));wcscat(app,L"\\cmd.exe");
 parent=CreateJobObjectW(NULL,NULL);CHECK(parent);CHECK(AssignProcessToJobObject(parent,GetCurrentProcess()));
 for(i=0;i<2;i++){
  job=CreateJobObjectW(NULL,NULL);CHECK(job);limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  CHECK(SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)));
  _snwprintf(line,MAX_PATH+64,L"\"%s\" /d /c %s",app,i?L"pause":L"exit 0");
  startup.StartupInfo.cb=sizeof(startup);
  CHECK(WvCreateJobProcessW(app,line,&startup.StartupInfo,job,&p));
  CHECK(IsProcessInJob(p.hProcess,job,&contained) && contained);
  if(!i){CHECK(ResumeThread(p.hThread)!=-1);CHECK(WaitForSingleObject(p.hProcess,5000)==WAIT_OBJECT_0);CHECK(GetExitCodeProcess(p.hProcess,&code) && !code);}
  /* The second child is killed while suspended; it never reads console input. */
  CHECK(CloseHandle(job));CHECK(WaitForSingleObject(p.hProcess,5000)==WAIT_OBJECT_0);
  CloseHandle(p.hThread);CloseHandle(p.hProcess);
 }
 CloseHandle(parent);puts("PASS actual Windows 11 nested jobs, suspended containment, normal exit and kill-on-close");return 0;
}
