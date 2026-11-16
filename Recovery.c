/* Durable, versioned recovery snapshots. GPL-2.0-or-later. */
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "Recovery.h"

#pragma pack(push,1)
typedef struct {
 BYTE magic[8];
 DWORD version,headerSize,pathUnits,encoding,characterSet,littleEndian;
 LONG newLine;
 DWORD newFile;
 WV_ULONG length,cursor;
 FILETIME sourceTime,created;
 BYTE digest[32];
} RECOVERY_HEADER;
#pragma pack(pop)
static const BYTE Magic[8]={'W','i','n','V','i','R','0','1'};
static volatile LONG64 LastCreated;
static void CreationTime(FILETIME *time)
{
 FILETIME system;LONG64 now,previous,next;GetSystemTimeAsFileTime(&system);memcpy(&now,&system,sizeof(now));
 /* Windows 7's clock can give several consecutive captures the same tick. */
 do{previous=InterlockedCompareExchange64(&LastCreated,0,0);next=now>previous?now:previous+1;}
 while(InterlockedCompareExchange64(&LastCreated,next,previous)!=previous);
 memcpy(time,&next,sizeof(*time));
}
typedef struct {BCRYPT_ALG_HANDLE algorithm;BCRYPT_HASH_HANDLE hash;BYTE *object;} HASH;
static void HashClose(HASH *h)
{if(h->hash)BCryptDestroyHash(h->hash);if(h->algorithm)BCryptCloseAlgorithmProvider(h->algorithm,0);free(h->object);memset(h,0,sizeof(*h));}
static BOOL HashStart(HASH *h)
{
 DWORD size,got;memset(h,0,sizeof(*h));
 if(BCryptOpenAlgorithmProvider(&h->algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0) ||
    BCryptGetProperty(h->algorithm,BCRYPT_OBJECT_LENGTH,(PBYTE)&size,sizeof(size),&got,0))goto fail;
 h->object=malloc(size);if(!h->object){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto fail;}
 if(BCryptCreateHash(h->algorithm,&h->hash,h->object,size,NULL,0,0))goto fail;
 return TRUE;
fail:HashClose(h);return FALSE;
}
static BOOL HashBytes(HASH *h,const void *p,DWORD n)
{if(BCryptHashData(h->hash,(PBYTE)p,n,0)){SetLastError(ERROR_CRC);return FALSE;}return TRUE;}
static BOOL Exact(HANDLE f,void *p,DWORD n,BOOL write)
{
 BYTE *b=p;DWORD done;
 while(n){done=0;if(!(write?WriteFile(f,b,n,&done,NULL):ReadFile(f,b,n,&done,NULL)))return FALSE;
  if(!done || done>n){SetLastError(write?ERROR_WRITE_FAULT:ERROR_HANDLE_EOF);return FALSE;}b+=done;n-=done;}
 return TRUE;
}
static BOOL Cancelled(WV_RECOVERY_CANCEL cancel,void *context)
{if(cancel && cancel(context)){SetLastError(ERROR_CANCELLED);return TRUE;}return FALSE;}
static BOOL ValidPath(PCWSTR p,DWORD n)
{
 DWORD i;for(i=0;i<n;i++){
  WCHAR c=p[i];if(!c)return FALSE;
  if(c>=0xd800 && c<=0xdbff){if(++i>=n || p[i]<0xdc00 || p[i]>0xdfff)return FALSE;}
  else if(c>=0xdc00 && c<=0xdfff)return FALSE;
 }
 return TRUE;
}
static BOOL ValidMeta(const RECOVERY_HEADER *h)
{
 return !memcmp(h->magic,Magic,8) && h->version==1 && h->headerSize==sizeof(*h) &&
  h->newFile<=1 && h->pathUnits<WV_RECOVERY_PATH_CAP-16 &&
  (h->encoding==0 || h->encoding==8 || h->encoding==16) && h->characterSet<=5 &&
  ((h->encoding==0 && (h->characterSet==0 || h->characterSet==1 || h->characterSet==5)) ||
   (h->encoding==8 && h->characterSet==2) || (h->encoding==16 && h->characterSet==(h->littleEndian?3:4))) &&
  h->littleEndian<=1 && h->length<=WV_MAX_POSITION-sizeof(*h)-2*(WV_ULONG)h->pathUnits &&
  h->cursor<=h->length && (h->encoding!=16 || !(h->length&1)) &&
  (h->newLine==-4 || h->newLine==0 || h->newLine==10 || h->newLine==13 || h->newLine==30);
}
/* An explicit user/SYSTEM DACL also protects captures of files whose source
   permissions are more restrictive than the containing directory. */
static PSECURITY_DESCRIPTOR PrivateSecurity(void)
{
 HANDLE token=NULL;DWORD size=0;TOKEN_USER *user=NULL;PWSTR sid=NULL,sddl=NULL;
 PSECURITY_DESCRIPTOR sd=NULL;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))goto done;
 GetTokenInformation(token,TokenUser,NULL,0,&size);
 if(!size || !(user=malloc(size)) || !GetTokenInformation(token,TokenUser,user,size,&size) ||
    !ConvertSidToStringSidW(user->User.Sid,&sid))goto done;
 size=(DWORD)wcslen(sid)+40;sddl=calloc(size,sizeof(WCHAR));if(!sddl)goto done;
 _snwprintf(sddl,size,L"D:P(A;;FA;;;SY)(A;;FA;;;%s)",sid);
 ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&sd,NULL);
done:if(token)CloseHandle(token);free(user);free(sddl);if(sid)LocalFree(sid);return sd;
}
static HANDLE Directory(PCWSTR path,PSECURITY_DESCRIPTOR sd)
{
 SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};HANDLE h;BY_HANDLE_FILE_INFORMATION info;
 if(sd && !CreateDirectoryW(path,&sa) && GetLastError()!=ERROR_ALREADY_EXISTS)return INVALID_HANDLE_VALUE;
 h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
  FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
 if(h!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(h,&info) ||
  !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))){
  CloseHandle(h);SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;
 }
 return h;
}
BOOL WvRecoverySave(PCWSTR directory,PCWSTR original,const WV_RECOVERY_META *meta,WV_ULONG length,
 WV_RECOVERY_READ read,void *readContext,WV_RECOVERY_CANCEL cancel,void *cancelContext,PWSTR output)
{
 RECOVERY_HEADER header={0};HASH hash={0};BYTE block[65536],random[16];
 WCHAR *part=NULL,*final=NULL;PSECURITY_DESCRIPTOR sd=NULL;HANDLE dir=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE;
 SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,FALSE};LARGE_INTEGER zero={0};DWORD got,error=ERROR_INVALID_DATA;
 BOOL ok=FALSE,owned=FALSE;WV_ULONG remaining=length;size_t n=wcslen(directory),units=wcslen(original);int attempt;
 static const WCHAR hex[]=L"0123456789abcdef";
 if(output)output[0]=0;
 memcpy(header.magic,Magic,8);header.version=1;header.headerSize=sizeof(header);
 header.pathUnits=(DWORD)units;header.encoding=meta->encoding;header.characterSet=meta->characterSet;
 header.littleEndian=meta->littleEndian;header.newLine=meta->newLine;header.length=length;header.cursor=meta->cursor;
 header.sourceTime=meta->sourceTime;CreationTime(&header.created);
 header.newFile=meta->newFile;
 if(units>=WV_RECOVERY_PATH_CAP-16 || n>=WV_RECOVERY_PATH_CAP-39 || !ValidMeta(&header) || !ValidPath(original,(DWORD)units)){
  SetLastError(ERROR_INVALID_DATA);goto done;
 }
 if(!(part=calloc(WV_RECOVERY_PATH_CAP,sizeof(WCHAR))) || !(final=calloc(WV_RECOVERY_PATH_CAP,sizeof(WCHAR)))){
  SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;
 }
 if(!(sd=PrivateSecurity()))goto done;
 dir=Directory(directory,sd);if(dir==INVALID_HANDLE_VALUE)goto done;
 sa.lpSecurityDescriptor=sd;
 for(attempt=0;attempt<16;attempt++){
  if(BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)){SetLastError(ERROR_GEN_FAILURE);goto done;}
  memcpy(part,directory,n*sizeof(WCHAR));part[n]=L'\\';
  for(size_t i=0;i<16;i++){part[n+1+2*i]=hex[random[i]>>4];part[n+2+2*i]=hex[random[i]&15];}
  wcscpy(part+n+33,L".part");wcscpy(final,part);wcscpy(final+n+33,L".wvr");
  file=CreateFileW(part,GENERIC_WRITE,0,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  if(file!=INVALID_HANDLE_VALUE){owned=TRUE;break;}
  if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)goto done;
 }
 if(file==INVALID_HANDLE_VALUE || !HashStart(&hash) || !HashBytes(&hash,&header,sizeof(header)) ||
  !HashBytes(&hash,original,(DWORD)units*2) || !Exact(file,&header,sizeof(header),TRUE) || !Exact(file,(void*)original,(DWORD)units*2,TRUE))goto done;
 while(remaining){
  DWORD want=remaining>sizeof(block)?sizeof(block):(DWORD)remaining;
  if(Cancelled(cancel,cancelContext) || !read(readContext,block,want,&got))goto done;
  if(!got || got>want){SetLastError(ERROR_READ_FAULT);goto done;}
  if(!HashBytes(&hash,block,got) || !Exact(file,block,got,TRUE))goto done;remaining-=got;
 }
 if(Cancelled(cancel,cancelContext) || BCryptFinishHash(hash.hash,header.digest,sizeof(header.digest),0) ||
  !SetFilePointerEx(file,zero,NULL,FILE_BEGIN) || !Exact(file,&header,sizeof(header),TRUE) || !FlushFileBuffers(file))goto done;
 if(!CloseHandle(file)){file=INVALID_HANDLE_VALUE;goto done;}file=INVALID_HANDLE_VALUE;
 if(!MoveFileExW(part,final,MOVEFILE_WRITE_THROUGH))goto done;
 owned=FALSE;ok=TRUE;if(output)wcscpy(output,final);
done:error=ok?ERROR_SUCCESS:GetLastError();if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 if(owned)DeleteFileW(part);if(dir!=INVALID_HANDLE_VALUE)CloseHandle(dir);
 if(sd)LocalFree(sd);free(part);free(final);HashClose(&hash);SetLastError(error);return ok;
}
void WvRecoveryClose(WV_RECOVERY *r)
{if(r->file && r->file!=INVALID_HANDLE_VALUE)CloseHandle(r->file);free(r->original);memset(r,0,sizeof(*r));r->file=INVALID_HANDLE_VALUE;}
BOOL WvRecoveryOpen(PCWSTR path,WV_RECOVERY *result,WV_RECOVERY_CANCEL cancel,void *context)
{
 WV_RECOVERY r={0};RECOVERY_HEADER h;HASH hash={0};BYTE digest[32],block[65536];
 LARGE_INTEGER size,offset;BY_HANDLE_FILE_INFORMATION info;BOOL ok=FALSE;DWORD error;WV_ULONG remaining;
 r.file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(r.file==INVALID_HANDLE_VALUE)goto done;
 if(GetFileType(r.file)!=FILE_TYPE_DISK || !GetFileInformationByHandle(r.file,&info) ||
  (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks!=1 ||
  !GetFileSizeEx(r.file,&size) || !Exact(r.file,&h,sizeof(h),FALSE))goto invalid;
 if(!ValidMeta(&h) || size.QuadPart!=(LONGLONG)(sizeof(h)+h.pathUnits*2+h.length))goto invalid;
 r.original=calloc((size_t)h.pathUnits+1,sizeof(WCHAR));if(!r.original){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
 if(!Exact(r.file,r.original,h.pathUnits*2,FALSE) || !ValidPath(r.original,h.pathUnits))goto invalid;
 memcpy(digest,h.digest,sizeof(digest));memset(h.digest,0,sizeof(h.digest));
 if(!HashStart(&hash) || !HashBytes(&hash,&h,sizeof(h)) || !HashBytes(&hash,r.original,h.pathUnits*2))goto done;
 remaining=h.length;
 while(remaining){DWORD n=remaining>sizeof(block)?sizeof(block):(DWORD)remaining;
  if(Cancelled(cancel,context) || !Exact(r.file,block,n,FALSE) || !HashBytes(&hash,block,n))goto done;remaining-=n;}
 if(BCryptFinishHash(hash.hash,h.digest,sizeof(h.digest),0) || memcmp(digest,h.digest,sizeof(digest)))goto invalid;
 offset.QuadPart=sizeof(h)+h.pathUnits*2;if(!SetFilePointerEx(r.file,offset,NULL,FILE_BEGIN))goto done;
 r.length=h.length;r.created=h.created;r.meta.encoding=h.encoding;r.meta.characterSet=h.characterSet;
 r.meta.littleEndian=h.littleEndian;r.meta.newLine=h.newLine;r.meta.cursor=h.cursor;r.meta.sourceTime=h.sourceTime;
 r.meta.newFile=h.newFile;
 *result=r;r.file=INVALID_HANDLE_VALUE;r.original=NULL;ok=TRUE;goto done;
invalid:SetLastError(ERROR_INVALID_DATA);
done:error=ok?ERROR_SUCCESS:GetLastError();HashClose(&hash);WvRecoveryClose(&r);SetLastError(error);return ok;
}
typedef struct {PWSTR path;FILETIME created;} CANDIDATE;
static int NewestFirst(const void *left,const void *right)
{
 const CANDIDATE *a=left,*b=right;int order=CompareFileTime(&b->created,&a->created);
 return order?order:wcscmp(b->path,a->path);
}
static BOOL SnapshotName(PCWSTR p)
{
 for(int i=0;i<32;i++)if(!((p[i]>=L'0' && p[i]<=L'9') || (p[i]>=L'a' && p[i]<=L'f')))return FALSE;
 return !wcscmp(p+32,L".wvr");
}
/* Only headers/names are read during discovery. Validate payloads in newest
   first order, falling back to an older intact capture after corruption. */
BOOL WvRecoveryFind(PCWSTR directory,PCWSTR original,WV_RECOVERY *result,WV_RECOVERY_CANCEL cancel,void *context)
{
 CANDIDATE *list=NULL;size_t count=0,capacity=0,n=wcslen(directory),i;
 WIN32_FIND_DATAW data;HANDLE dir=INVALID_HANDLE_VALUE,search=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE;
 WCHAR *pattern=NULL,*name=NULL,*stored=NULL;BOOL ok=FALSE;DWORD error=ERROR_FILE_NOT_FOUND;
 if(n>=WV_RECOVERY_PATH_CAP-39){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
 dir=Directory(directory,NULL);if(dir==INVALID_HANDLE_VALUE)return FALSE;
 pattern=calloc(WV_RECOVERY_PATH_CAP,sizeof(WCHAR));name=calloc(WV_RECOVERY_PATH_CAP,sizeof(WCHAR));
 stored=calloc(WV_RECOVERY_PATH_CAP,sizeof(WCHAR));
 if(!pattern || !name || !stored){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
 wcscpy(pattern,directory);wcscat(pattern,L"\\*.wvr");search=FindFirstFileW(pattern,&data);
 if(search==INVALID_HANDLE_VALUE)goto done;
 do{
  RECOVERY_HEADER h;LARGE_INTEGER size;
  if(Cancelled(cancel,context))goto done;
  if(wcslen(data.cFileName)!=36 || !SnapshotName(data.cFileName) ||
   (data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))continue;
  wcscpy(name,directory);wcscat(name,L"\\");wcscat(name,data.cFileName);
  file=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  if(file==INVALID_HANDLE_VALUE)continue;
  BOOL matching=GetFileSizeEx(file,&size) && Exact(file,&h,sizeof(h),FALSE) && ValidMeta(&h) &&
   size.QuadPart==(LONGLONG)(sizeof(h)+h.pathUnits*2+h.length) &&
   Exact(file,stored,h.pathUnits*2,FALSE) && ValidPath(stored,h.pathUnits);
  CloseHandle(file);file=INVALID_HANDLE_VALUE;
  if(!matching)continue;stored[h.pathUnits]=0;
  if(CompareStringOrdinal(stored,-1,original,-1,TRUE)!=CSTR_EQUAL)continue;
  if(count==capacity){
   size_t next=capacity?capacity*2:16;CANDIDATE *grown;
   if(next<=capacity || next>SIZE_MAX/sizeof(*list) || !(grown=malloc(next*sizeof(*list)))){
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;
   }
   if(count)memcpy(grown,list,count*sizeof(*list));free(list);list=grown;capacity=next;
  }
  list[count].path=malloc((wcslen(name)+1)*sizeof(WCHAR));if(!list[count].path){SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
  wcscpy(list[count].path,name);list[count++].created=h.created;
 }while(FindNextFileW(search,&data));
 if(GetLastError()!=ERROR_NO_MORE_FILES)goto done;
 if(count>1)qsort(list,count,sizeof(*list),NewestFirst);
 for(i=0;i<count;i++){
  WV_RECOVERY candidate={0};if(Cancelled(cancel,context))goto done;
  if(WvRecoveryOpen(list[i].path,&candidate,cancel,context)){
   if(CompareStringOrdinal(candidate.original,-1,original,-1,TRUE)==CSTR_EQUAL){*result=candidate;ok=TRUE;goto done;}
   WvRecoveryClose(&candidate);
  }else if(GetLastError()==ERROR_CANCELLED || GetLastError()==ERROR_NOT_ENOUGH_MEMORY)goto done;
 }
 SetLastError(ERROR_FILE_NOT_FOUND);
done:error=ok?ERROR_SUCCESS:GetLastError();
 if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(search!=INVALID_HANDLE_VALUE)FindClose(search);
 if(dir!=INVALID_HANDLE_VALUE)CloseHandle(dir);for(i=0;i<count;i++)free(list[i].path);
 free(list);free(pattern);free(name);free(stored);SetLastError(error);return ok;
}
