/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2010 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *     15-Nov-2002	tag back in same file with NewPosition() (uses scrolling)
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 *     10-Jan-2009	fixed identifier extraction after multi-byte UTF-8 chars
 *     12-Nov-2009	better check of Ctrl+Break interrupts when tagging
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"

static PWSTR TagsFile;
static PCWSTR ActiveTagsName;
static size_t ActiveTagsLength;
static WCHAR TagCommand[WV_PATH_CAP];

typedef struct tagTAGSTACK {
	struct tagTAGSTACK FAR *Next;
	WV_ULONG					Pos;
	WCHAR					FName[1];
} TAGSTACK, FAR *LPTAGSTACK;
static LPTAGSTACK TagStack = NULL;

static VOID PushTag(VOID)
{	LPTAGSTACK SavePos;

	if (CurrFile == NULL || !*CurrFile) return;
	SavePos = _fcalloc(sizeof(TAGSTACK) + wcslen(CurrFile)*sizeof(WCHAR), 1);
	if (SavePos == NULL) return;
	SavePos->Next = TagStack;
	SavePos->Pos  = CountBytes(&CurrPos);
	WvCopyString(SavePos->FName, CurrFile);
	TagStack = SavePos;
}

VOID PopTag(BOOL Jump)
{
	if (TagStack != NULL) {
		LPTAGSTACK ToFree;

		if (Jump) {
			if (wcsicmp(TagStack->FName, CurrFile) != 0) {
				if (!EditCmd(TagStack->FName)) return;
				JumpAbsolute(TagStack->Pos);
				NewScrollPos();
			} else {
				POSITION p;

				p.p = FirstPage;
				while (p.p->Next && TagStack->Pos > p.p->Fill) {
					TagStack->Pos -= p.p->Fill;
					p.p = p.p->Next;
				}
				if (TagStack->Pos > p.p->Fill) p.i = p.p->Fill;
				else p.i = (short)TagStack->Pos;
				p.f = 0;
				NewPosition(&p);
			}
			if (!HexEditMode) Position(1, '^', 0);	/*position to first non-space*/
			else ShowEditCaret();					/*show caret only*/
		}
		ToFree   = TagStack;
		TagStack = TagStack->Next;
		_ffree(ToFree);
	} else Error(330);
}

BOOL IsTagStackEmpty(VOID)
{
	return (TagStack == NULL);
}

static HANDLE OpenNextTagsFile(void)
{
 WCHAR path[WV_PATH_CAP];PWSTR p;
 while(*TagsFile==L',' || *TagsFile==L';')TagsFile++;
 ActiveTagsName=TagsFile;p=TagsFile;
 while(*p && *p!=L',' && *p!=L';')p++;
 ActiveTagsLength=p-TagsFile;TagsFile=*p?p+1:p;
 if(!ActiveTagsLength || ActiveTagsLength>=WSIZE(path))return INVALID_HANDLE_VALUE;
 memcpy(path,ActiveTagsName,ActiveTagsLength*sizeof(WCHAR));path[ActiveTagsLength]=0;
 return CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
}

static INT Fill;

static INT NextChar(HFILE hf)
{	static CHAR	Buf[128];
	static INT	Index;

	if (Index >= Fill) {
		Index = 0;
		if ((Fill = _lread(hf, Buf, sizeof(Buf))) <= 0) {
			Fill = 0;
			return ('\0');
		}
	}
	return (Buf[Index++]);
}

static BOOL CombinePath(PWSTR buffer, size_t cap)
{
 PCWSTR start=ActiveTagsName,end=start+ActiveTagsLength;size_t prefix,len=wcslen(buffer);
 while(end>start && end[-1]!=L'/' && end[-1]!=L'\\')end--;
 prefix=(size_t)(end-start);
 if(prefix && (len>=cap || prefix>=cap-len))return FALSE;
 if(prefix){memmove(buffer+prefix,buffer,(len+1)*sizeof(WCHAR));memcpy(buffer,start,prefix*sizeof(WCHAR));}
 return TRUE;
}

/* Project tags may contain a location, never a general Ex/shell command. */
static BOOL ParseTagAddress(PCWSTR source,PWSTR out,size_t cap)
{
 PCWSTR p=source,start=source;WV_ULONG number=0;WCHAR delimiter;
 if(*p>=L'0' && *p<=L'9') {
  do {unsigned digit=*p++-L'0';if(number>(WV_MAX_POSITION-digit)/10)return FALSE;number=number*10+digit;}while(*p>=L'0'&&*p<=L'9');
 } else if(*p==L'/' || *p==L'?') {
  delimiter=*p++;
  for(;;){if(!*p)return FALSE;if(*p==L'\\'){if(!*++p)return FALSE;p++;continue;}if(*p++==delimiter)break;}
 } else return FALSE;
 {size_t n=p-start;if(n+2>cap)return FALSE;out[0]=L':';memcpy(out+1,start,n*sizeof(WCHAR));out[n+1]=0;}
 while(*p==L' ' || *p==L'\t')p++;
 return !*p || (p[0]==L';' && p[1]==L'"');
}

extern WCHAR BufferW[WV_TEXT_CAP];
BOOL Tag(PWSTR identifier, WORD flags)
{
 CHAR raw[4*WV_PATH_CAP+128];WCHAR line[WV_TEXT_CAP],fileName[WV_PATH_CAP];
 HFILE hf;INT c;size_t used;WV_ULONG scanned;BOOL tooLong;
 if(!identifier || !*identifier || !Tags)return FALSE;
 if(flags&2){_snwprintf(BufferW,WSIZE(BufferW)-1,L":tag%s %s",flags&1?L"!":L"",identifier);BufferW[WSIZE(BufferW)-1]=0;NewStatus(0,BufferW,NS_BUSY);}
 TagsFile=Tags;StartInterruptCheck();
 while(!Interrupted && *TagsFile) {
  hf=(HFILE)OpenNextTagsFile();if(hf==HFILE_ERROR)continue;Fill=0;
  for(;;) {
   used=0;scanned=0;tooLong=FALSE;
   while((c=NextChar(hf))!=0 && c!='\n') {
    if(used<sizeof(raw)-1)raw[used++]=(CHAR)c;else tooLong=TRUE;
    if((++scanned&4095)==0 && CheckInterrupt())break;
   }
   if(Interrupted)break;
   if(used && raw[used-1]=='\r')used--;raw[used]=0;
   if(!tooLong && used) {
    WCHAR *p,*name,*address,*end;BOOL quoted=FALSE;size_t length;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,raw,-1,line,WSIZE(line)) &&
       !MultiByteToWideChar(CP_ACP,0,raw,-1,line,WSIZE(line)))goto next_line;
    p=line;while(*p && *p!=L' ' && *p!=L'\t')p++;if(!*p)goto next_line;*p++=0;
    if(wcscmp(line,identifier))goto next_line;
    while(*p==L' '||*p==L'\t')p++;
    if(*p==L'"'){quoted=TRUE;p++;}name=p;
    while(*p && (quoted ? *p!=L'"' : (*p!=L' '&&*p!=L'\t')))p++;
    end=p;if(quoted && *p!=L'"')goto invalid_tag;
    if(*p)p++;else goto invalid_tag;*end=0;
    while(*p==L' '||*p==L'\t')p++;address=p;
    length=wcslen(name);
    if(!length || length>=WSIZE(fileName) || !ParseTagAddress(address,TagCommand,WSIZE(TagCommand)))goto invalid_tag;
    WvCopyString(fileName,name);
    if(fileName[0]!=L'/' && fileName[0]!=L'\\' && fileName[1]!=L':')
      if(!CombinePath(fileName,WSIZE(fileName)))goto invalid_tag;
    _lclose(hf);PushTag();
    if(wcsicmp(fileName,CurrFile)) {
     BOOL saved=CommandWithExclamation,ok;CommandWithExclamation=(flags&1)!=0;
     ok=EditCmd(fileName);CommandWithExclamation=saved;
     if(!ok){Enable();PopTag(FALSE);return FALSE;}
    }
    {
     BOOL magic=MagicFlag,ignore=IgnoreCaseFlag,wrap=WrapScanFlag,ok;
     if(!SaveMatchList()){Enable();NewStatus(0,L"Not enough memory to follow tag.",NS_ERROR);return FALSE;}
     MagicFlag=IgnoreCaseFlag=FALSE;WrapScanFlag=TRUE;
     ok=CommandExec(TagCommand);
     MagicFlag=magic;IgnoreCaseFlag=ignore;WrapScanFlag=wrap;RestoreMatchList();Enable();ShowEditCaret();
     if(ok && (flags&2))NewStatus(0,L"",NS_NORMAL);return ok;
    }
   }
next_line:
   if(!c || CheckInterrupt())break;
  }
  _lclose(hf);continue;
invalid_tag:
  _lclose(hf);Enable();NewStatus(0,L"Unsafe, invalid or oversized tag entry was rejected.",NS_ERROR);return FALSE;
 }
 Enable();ShowEditCaret();Error(Interrupted?218:329);return FALSE;
}

PWSTR ExtractIdentifier(PPOSITION pPos)
{	PWSTR		p = TagCommand;
	INT			c;
	POSITION	Pos;

	Pos = CurrPos;
	if (!(CharFlags(c = CharAt(&Pos)) & 8)) return (NULL);
	do {
		if (pPos != NULL) *pPos = Pos;
		if ((c = GoBackAndChar(&Pos)) == C_EOF) {
			*p++ = CharAt(&Pos);
			break;
		}
	} while (CharFlags(c) & 8);
	while (CharFlags(c = AdvanceAndChar(&Pos)) & 8)
		if (p-TagCommand < WSIZE(TagCommand)-1) *p++ = c;
	*p = '\0';
	return (*TagCommand ? TagCommand : NULL);
}

VOID TagCurrentPosition(VOID)
{	PWSTR p;

	if ((p = ExtractIdentifier(NULL)) != NULL) Tag(p, 2);
}
