#include <windows.h>
#include <stdio.h>
#include <string.h>
static void Record(const char *name){FILE *f=fopen(name,"wb");if(f){fprintf(f,"%lu",GetCurrentProcessId());fclose(f);}}
int main(int argc,char **argv){
 DWORD n;char bytes[4096];HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);
 if(argc<2)return 2;
 if(!strcmp(argv[1],"emit")){WriteFile(out,"OUT\r\n",5,&n,NULL);return 0;}
 if(!strcmp(argv[1],"echo")){while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),bytes,sizeof(bytes),&n,NULL)&&n){DWORD written;if(!WriteFile(out,bytes,n,&written,NULL))break;}return 0;}
 if(argc>2)Record(argv[2]);
 if(!strcmp(argv[1],"tree")){
  char self[32768],line[33000];STARTUPINFOA si={sizeof(si)};PROCESS_INFORMATION pi;
  GetModuleFileNameA(NULL,self,sizeof(self));_snprintf(line,sizeof(line),"\"%s\" wait %s",self,argc>3?argv[3]:"child.pid");
  if(!CreateProcessA(self,line,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi))return 3;
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }
 if(!strcmp(argv[1],"flood")){Sleep(300);memset(bytes,'f',sizeof(bytes));for(;;)if(!WriteFile(out,bytes,sizeof(bytes),&n,NULL))break;return 0;}
 Sleep(INFINITE);return 0;
}
