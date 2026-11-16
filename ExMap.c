/* Ex front end for the shared GUI keyboard mapping table. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>
#include "winvi.h"
#include "map.h"
#define EX_KEY WV_MAP_KEYDOWN
#define EX_CHARKEY WV_MAP_CHAR
#define EX_CTRL WV_MAP_CTRL
#define EX_SHIFT WV_MAP_SHIFT
#define EX_ALT WV_MAP_ALT
extern HWND hwndSettings;
static BOOL MapError(LPCWSTR text){NewStatus(0,(PWSTR)text,NS_ERROR);return FALSE;}
static DWORD NamedKey(LPCWSTR text,size_t n)
{
 struct {LPCWSTR name;UINT key;BOOL character;} names[]={
  {L"Esc",27,TRUE},{L"CR",13,TRUE},{L"Enter",13,TRUE},{L"Tab",9,TRUE},{L"BS",8,FALSE},
  {L"Space",32,TRUE},{L"lt",'<',TRUE},{L"Del",VK_DELETE,FALSE},{L"Insert",VK_INSERT,FALSE},
  {L"Home",VK_HOME,FALSE},{L"End",VK_END,FALSE},{L"Up",VK_UP,FALSE},{L"Down",VK_DOWN,FALSE},
  {L"Left",VK_LEFT,FALSE},{L"Right",VK_RIGHT,FALSE},{L"PageUp",VK_PRIOR,FALSE},{L"PageDown",VK_NEXT,FALSE}};
 DWORD modifiers=0,t;size_t i;
 while(n>=2 && text[1]==L'-'){
  WCHAR c=(WCHAR)towupper(*text);if(c==L'C')modifiers|=EX_CTRL;else if(c==L'S')modifiers|=EX_SHIFT;else if(c==L'A')modifiers|=EX_ALT;else break;
  text+=2;n-=2;
 }
 for(i=0;i<sizeof(names)/sizeof(names[0]);i++)if(wcslen(names[i].name)==n && !_wcsnicmp(text,names[i].name,n)){
  if(names[i].character && modifiers && names[i].key!='<')return EX_KEY|names[i].key|modifiers;
  t=names[i].character?WvMapChar((WCHAR)names[i].key):EX_KEY|names[i].key;goto found;
 }
 if(n>=2 && n<=3 && towupper(text[0])==L'F'){
  UINT f=0;for(i=1;i<n;i++){if(text[i]<L'0'||text[i]>L'9')return 0;f=f*10+text[i]-L'0';}
  if(f>=1 && f<=24){t=EX_KEY|(VK_F1+f-1);goto found;}
 }
 if(n!=1 || text[0]>255)return 0;t=WvMapChar(text[0]);
found:
 if((modifiers&EX_CTRL) && (t&EX_CHARKEY)){
  UINT c=t&255;if(c=='?' && !(modifiers&(EX_ALT|EX_SHIFT)))return WvMapChar(127);
  if(c==' ')c='@';if(c>='a' && c<='z')c-=32;
  if(c<'@' || c>'_')return 0;t=EX_CHARKEY|c;
 }
 if((t&EX_CHARKEY) && (modifiers&EX_SHIFT) && (t&255)>='a' && (t&255)<='z')t-=32;
 if((t&EX_CHARKEY) && modifiers==(EX_CTRL|EX_ALT) && (t&255)=='V')t=EX_KEY|'V';
 return t|modifiers;
}
/* Literal ex characters, Ctrl+V quoting, and explicit <key> names. */
static BOOL ParseKeys(PWSTR *cursor,BOOL left,DWORD **out)
{
 PWSTR p=*cursor;size_t used=0,capacity=wcslen(p)+1;DWORD *tokens=calloc(capacity,sizeof(DWORD));
 if(!tokens)return MapError(L"Not enough memory for mapping keys.");
 if(left && p[0]==L'#' && p[1]>=L'1' && p[1]<=L'9'){
  PWSTR end=p+1;UINT f=0;while(*end>=L'0' && *end<=L'9'){if(f>24)break;f=f*10+*end++-L'0';}
  if(f>=1 && f<=24 && (!*end || *end==L' ' || *end==L'\t')){tokens[used++]=EX_KEY|(VK_F1+f-1);p=end;goto parsed;}
 }
 while(*p && (!left || (*p!=L' ' && *p!=L'\t'))){
  DWORD t=0;
  if(*p==22){if(!p[1]){free(tokens);return MapError(L"Ctrl+V requires a following character.");}p++;t=WvMapChar(*p++);}
  else if(*p==L'<'){
   PWSTR end=wcschr(p+1,L'>');
   if(end && end-p<=32){
    if(end-p==4 && !_wcsnicmp(p+1,L"Nop",3)){p=end+1;continue;}
    t=NamedKey(p+1,end-p-1);if(t)p=end+1;
   }
   if(!t)t=WvMapChar(*p++);
  }else t=WvMapChar(*p++);
  tokens[used++]=t;
 }
parsed:
 if(left && (!used || used>63)){free(tokens);return MapError(L"Mapping input requires 1-63 key messages.");}
 for(size_t i=0;i<used;i++)if(tokens[i]&WV_MAP_UNICODE){
  UINT c=tokens[i]&0xffff;
  if(c>=0xd800 && c<=0xdbff){if(i+1>=used || !(tokens[i+1]&WV_MAP_UNICODE) || (tokens[i+1]&0xffff)<0xdc00 || (tokens[i+1]&0xffff)>0xdfff){free(tokens);return MapError(L"Invalid Unicode mapping key.");}i++;}
  else if(c>=0xdc00 && c<=0xdfff){free(tokens);return MapError(L"Invalid Unicode mapping key.");}
 }
 *cursor=p;*out=tokens;return TRUE;
}
/* Saved profiles use the same ASCII key notation as Settings, including new
   U+hhhh Unicode and V+hh virtual-key escapes. No binary profile migration. */
static LPSTR NativeKeys(const DWORD *keys)
{
 size_t n=WvMapLength(keys),used=0;LPSTR text;char part[64];
 if(n>(SIZE_MAX-1)/32)return NULL;text=malloc(n*32+1);if(!text)return NULL;
 for(size_t i=0;i<n;i++){
  DWORD t=keys[i];part[0]=0;
  if(t&WV_MAP_UNICODE)sprintf(part,"U+%04X ",(UINT)(t&0xffff));
  else{
   if(t&EX_CTRL)strcat(part,"Ctrl+");if(t&EX_SHIFT)strcat(part,"Shift+");if(t&EX_ALT)strcat(part,"Alt+");
   if(t&EX_KEY)sprintf(part+strlen(part),"V+%02X ",(UINT)(t&255));
   else if((t&255)>=127)sprintf(part+strlen(part),"U+%04X ",(UINT)(t&255));
   else{size_t k=strlen(part);char quote=(t&255)=='"'?'\'':'"';part[k]=quote;part[k+1]=(char)t;part[k+2]=quote;part[k+3]=' ';part[k+4]=0;}
  }
  size_t length=strlen(part);if(length>=WV_MAP_TEXT_CAP-used){free(text);return NULL;}memcpy(text+used,part,length);used+=length;
 }
 text[used]=0;return text;
}
static BOOL SameKind(PMAP m,BOOL abbreviation,DWORD modes)
{return ((m->Flags&M_ABBREVIATION)!=0)==abbreviation && (m->Flags&modes)!=0;}
static BOOL ListMappings(const DWORD *input,BOOL abbreviation,DWORD modes)
{
 size_t count=0,at=0;PMAP m;WCHAR *text;
 for(m=FirstMapping;m;m=m->Next)if(SameKind(m,abbreviation,modes) && (!input || IsMappingEqual(m->InputMatch,(PDWORD)input))){
  size_t n=strlen(m->InputDisplay)+strlen(m->Replace)+80;if(n>SIZE_MAX/sizeof(WCHAR)-count-1)return MapError(L"Mapping list is too large.");count+=n;if(count>=INT_MAX)return MapError(L"Mapping output exceeds the window limit.");
 }
 text=calloc(count+1,sizeof(WCHAR));if(!text)return MapError(L"Not enough memory for mapping list.");
 for(m=FirstMapping;m;m=m->Next)if(SameKind(m,abbreviation,modes) && (!input || IsMappingEqual(m->InputMatch,(PDWORD)input))){
  int n=MultiByteToWideChar(CP_ACP,0,m->InputDisplay,-1,text+at,(int)(count-at+1));if(!n)goto fail;at+=n-1;text[at++]=L'\t';
  n=MultiByteToWideChar(CP_ACP,0,m->Replace,-1,text+at,(int)(count-at+1));if(!n)goto fail;at+=n-1;text[at++]=13;text[at++]=10;text[at]=0;
 }
 if(!WvExOutput(text))goto fail;free(text);return TRUE;
fail:free(text);return MapError(L"Cannot display mappings.");
}
BOOL WvExMapping(PWSTR args,BOOL remove,BOOL inputMode,BOOL abbreviation)
{
 DWORD *left=NULL,*right=NULL,modes=inputMode?M_INSERTMODE|M_COMMANDLINE:M_COMMANDMODE;PMAP entry=NULL,m,*link;BOOL found=FALSE;
 if(WvExGlobalActive || ExecRunning())return MapError(L"Mappings cannot change during a global/input or filtered command.");
 while(*args==L' ' || *args==L'\t')args++;
 if(!*args){if(remove)return MapError(L"Unmap requires an input key sequence.");return ListMappings(NULL,abbreviation,modes);}
 if(!ParseKeys(&args,TRUE,&left))return FALSE;
 while(*args==L' ' || *args==L'\t')args++;
 if(!*args && !remove){BOOL ok=ListMappings(left,abbreviation,modes);free(left);return ok;}
 if(remove && *args){free(left);return MapError(L"Unmap accepts only its input key sequence.");}
 if(hwndSettings && IsWindow(hwndSettings)){free(left);return MapError(L"Close Settings before changing mappings.");}
 if(!remove){
  char name[80];UINT serial=0;BOOL used;
  if(!ParseKeys(&args,FALSE,&right)){free(left);return FALSE;}
  entry=calloc(1,sizeof(*entry));if(!entry)goto memory;
  entry->InputMatch=left;left=NULL;entry->ReplaceMap=right;right=NULL;
  entry->InputDisplay=NativeKeys(entry->InputMatch);entry->Replace=NativeKeys(entry->ReplaceMap);
  do{sprintf(name,"ex %s %u",inputMode?"input":"normal",++serial);used=FALSE;for(m=FirstMapping;m;m=m->Next)if(!strcmp(m->Name,name)){used=TRUE;break;}}while(used && serial);
  if(!serial || !AllocStringA(&entry->Name,name) || !entry->InputDisplay || !entry->Replace)goto memory;
  entry->Flags=modes|(abbreviation?M_ABBREVIATION:0);
 }
 /* Prepare every allocation before touching the shared table; snapshot expansion
    before freeing any replacement which the dequeue engine may reference. */
 if(remove){for(m=FirstMapping;m;m=m->Next)if(SameKind(m,abbreviation,modes) && IsMappingEqual(m->InputMatch,left)){found=TRUE;break;}if(!found){free(left);return MapError(L"No matching mapping in this mode.");}}
 if(!WvMapPrepareChange())goto memory;
 for(link=&FirstMapping;(m=*link)!=NULL;){
  if(SameKind(m,abbreviation,modes) && IsMappingEqual(m->InputMatch,remove?left:entry->InputMatch)){
   m->Flags&=~modes;
   if(!(m->Flags&(M_INSERTMODE|M_COMMANDMODE|M_COMMANDLINE))){*link=m->Next;m->Next=NULL;FreeMappings(m);continue;}
  }
  link=&m->Next;
 }
 if(entry){entry->Next=FirstMapping;FirstMapping=entry;}
 free(left);return TRUE;
memory:free(left);free(right);FreeMappings(entry);return MapError(L"Not enough memory or mapping text exceeds the Settings limit.");
}
BOOL WvExMap(PWSTR args,BOOL remove,BOOL inputMode){return WvExMapping(args,remove,inputMode,FALSE);}
