/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *     22-Jul-2002	use of private myassert.h
 *     14-Nov-2002	fixed failure to count single lf at start of read buffer
 *	   22-Jun-2004	reload last buffer if CharAt() yields EOF
 *      2-Jul-2007	handling of UTF-16 and page overlapping CR-LF
 */

#include <windows.h>
/*#include <io.h>*/
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <bcrypt.h>
#include <sddl.h>
#include <wchar.h>
#include "myassert.h"
#include "winvi.h"
#include "page.h"

#if defined(WIN32)
#  define GLOBALHANDLE(p) GlobalHandle(p)
#else
#  define GLOBALHANDLE(p) (HGLOBAL)LOWORD(GlobalHandle(HIWORD(p)))
#endif

#define  ALLOC_PPAGE  LPPAGE
#define  PAGEALLOC(s) (LPPAGE)_fcalloc(1, s)
#define  PAGEFREE(p)  _ffree(p)

POSITION		MarkPos[27];
static HANDLE	TmpFile = INVALID_HANDLE_VALUE;
static WCHAR	TmpFileName[WV_PATH_CAP];
static CHAR		ErrorUseVirtual[170], ErrorNoGlobalMem[50];
static CHAR		ErrorFatal[30], ErrorPleaseSave[60];

BOOL EnvEntryMatches(LPSTR e, PSTR m)
{
	while (*e == ' ') ++e;
	while (*e == *m) {
		++e; ++m;
	}
	while (*e == ' ') ++e;
	if (*e == '=' && *m == '\0') {
		e += lstrlen(e);
		do; while (*--e == ' ');
		*++e = '\0';
		return (TRUE);
	}
	return (FALSE);
}

void GetSystemTmpDir(PWSTR Path, INT Len)
{
	#if defined(WIN32)
		GetTempPathW(Len, Path);
	#else
		static LPSTR TmpDir;

		if (!TmpDir) {
			TmpDir = GetDOSEnvironment();
			do {
				if (EnvEntryMatches(TmpDir, "TMP") ||
					EnvEntryMatches(TmpDir, "TEMP"))
						break;
				TmpDir += lstrlen(TmpDir) + 1;
			} while (*TmpDir);
			if (*TmpDir) {
				do; while (*TmpDir++ != '=');
				while (*TmpDir == ' ') ++TmpDir;
			}
		}
		lstrcpyn(Path, TmpDir, Len);
	#endif
}

BOOL CreateTmpFile(void)
{
 PSECURITY_DESCRIPTOR sd=NULL;SECURITY_ATTRIBUTES sa;DWORD n,error=ERROR_CANNOT_MAKE;size_t len,i;int attempt;BYTE random[16];
 static const WCHAR hex[]=L"0123456789abcdef";
 if(SessionEnding)return FALSE;
 if(!DefaultTmpFlag && TmpDirectory) {
  len=wcslen(TmpDirectory);if(len>=WSIZE(TmpFileName)-64){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
  WvCopyString(TmpFileName,TmpDirectory);
 } else {
  n=GetTempPathW(WSIZE(TmpFileName),TmpFileName);if(!n || n>=WSIZE(TmpFileName)-64){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
  len=n;
 }
 if(len && TmpFileName[len-1]!=L'\\' && TmpFileName[len-1]!=L'/')TmpFileName[len++]=L'\\';
 /* Protected owner/SYSTEM access, non-inheritable handle, deletion even on abnormal exit. */
 if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;OW)(A;;FA;;;SY)",SDDL_REVISION_1,&sd,NULL))return FALSE;
 sa.nLength=sizeof(sa);sa.lpSecurityDescriptor=sd;sa.bInheritHandle=FALSE;
 for(attempt=0;attempt<16;attempt++) {
  if(BCryptGenRandom(NULL,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0){error=ERROR_GEN_FAILURE;break;}
  WvCopyString(TmpFileName+len,L"~winvi-");
  for(i=0;i<16;i++){TmpFileName[len+7+2*i]=hex[random[i]>>4];TmpFileName[len+8+2*i]=hex[random[i]&15];}
  WvCopyString(TmpFileName+len+39,L".tmp");
  TmpFile=CreateFileW(TmpFileName,GENERIC_READ|GENERIC_WRITE|DELETE,0,&sa,CREATE_NEW,
      FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
  if(TmpFile!=INVALID_HANDLE_VALUE){LocalFree(sd);return TRUE;}
  error=GetLastError();if(error!=ERROR_FILE_EXISTS && error!=ERROR_ALREADY_EXISTS)break;
 }
 LocalFree(sd);SetLastError(error);return FALSE;
}

static WV_LONG LastTmpOffset = 0L;

void RemoveTmpFile(void)
{
	if (TmpFile != INVALID_HANDLE_VALUE) {
		CloseHandle(TmpFile);
		DeleteFileW(TmpFileName);
		TmpFile = INVALID_HANDLE_VALUE;
	}
	LastTmpOffset = 0;
}

static WV_ULONG PagesOccupied;
WV_ULONG	   AccessTime;

static LPPAGE ResidentHead, ResidentTail;
static void UnlinkResident(LPPAGE p)
{
 if(p->ResidentPrev)p->ResidentPrev->ResidentNext=p->ResidentNext;else ResidentHead=p->ResidentNext;
 if(p->ResidentNext)p->ResidentNext->ResidentPrev=p->ResidentPrev;else ResidentTail=p->ResidentPrev;
 p->ResidentPrev=p->ResidentNext=NULL;
}
static void TouchResident(LPPAGE p)
{
 if(ResidentTail==p)return;
 if(p->ResidentPrev || p->ResidentNext || ResidentHead==p)UnlinkResident(p);
 p->ResidentPrev=ResidentTail;p->ResidentNext=NULL;
 if(ResidentTail)ResidentTail->ResidentNext=p;else ResidentHead=p;
 ResidentTail=p;
}
BOOL AllocPageMem(LPPAGE Page)
{	HGLOBAL h;

	if ((((PagesOccupied+1)*PAGESIZE) >> 10) > (PageThreshold<16 ? 16 : PageThreshold)) {
		if (TmpFile != INVALID_HANDLE_VALUE || CreateTmpFile()) {
			LPPAGE ToSave = ResidentHead;
            if(!ToSave) return FALSE;
			if (ToSave->PageFilePos == -1) {
				if ((WV_ULONG)LastTmpOffset > WV_MAX_POSITION-PAGESIZE) {
                    NewStatus(0,L"Temporary-file position would overflow signed 64-bit storage.",NS_ERROR);
                    return FALSE;
                }
                ToSave->PageFilePos = LastTmpOffset;
				LastTmpOffset += PAGESIZE;
			}
			if (ToSave->Flags & ISSAFE
				|| (_llseek((HFILE)TmpFile, ToSave->PageFilePos, 0)
					== ToSave->PageFilePos
				   && _lwrite((HFILE)TmpFile, ToSave->PageBuf, PAGESIZE)
					  == PAGESIZE)) {
				h = GLOBALHANDLE(ToSave->PageBuf);
				GlobalUnlock(h);
				GlobalFree(h);
				--PagesOccupied;
				ToSave->PageBuf = NULL;
            UnlinkResident(ToSave);
				ToSave->Flags |= ISSAFE;
			} else {
				static BOOL AlreadyAsked;
				static int	Answer;

				if (!AlreadyAsked) {
					Answer = MessageBox(hwndMain, ErrorUseVirtual, "WinVi",
										MB_YESNO | MB_ICONQUESTION);
					AlreadyAsked = TRUE;
				}
				if (Answer != IDYES) return (FALSE);
			}
		}
	}
	h = GlobalAlloc(GPTR, PAGESIZE);
	if (h) {
		Page->PageBuf = (LPBYTE)GlobalLock(h);
		if (Page->PageBuf != NULL) {
			Page->LastAccess = ++AccessTime;
            TouchResident(Page);
			++PagesOccupied;
			return (TRUE);
		}
		GlobalFree(h);
	}
	return (FALSE);
}

BOOL IncompleteLastLine(void)
{	POSITION Pos;

	Pos.p = LastPage;
	while (Pos.p && Pos.p->Fill==0) Pos.p = Pos.p->Prev;
	if (Pos.p == NULL) return (FALSE);
	Pos.i = Pos.p->Fill - (UtfEncoding == 16 ? 2 : 1);
	Pos.f = 0;
	return (!(CharFlags(CharAt(&Pos)) & 1));
}

LPPAGE NewPage(void)
{	ALLOC_PPAGE	pPage;

	pPage = PAGEALLOC(sizeof(PAGE));
	if (pPage != NULL) {
		pPage->PageFilePos = -1;
		if (AllocPageMem(pPage)) return (pPage);
		PAGEFREE(pPage);
	}
	MessageBox(hwndMain, ErrorNoGlobalMem, NULL, MB_OK | MB_ICONSTOP);
	return (NULL);
}

BOOL ReloadPage(LPPAGE p)
{	BOOL Result = TRUE;

	if (p->PageFilePos == -1 || TmpFile == INVALID_HANDLE_VALUE
							 || !AllocPageMem(p)) {
		static BOOL ErrorDisplayed;

		if (!ErrorDisplayed && !SessionEnding) {
			ErrorDisplayed = TRUE;
			MessageBox(hwndMain, ErrorPleaseSave, ErrorFatal,
					   MB_OK | MB_ICONSTOP);
		}
		return (FALSE);
	}
	if (_llseek((HFILE)TmpFile, p->PageFilePos, 0)   != p->PageFilePos)
		Result = FALSE;
	if (_lread ((HFILE)TmpFile, p->PageBuf, p->Fill) != p->Fill)
		Result = FALSE;
	if (!Result) {
        HGLOBAL h=GLOBALHANDLE(p->PageBuf);
        UnlinkResident(p);GlobalUnlock(h);GlobalFree(h);
        p->PageBuf=NULL;--PagesOccupied;
    }
    return (Result);
}

/* Release a detached recovery chain, or the previous document after a fully
   staged replacement. Keep the paging file: both chains may have pages in it. */
void WvFreePageChain(LPPAGE page)
{
 WvDbcsInvalidate();
 while(page){
  LPPAGE next=page->Next;
  if(page->PageBuf){
   HGLOBAL h=GLOBALHANDLE(page->PageBuf);UnlinkResident(page);
   GlobalUnlock(h);GlobalFree(h);--PagesOccupied;
  }
  PAGEFREE(page);page=next;
 }
}
void WvReplacePageChain(LPPAGE first,LPPAGE last)
{
 WvFreePageChain(FirstPage);FirstPage=first;LastPage=last;
 CurrPos.p=ScreenStart.p=SelectStart.p=first;
 CurrPos.i=CurrPos.f=ScreenStart.i=ScreenStart.f=SelectStart.i=SelectStart.f=0;
 SelectCount=FirstVisible=0;AdditionalNullByte=FALSE;
 if(LineInfo)memset(LineInfo,0,(VisiLines+2)*sizeof(LINEINFO));
 memset(MarkPos,0,sizeof(MarkPos));
}
void FreeAllPages(void)
{ WvDbcsInvalidate();
 LPPAGE lp = FirstPage, lp2;

	if (!*ErrorUseVirtual) {
		LOADSTRING(hInst, 120, ErrorUseVirtual,  sizeof(ErrorUseVirtual));
		LOADSTRING(hInst, 121, ErrorNoGlobalMem, sizeof(ErrorNoGlobalMem));
		LOADSTRING(hInst, 123, ErrorFatal,       sizeof(ErrorFatal));
		LOADSTRING(hInst, 124, ErrorPleaseSave,  sizeof(ErrorPleaseSave));
	}
	while (lp) {
		lp2 = lp->Next;
		if (lp->PageBuf) {
			HGLOBAL h = GLOBALHANDLE(lp->PageBuf);

			GlobalUnlock(h);
			GlobalFree(h);
			--PagesOccupied;
		}
		PAGEFREE(lp);
		lp = lp2;
	}
	ResidentHead = ResidentTail = NULL;
	RemoveTmpFile();
	FirstPage = LastPage = ScreenStart.p = CurrPos.p = NewPage();
	FirstPage->PageNo = 1;
	ScreenStart.i = CurrPos.i = 0;
	SelectCount = FirstVisible = 0;
	AdditionalNullByte = FALSE;
	memset (LineInfo, 0, (VisiLines+2) * sizeof(LINEINFO));
	memset (MarkPos,  0, sizeof(MarkPos));
}

BYTE const MapAnsiToEbcdic[256] = {
	#define xx 0x1a
	/*-------x0,--x1---x2---x3---x4---x5---x6---x7-
					 --x8---x9---xA---xB---xC---xD---xE---xF-*/
	/*0x*/ 0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f,
					 0x16,0x05,0x25,0x0b,0x0c,0x0d,0x0e,0x0f,
	/*1x*/ 0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26,
					 0x18,0x19,0x3f,0x27,0x1c,0x1d,0x1e,0x1f,
	/*2x*/ 0x40,0x4f,0x7f,0x7b,0x5b,0x6c,0x50,0x7d,
					 0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61,
	/*3x*/ 0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
					 0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f,
	/*4x*/ 0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
					 0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
	/*5x*/ 0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,
					 0xe7,0xe8,0xe9,0x4a,0xe0,0x5a,0x5f,0x6d,
	/*6x*/ 0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
					 0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
	/*7x*/ 0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,
					 0xa7,0xa8,0xa9,0xc0,0xbb,0xd0,0xa1,0x07,
	/*8x*/   xx,  xx,  xx,  xx,  xx,  xx,  xx,  xx,
					   xx,  xx,  xx,  xx,  xx,  xx,  xx,  xx,
	/*9x*/   xx,  xx,  xx,  xx,  xx,  xx,  xx,  xx,
					   xx,  xx,  xx,  xx,  xx,  xx,  xx,  xx,
	/*Ax*/ 0x41,0xaa,0xb0,0xb1,0x9f,0xb2,0x6a,0xb5,
					 0xbd,0xb4,0x9a,0x8a,0xba,0xca,0xaf,0xbc,
	/*Bx*/ 0x90,0x8f,0xea,0xfa,0xbe,0xa0,0xb6,0xb3,
					 0x9d,0xda,0x9b,0x8b,0xb7,0xb8,0xb9,0xab,
	/*Cx*/ 0x64,0x65,0x62,0x66,0x63,0x67,0x9e,0x68,
					 0x74,0x71,0x72,0x73,0x78,0x75,0x76,0x77,
	/*Dx*/ 0xac,0x69,0xed,0xee,0xeb,0xef,0xec,0xbf,
					 0x80,0xfd,0xfe,0xfb,0xfc,0xad,0xae,0x59,
	/*Ex*/ 0x44,0x45,0x42,0x46,0x43,0x47,0x9c,0x48,
					 0x54,0x51,0x52,0x53,0x58,0x55,0x56,0x57,
	/*Fx*/ 0x8c,0x49,0xcd,0xce,0xcb,0xcf,0xcc,0xe1,
					 0x70,0xdd,0xde,0xdb,0xdc,0x8d,0x8e,0xdf
	#undef xx
};
BYTE const MapEbcdicToAnsi[256] = {
	#define xxx 127
	/*-------x0,--x1---x2---x3---x4---x5---x6---x7-
					  -x8---x9---xA---xB---xC---xD---xE---xF-*/
	/*0x*/ 0x00,0x01,0x02,0x03, xxx,'\t', xxx,0x7f,
					  xxx, xxx, xxx,0x0b,0x0c,'\r',0x0e,0x0f,
	/*1x*/ 0x10,0x11,0x12,0x13, xxx, xxx,'\b', xxx,
					 0x18,0x19, xxx, xxx,0x1c,0x1d,0x1e,0x1f,
	/*2x*/  xxx, xxx, xxx, xxx, xxx,'\n',0x17,0x1b,
					  xxx, xxx, xxx, xxx, xxx,0x05,0x06,0x07,
	/*3x*/  xxx, xxx,0x16, xxx, xxx, xxx, xxx,0x04,
					  xxx, xxx, xxx, xxx,0x14,0x15, xxx,0x1a,
	/*4x*/  ' ',0xa0,0xe2,0xe4,0xe0,0xe1,0xe3,0xe5,
					 0xe7,0xf1, '[', '.', '<', '(', '+', '!',
	/*5x*/  '&',0xe9,0xea,0xeb,0xe8,0xed,0xee,0xef,
					 0xec,0xdf, ']', '$', '*', ')', ';', '^',
	/*6x*/  '-', '/',0xc2,0xc4,0xc0,0xc1,0xc3,0xc5,
					 0xc7,0xd1,0xa6, ',', '%', '_', '>', '?',
	/*7x*/ 0xf8,0xc9,0xca,0xcb,0xc8,0xcd,0xce,0xcf,
					 0xcc, '`', ':', '#', '@','\'', '=', '"',
	/*8x*/ 0xd8, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
					  'h', 'i',0xab,0xbb,0xf0,0xfd,0xfe,0xb1,
	/*9x*/ 0xb0, 'j', 'k', 'l', 'm', 'n', 'o', 'p',
					  'q', 'r',0xaa,0xba,0xe6,0xb8,0xc6,0xa4,
	/*Ax*/ 0xb5, '~', 's', 't', 'u', 'v', 'w', 'x',
					  'y', 'z',0xa1,0xbf,0xd0,0xdd,0xde,0xae,
	/*Bx*/ 0xa2,0xa3,0xa5,0xb7,0xa9,0xa7,0xb6,0xbc,
					 0xbd,0xbe,0xac, '|',0xaf,0xa8,0xb4,0xd7,
	/*Cx*/  '{', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
					  'H', 'I',0xad,0xf4,0xf6,0xf2,0xf3,0xf5,
	/*Dx*/  '}', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
					  'Q', 'R',0xb9,0xfb,0xfc,0xf9,0xfa,0xff,
	/*Ex*/ '\\',0xf7, 'S', 'T', 'U', 'V', 'W', 'X',
					  'Y', 'Z',0xb2,0xd4,0xd6,0xd2,0xd3,0xd5,
	/*Fx*/  '0', '1', '2', '3', '4', '5', '6', '7',
					  '8', '9',0xb3,0xdb,0xdc,0xd9,0xda, xxx
	#undef xxx
};

INT ByteAt(PPOSITION Pos)
{
	if (!Pos->p) return (C_ERROR);
	while (Pos->i >= Pos->p->Fill) {
		if (!Pos->p->Next) {
			/*CharAt *MUST* make sure the page is loaded, even at EOF...*/
			/*Next line added because of crash at 2004-06-22 after :substitute*/
			if (Pos->p->PageBuf==NULL && !ReloadPage(Pos->p)) return (C_ERROR);
			return (C_EOF);
		}
		Pos->i -= Pos->p->Fill;
		Pos->p	= Pos->p->Next;
	}
	if (Pos->p->PageBuf == NULL && !ReloadPage(Pos->p)) return (C_ERROR);
	if (Pos->p->LastAccess != AccessTime) Pos->p->LastAccess = ++AccessTime;
    TouchResident(Pos->p);
	return Pos->p->PageBuf[Pos->i];
}

/* DBCS text readers expose decoded UTF-16 values, while Byte* and hex mode
   remain byte-oriented. Boundary summaries avoid rescanning long lines on h. */
extern INT AnsiCodePage,OemCodePage;
static DWORD DbcsEpoch=1,DbcsMapEpoch;
static WV_ULONG DbcsDirtyFrom=1;
static LPPAGE DbcsMapPage;
static BYTE DbcsMap[(PAGESIZE+7)/8];
static BYTE DbcsLead[256],DbcsSingleValid[256],DbcsPairValid[8192];
static WCHAR DbcsSingle[256],DbcsValues[65536];
static void DbcsPrepare(UINT cp)
{
 memset(DbcsPairValid,0,sizeof(DbcsPairValid));
 for(UINT a=0;a<256;a++){
  CHAR byte=(CHAR)a;DbcsLead[a]=(BYTE)IsDBCSLeadByteEx(cp,(BYTE)a);
  DbcsSingleValid[a]=(BYTE)(MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,&byte,1,DbcsSingle+a,1)==1);
  if(DbcsLead[a])for(UINT b=0;b<256;b++){
   CHAR pair[2]={(CHAR)a,(CHAR)b};UINT key=(a<<8)|b;
   if(MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,pair,2,DbcsValues+key,1)==1)DbcsPairValid[key>>3]|=(BYTE)(1<<(key&7));
  }
 }
}
UINT WvLegacyCodePage(void)
{UINT cp=CharSet==CS_OEM?OemCodePage:AnsiCodePage;return cp?cp:CharSet==CS_OEM?GetOEMCP():GetACP();}
void WvDbcsInvalidate(void)
{
 if(!++DbcsEpoch){for(LPPAGE p=FirstPage;p;p=p->Next)p->DbcsEpoch=0;DbcsEpoch=1;}
 DbcsMapPage=NULL;DbcsMapEpoch=0;DbcsDirtyFrom=1;
}
void WvDbcsDirty(LPPAGE page)
{
 WV_ULONG first=page && page->PageNo>1?page->PageNo-1:1;
 if(first<DbcsDirtyFrom)DbcsDirtyFrom=first;DbcsMapPage=NULL;DbcsMapEpoch=0;
}
BOOL WvDbcsText(void)
{
 static UINT previous=(UINT)-1;static BOOL dbcs;
 UINT cp;if(UtfEncoding || HexEditMode || (CharSet!=CS_ANSI && CharSet!=CS_OEM))return FALSE;
 cp=WvLegacyCodePage();if(cp!=previous){CPINFO info;previous=cp;dbcs=GetCPInfo(cp,&info) && info.MaxCharSize==2;if(dbcs)DbcsPrepare(cp);WvDbcsInvalidate();}
 return dbcs;
}
INT WvLegacyEncode(INT value,BYTE bytes[4])
{
 WCHAR c=(WCHAR)value;BOOL used=FALSE;INT n;
 if(value<0 || value>0xffff || (value>=0xd800 && value<=0xdfff))return 0;
 n=WideCharToMultiByte(WvLegacyCodePage(),WC_NO_BEST_FIT_CHARS,&c,1,(LPSTR)bytes,4,NULL,&used);
 return used?0:n;
}
static BOOL DbcsPair(INT first,INT second,WCHAR *wide)
{
 if(first<0 || first>255 || second<0 || second>255)return FALSE;
 UINT key=((UINT)first<<8)|(UINT)second;
 if(!(DbcsPairValid[key>>3]&(1<<(key&7))))return FALSE;
 if(wide)*wide=DbcsValues[key];return TRUE;
}
INT WvDbcsRead(PPOSITION pos,UINT *span)
{
 INT first=ByteAt(pos);WCHAR value;*span=1;
 if(first<0)return first;if(first<128)return first;
 if(DbcsLead[(BYTE)first]){
  POSITION next=*pos;Advance(&next,1);INT second=ByteAt(&next);
  if(second==C_ERROR)return C_ERROR;
  if(DbcsPair(first,second,&value)){*span=2;return value;}return C_UNDEF;
 }
 return DbcsSingleValid[(BYTE)first]?DbcsSingle[(BYTE)first]:C_UNDEF;
}
static BOOL DbcsBuildMap(LPPAGE target)
{
 LPPAGE start=target;UINT incoming;
 if(target->PageNo>=DbcsDirtyFrom)while(start->Prev && start->PageNo>DbcsDirtyFrom)start=start->Prev;
 while(start->Prev && start->Prev->DbcsEpoch!=DbcsEpoch)start=start->Prev;
 incoming=start->Prev?start->Prev->DbcsTail:0;
 for(LPPAGE page=start;page;page=page->Next){
  UINT i=0,tail=0;BOOL mapping=page==target;
  if(!page->PageBuf && !ReloadPage(page))return FALSE;
  if(mapping)memset(DbcsMap,0,sizeof(DbcsMap));
  if(!page->Fill)tail=incoming;
  else if(incoming && DbcsPair(incoming,page->PageBuf[0],NULL))i=1;
  while(i<page->Fill){
   INT c=page->PageBuf[i];if(mapping)DbcsMap[i>>3]|=(BYTE)(1<<(i&7));
   if(DbcsLead[(BYTE)c]){
    if(i+1==page->Fill){tail=c;i++;}
    else if(DbcsPair(c,page->PageBuf[i+1],NULL))i+=2;
    else i++;
   }else i++;
  }
  page->DbcsEpoch=DbcsEpoch;page->DbcsTail=tail;incoming=tail;
  if(mapping){if(target->PageNo>=DbcsDirtyFrom)DbcsDirtyFrom=target->PageNo+1;DbcsMapPage=target;DbcsMapEpoch=DbcsEpoch;return TRUE;}
 }
 return FALSE;
}
static BOOL DbcsMapFor(LPPAGE page)
{return DbcsMapPage==page && DbcsMapEpoch==DbcsEpoch?TRUE:DbcsBuildMap(page);}
BOOL WvDbcsPrevious(PPOSITION pos)
{
 POSITION p=*pos;
 for(;;){
  if(p.i>p.p->Fill)p.i=p.p->Fill;
  if(!DbcsMapFor(p.p))return FALSE;
  while(p.i){--p.i;if(DbcsMap[p.i>>3]&(1<<(p.i&7))){p.f=0;*pos=p;return TRUE;}}
  if(!p.p->Prev)return FALSE;p.p=p.p->Prev;p.i=p.p->Fill;
 }
}
BOOL WvDbcsBoundary(PPOSITION pos)
{
 POSITION p=*pos;INT c=ByteAt(&p);
 if(c==C_ERROR)return FALSE;if(c==C_EOF)return TRUE;
 return DbcsMapFor(p.p) && (DbcsMap[p.i>>3]&(1<<(p.i&7)))!=0;
}
void WvDbcsNormalize(PPOSITION pos,BOOL after)
{
 if(WvDbcsText() && !WvDbcsBoundary(pos)){
  POSITION p=*pos;if(WvDbcsPrevious(&p)){if(after)CharAndAdvance(&p);*pos=p;}
 }
}

INT CharAt(PPOSITION Pos)
{ INT c;
 if(WvDbcsText()){
  UINT span;c=WvDbcsRead(Pos,&span);
  if(c=='\r' && !(Pos->f&POSITION_PREVENT_CRLF_COMBINATION)){POSITION p=*Pos;Advance(&p,1);if(ByteAt(&p)=='\n')c=C_CRLF;}
  return c;
 }

	c = ByteAt(Pos);
	if (c < 0) return c;
	if (UtfEncoding) {
		if (UtfEncoding == 16) {
			if (UtfLsbFirst) {
				c |= AdvanceAndByte(Pos) << 8;
			} else {
				c <<= 8;
				c |= AdvanceAndByte(Pos);
			}
			GoBack(Pos, 1);
		} else if (c >= 0xc0) {
			POSITION Pos2 = *Pos;
			WV_LONG	 c2;

			if (c < 0xe0) {
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x1f) << 6) | (c2 & 0x3f);
				if (c < 0x80) return C_UNDEF;
			} else if (c < 0xf0) {
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x1f) << 6) | (c2 & 0x3f);
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if (c < 0x800) return C_UNDEF;
			} else if (c < 0xf8) {
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x07) << 6) | (c2 & 0x3f);
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if ((c2 = AdvanceAndByte(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if (c < 0x10000 || c > 0x10ffff) return C_UNDEF;
				/*convert to UTF-16 word...*/
				if (Pos->f & POSITION_UTF8_HIGH_SURROGATE_DONE)
					 c = (c & 0x3ff)		| 0xdc00;
				else c = ((c >> 10) - 0x40)	| 0xd800;
			} else return C_UNDEF;
		} else if (c >= 0x80) return C_UNDEF;
	} else if (CharSet == CS_EBCDIC) c = MapEbcdicToAnsi[c];
	if (c == '\r' && !(Pos->f & POSITION_PREVENT_CRLF_COMBINATION)) {
		POSITION Pos2 = *Pos;

		Pos2.f |= POSITION_PREVENT_CRLF_COMBINATION;
		if (Advance(&Pos2, UtfEncoding == 16 ? 2 : 1) && CharAt(&Pos2) == '\n')
			c = C_CRLF;
	}
	return c;
}

INT ByteAndAdvance(PPOSITION Pos)
{	INT c;

	if (!Pos->p) return (C_ERROR);
	while (Pos->i >= Pos->p->Fill) {
		if (!Pos->p->Next) return (C_EOF);
		Pos->i -= Pos->p->Fill;
		Pos->p	= Pos->p->Next;
	}
	if (!Pos->p->PageBuf && !ReloadPage(Pos->p)) return C_ERROR;
	if (Pos->p->LastAccess != AccessTime) Pos->p->LastAccess = ++AccessTime;
    TouchResident(Pos->p);
	c = Pos->p->PageBuf[Pos->i];
	++Pos->i;
	while (Pos->i >= Pos->p->Fill && Pos->p->Next) {
		Pos->i = 0;
		Pos->p = Pos->p->Next;
	}
	return c;
}

INT CharAndAdvance(PPOSITION Pos)
{ INT c;
 if(WvDbcsText()){
  UINT span;c=WvDbcsRead(Pos,&span);if(c==C_EOF || c==C_ERROR)return c;Advance(Pos,span);
  if(c=='\r' && !(Pos->f&POSITION_PREVENT_CRLF_COMBINATION) && ByteAt(Pos)=='\n'){Advance(Pos,1);c=C_CRLF;}
  return c;
 }

	c = ByteAndAdvance(Pos);
	if (c < 0) return c;
	if (UtfEncoding) {
		if (UtfEncoding == 16) {
			if (UtfLsbFirst) {
				c |= ByteAndAdvance(Pos) << 8;
			} else {
				c <<= 8;
				c |= ByteAndAdvance(Pos);
			}
		} else if (c >= 0xc0) {
			POSITION Pos2 = *Pos;
			WV_LONG	 c2;

			if (c < 0xe0) {
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x1f) << 6) | (c2 & 0x3f);
				if (c < 0x80) return C_UNDEF;
				*Pos = Pos2;
			} else if (c < 0xf0) {
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x1f) << 6) | (c2 & 0x3f);
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if (c < 0x800) return C_UNDEF;
				*Pos = Pos2;
			} else if (c < 0xf8) {
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = ((c & 0x07) << 6) | (c2 & 0x3f);
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if ((c2 = ByteAndAdvance(&Pos2)) < 0x80 || c2 >= 0xc0)
					return C_UNDEF;
				c = (c << 6) | (c2 & 0x3f);
				if (c < 0x10000 || c > 0x10ffff) return C_UNDEF;
				/*convert to UTF-16 word...*/
				if (Pos->f&POSITION_UTF8_HIGH_SURROGATE_DONE) {
					c = (c & 0x3ff) | 0xdc00;
					*Pos = Pos2;
					Pos->f &= ~POSITION_UTF8_HIGH_SURROGATE_DONE;
				} else {
					c = ((c >> 10) - 0x40) | 0xd800;
					Pos->f |= POSITION_UTF8_HIGH_SURROGATE_DONE;
					GoBack(Pos, 1);
				}
			} else return C_UNDEF;
		} else if (c >= 0x80) return C_UNDEF;
	} else if (CharSet == CS_EBCDIC) c = MapEbcdicToAnsi[c];
	if (c == '\r' && !(Pos->f & POSITION_PREVENT_CRLF_COMBINATION)) {
		POSITION Pos2 = *Pos;

		Pos2.f |= POSITION_PREVENT_CRLF_COMBINATION;
		if (CharAndAdvance(&Pos2) == '\n') {
			c = C_CRLF;
			Pos2.f &= ~POSITION_PREVENT_CRLF_COMBINATION;
			*Pos = Pos2;
		}
	}
	return c;
}

INT AdvanceAndByte(PPOSITION Pos)
{	INT c;

	if (!(CharFlags(c = ByteAndAdvance(Pos)) & 0x20)) c = ByteAt(Pos);
	return c;
}

INT AdvanceAndChar(PPOSITION Pos)
{	INT c;

	if (!(CharFlags(c = CharAndAdvance(Pos)) & 0x20)) c = CharAt(Pos);
	return c;
}

WV_ULONG GoBack(PPOSITION Pos, WV_ULONG Amount)
{	WV_ULONG Ret = 0;

	while (Amount > Pos->i) {
		Ret += Pos->i;
		Amount -= Pos->i;
		if (!Pos->p->Prev) {
			Pos->i = 0;
			return (Ret);
		}
		Pos->p = Pos->p->Prev;
		Pos->i = Pos->p->Fill;
	}
	Pos->i -= (int)Amount;
	return Ret + Amount;
}

INT GoBackAndByte(PPOSITION Pos)
{
	if (!GoBack(Pos, 1)) return (C_EOF);
	return ByteAt(Pos);
}

INT GoBackAndChar(PPOSITION Pos)
{ INT c;
 if(WvDbcsText()){
  POSITION p=*Pos;if(!WvDbcsPrevious(&p))return CountBytes(Pos)?C_ERROR:C_EOF;c=CharAt(&p);
  if(c=='\n' && !(Pos->f&POSITION_PREVENT_CRLF_COMBINATION)){POSITION q=p;if(GoBack(&q,1) && ByteAt(&q)=='\r'){p=q;c=C_CRLF;}}
  *Pos=p;return c;
 }
	INT		 cmin = C_MIN;
	POSITION Pos2;

	if (UtfEncoding == 8 && Pos->f & POSITION_UTF8_HIGH_SURROGATE_DONE) {
		Pos->f &= ~POSITION_UTF8_HIGH_SURROGATE_DONE;
		return CharAt(Pos);
	}
	if (!GoBack(Pos, UtfEncoding == 16 ? 2 : 1)) return (C_EOF);
	if (UtfEncoding == 8) {
		INT bmin, bmax;

		Pos2 = *Pos;
		if ((c = ByteAt(Pos)) >= 0x80 && c < 0xc0) {
			if ((c = GoBackAndByte(Pos)) >= 0x80 && c < 0xc0) {
				if ((c = GoBackAndByte(Pos)) >= 0x80 && c < 0xc0) {
					c = GoBackAndByte(Pos);
					Pos->f |= POSITION_UTF8_HIGH_SURROGATE_DONE;
					bmin = 0xf0;
					bmax = 0xf7;
					cmin = 0x10000;
				} else {
					bmin = 0xe0;
					bmax = 0xef;
					cmin = 0x800;
				}
			} else {
				bmin = 0xc0;
				bmax = 0xdf;
			}
			if (c < bmin || c > bmax) {
				CHAR Buf[80];

				snprintf(Buf, sizeof(Buf),
						 "WinVi: invalid char 0x%02x < %x || > %x at pos %I64u\n",
						 c, bmin, bmax, CountBytes(Pos));
				OutputDebugString(Buf);
				if (c != C_EOF) Advance(Pos, 1);	/* enforce C_UNDEF */
				Pos->f &= ~POSITION_UTF8_HIGH_SURROGATE_DONE;
			}
		}
	}
	if ((c = CharAt(Pos)) == '\n' && CountBytes(Pos)) {
		Pos2 = *Pos;
		if (GoBack(Pos, UtfEncoding == 16 ? 2 : 1)) {
			if (CharAt(Pos) == C_CRLF) c = C_CRLF;
			else *Pos = Pos2;
		}
	} else if (!HexEditMode && c == C_BOM && CountBytes(Pos) == 0) {
		Advance(Pos, UtfEncoding == 8 ? 3 : 2);
		c = C_EOF;
	} else if (c < cmin) c = C_UNDEF;
	if (c == C_UNDEF && UtfEncoding == 8) *Pos = Pos2;
	return c;
}

WV_ULONG Advance(PPOSITION Pos, WV_ULONG Amount)
{	WV_ULONG Ret = 0;

	Pos->f &= ~POSITION_UTF8_HIGH_SURROGATE_DONE;
	for (;;) {
		int n = Pos->p->Fill - Pos->i;

		if ((WV_ULONG)n > Amount) {
			Pos->i += (int)Amount;
			return (Ret + Amount);
		}
		Ret += n;
		Amount -= n;
		if (!Pos->p->Next) {
			Pos->i += n;
			return Ret;
		}
		Pos->p = Pos->p->Next;
		Pos->i = 0;
	}
}

void BeginOfFile(PPOSITION Pos)
{	
	Pos->p = FirstPage;
	Pos->i = 0;
	Pos->f = 0;
	if (UtfEncoding && CharAt(Pos) == C_BOM) AdvanceAndChar(Pos);
}

void EndOfFile(PPOSITION Pos)
{
	Pos->p = LastPage;
	Pos->i = LastPage->Fill;
	Pos->f = 0;
}

void CorrectLineInfo(LPPAGE OldPage, UINT OldInx, LPPAGE NewPage, UINT NewInx,
					 WV_ULONG OldCount, INT CountDiff)
{	int i;

    /* Marks follow their original character (right gravity on insertion).
       Page splits relocate positions; deletions clamp consumed marks to the
       surviving boundary instead of leaving an index beyond the page fill. */
    for (i=0; i<27; ++i) if (MarkPos[i].p) {
        PPOSITION mark=&MarkPos[i];
        while (mark->p->Next && mark->i>=mark->p->Fill) {
            mark->i-=mark->p->Fill;mark->p=mark->p->Next;
        }
        if (OldPage && mark->p==OldPage) {
            if (mark->i>=OldInx) {
                mark->p=NewPage;
                mark->i=mark->i-OldInx+NewInx;
            } else if (OldPage==NewPage && CountDiff>0 && OldInx && mark->i==OldInx-1) {
                mark->i+=(UINT)CountDiff;
            } else if (OldPage==NewPage && NewInx<OldInx && mark->i>=NewInx) {
                mark->i=NewInx;mark->f=0;
            }
        }
        while (mark->p->Next && mark->i>=mark->p->Fill) {
            mark->i-=mark->p->Fill;mark->p=mark->p->Next;
        }
    }
	LineInfo[0].Pos = ScreenStart;
	for (i=CurrLine; i<=VisiLines; ++i) {
		if (LineInfo[i].Pos.p == OldPage && LineInfo[i].Pos.i >= OldInx) {
			LineInfo[i].Pos.p  = NewPage;
			LineInfo[i].Pos.i -= OldInx - NewInx;
			if (!i) ScreenStart = LineInfo[0].Pos;
		}
		if (LineInfo[i].ByteCount > OldCount)
			LineInfo[i].ByteCount += CountDiff;
	}
}

INT CountNewlinesInBuf(LPBYTE Buf, INT Len, WV_LONG NewLineCnt[9], DWORD *pFlags)
{	INT    Ret = 0;
	LPBYTE Ptr = Buf;
	BYTE   c;
	BOOL   PrevCr, PrevNul;
	DWORD  Utf8Flags = 0;

	if (pFlags != NULL) {
		PrevCr  = (*pFlags & CNLB_LASTWASCR)  != 0;
		PrevNul = (*pFlags & CNLB_LASTWASNUL) != 0;
		Utf8Flags = *pFlags & ~(CNLB_LASTWASCR | CNLB_LASTWASNUL);
	} else PrevCr = PrevNul = FALSE;
	while (Ptr < Buf + Len) {
		if (UtfEncoding == 16) {
			if (UtfLsbFirst) {
				if (Ptr-Buf+1 >= Len || Ptr[1] != '\0') {
					PrevCr = PrevNul = FALSE; /* Non-ASCII is not CR/LF. */
					Ptr += 2;
					continue;
				}
			} else if (Ptr-Buf+1 >= Len || *Ptr++ != '\0') {
				PrevCr = PrevNul = FALSE;
				++Ptr;
				continue;
			}
		} else if (!(Utf8Flags & CNLB_UTF8_MISMATCH)) {
			INT c;

			if ((c = *Ptr) >= 0x80) {
				if (c < 0xc0)
					if (Utf8Flags & CNLB_UTF8_3FOLLOWING) {
						Utf8Flags -= CNLB_UTF8_1FOLLOWING;
						if (!(Utf8Flags & CNLB_UTF8_3FOLLOWING))
							if ((Utf8Flags & CNLB_UTF8_COUNT) < CNLB_UTF8_COUNT)
								Utf8Flags += CNLB_UTF8_INCREMENT;
					} else Utf8Flags = CNLB_UTF8_MISMATCH;
				else if (Utf8Flags & CNLB_UTF8_3FOLLOWING)
					Utf8Flags = CNLB_UTF8_MISMATCH;
				else if (c < 0xe0) Utf8Flags += CNLB_UTF8_1FOLLOWING;
				else if (c < 0xf0) Utf8Flags += CNLB_UTF8_2FOLLOWING;
				else if (c < 0xf8) Utf8Flags += CNLB_UTF8_3FOLLOWING;
				else Utf8Flags = CNLB_UTF8_MISMATCH;
			} else if (Utf8Flags & CNLB_UTF8_3FOLLOWING)
				Utf8Flags = CNLB_UTF8_MISMATCH;
		}
		c = *Ptr;
		if (CharSet == CS_EBCDIC) c = MapEbcdicToAnsi[c];
		if (CharFlags(c) & 1) {
			++Ret;
			if (NewLineCnt != NULL) {
				switch (c) {
					case '\r':		++NewLineCnt[1];
									break;
					case '\n':		if (PrevCr) {
										   ++NewLineCnt[0];
										   --NewLineCnt[1];
										   --Ret;
									} else ++NewLineCnt[2];
									break;
					case '\036':	++NewLineCnt[3];
									break;
					case '\0':		if ((Ptr - Buf) & 1) {
										++NewLineCnt[5];
										if (PrevNul) ++NewLineCnt[6];
									} else {
										++NewLineCnt[4];
										PrevNul = TRUE;
									}
									break;
				}
			} else if (c == '\n' && PrevCr) --Ret;
		} else if (c == 0x25 /* EBCDIC LF */ && NewLineCnt != NULL) {
			if (PrevCr)
				 ++NewLineCnt[7];
			else ++NewLineCnt[8];
		}
		if (c != '\0') PrevNul = FALSE;
		PrevCr = c == '\r';
		++Ptr;
		if (UtfEncoding == 16 && UtfLsbFirst) {
			assert(*Ptr == '\0');
			++Ptr;
		}
	}
	if (pFlags != NULL) {
		*pFlags = Utf8Flags;
		if (PrevCr)  *pFlags |= CNLB_LASTWASCR;
		if (PrevNul) *pFlags |= CNLB_LASTWASNUL;
	}
	return Ret;
}

void UnsafePage(PPOSITION Pos)
{ WvDbcsDirty(Pos->p);
 BYTE  Nl = CharSet==CS_EBCDIC ? MapAnsiToEbcdic['\n'] : '\n';

	Pos->p->Flags &= ~ISSAFE;
	if (HexEditMode && Pos->p->Fill) {
		if (Pos->p->PageBuf == NULL) ReloadPage(Pos->p);
		if (Pos->p->PageBuf == NULL) {
			assert(Pos->p->PageBuf != NULL);
		} else {
			if (Pos->p->PageBuf[0]==Nl && Pos->p->Prev!=NULL)
				Pos->p->Prev->Flags |= CHECK_CRLF;
			if (Pos->p->PageBuf[Pos->p->Fill-1]=='\r')
				Pos->p->Flags |= CHECK_CRLF;
		}
	}
}

void (*WvExGlobalEditHook)(WV_ULONG,WV_ULONG,WV_ULONG);
BOOL Reserve(PPOSITION Pos, WV_LONG Amount, WORD Flags)
	/* Amount may be:  <0 (delete bytes),
	 *					1 (insert one byte), or
	 *					2 (insert two consecutive bytes, i.e. cr/lf)
	 * Flags: 1=contains newline
	 * return value: state of success
	 */
{
	LPPAGE Page;
	WV_ULONG globalOffset=WvExGlobalEditHook?CountBytes(Pos):0;

	if (Amount > PAGESIZE || Amount == (-9223372036854775807LL-1)) {
        NewStatus(0,L"Invalid page-reservation size.",NS_ERROR);return FALSE;
    }
    WvDbcsDirty(Pos->p);
    /* A full document near INT64_MAX must also reject single-character growth.
       Normal edits avoid a whole-document count: that size requires this many pages. */
    if (Amount>0 && LastPage && LastPage->PageNo>=WV_MAX_POSITION/PAGESIZE) {
        POSITION end={LastPage,LastPage->Fill,0};WV_ULONG size;
        if (!WvSizeAdd(CountBytes(&end),(WV_ULONG)Amount,&size)) {
            NewStatus(0,L"Edit would overflow the signed 64-bit document size.",NS_ERROR);return FALSE;
        }
    }
    if (Flags & 1) LinesInFile += Amount > 0 ? 1 : -1;
	if (Amount < 0) {
		EnterDeleteForUndo(Pos, -Amount, 0);
		do {
			if (CharAt(Pos) == C_EOF || !Pos->p->PageBuf) return (FALSE);
			if (Pos->i < Pos->p->Fill) {
				_fmemcpy(Pos->p->PageBuf + Pos->i,
						 Pos->p->PageBuf + Pos->i + 1,
						 Pos->p->Fill    - Pos->i - 1);
				UnsafePage(Pos);
				if (!HexEditMode)
					CorrectLineInfo(Pos->p, Pos->i+1, Pos->p, Pos->i,
									CountBytes(Pos), -1);
			}
			assert(Pos->p->Fill);
			--Pos->p->Fill;
			if(WvExGlobalEditHook)WvExGlobalEditHook(globalOffset,1,0);
		} while (++Amount);
		if (Flags & 1) {
			Page = Pos->p;
			do {
				--Page->Newlines;
				Page = Page->Next;
			} while (Page != NULL);
		}
		return (TRUE);
	}
	EnterInsertForUndo(Pos, Amount);
	if (Pos->i==0 && Pos->p->Prev && Pos->p->Prev->Fill+Amount < PAGESIZE) {
		CharAt(&ScreenStart);
		Pos->p = Pos->p->Prev;
		Pos->i = Pos->p->Fill;
		if (!Pos->p->PageBuf && !ReloadPage(Pos->p)) return (FALSE);
		Pos->p->Fill += Amount;
		UnsafePage(Pos);
		if (Flags & 1) {
			Page = Pos->p;
			do {
				++Page->Newlines;
				Page = Page->Next;
			} while (Page != NULL);
		}
		if (ScreenStart.p==Pos->p->Next && ScreenStart.i==0)
			LineInfo[0].Pos = ScreenStart = *Pos;
		CorrectLineInfo(Pos->p, Pos->i+1, Pos->p, Pos->i+Amount+1, CountBytes(Pos), Amount);
		if(WvExGlobalEditHook)WvExGlobalEditHook(globalOffset,0,Amount);
		return (TRUE);
	}
	if (Pos->p->Fill + Amount > PAGESIZE) {
		Page = NewPage();
		if (!Page) return (FALSE);
		if (!Pos->p->PageBuf && !ReloadPage(Pos->p))
			return (FALSE);
		assert((WV_ULONG)Pos->i <= PAGESIZE);
		assert(Page->PageBuf);
		assert(Pos->p->PageBuf);
		if (Pos->i < Pos->p->Fill) {
			_fmemcpy(Page->PageBuf, Pos->p->PageBuf + Pos->i,
									Page->Fill = Pos->p->Fill - Pos->i);
			UnsafePage(Pos);
		} else Page->Fill = 0;
		Page->Next = Pos->p->Next;
		if (Page->Next == NULL) LastPage = Page;
		else Page->Next->Prev = Page;
		Page->Prev = Pos->p;
		Page->Newlines = Pos->p->Newlines;
		Pos->p->Newlines -= CountNewlinesInBuf(Page->PageBuf, Page->Fill, NULL,
											   NULL /*TODO: check prev char*/);
		Pos->p->Next = Page;
		Pos->p->Fill = Pos->i;
		if (Pos->i + Amount > PAGESIZE) {
			CorrectLineInfo(Pos->p, Pos->i, Page, 0, 0, 0);
			Pos->p = Page;
			Pos->i = 0;
		} else {
			CorrectLineInfo(Pos->p, Pos->i+1, Page, 1, 0, 0);
			if (Flags & 1) ++Pos->p->Newlines;
		}
		while (Page) {
			Page->PageNo = Page->Prev->PageNo + 1;
			if (Flags & 1) ++Page->Newlines;
			Page = Page->Next;
		}
	} else if (Flags & 1)
		for (Page = Pos->p; Page; Page = Page->Next)
			++Page->Newlines;
	assert(Pos->i + Amount <= PAGESIZE);
	assert(Pos->p->Fill + Amount <= PAGESIZE);
	if (!Pos->p->PageBuf && !ReloadPage(Pos->p))
		return (FALSE);
	UnsafePage(Pos);
	if (Pos->i < Pos->p->Fill)
		_fmemmove(Pos->p->PageBuf + Pos->i + Amount,
				  Pos->p->PageBuf + Pos->i, Pos->p->Fill - Pos->i);
	else if (LineInfo[0].Pos.p==Pos->p->Next && !LineInfo[0].Pos.i) {
		LineInfo[0].Pos = ScreenStart = *Pos;
	}
	Pos->p->Fill += Amount;
	CorrectLineInfo(Pos->p, Pos->i+1, Pos->p, Pos->i + Amount+1,
					CountBytes(Pos), Amount);
	if(WvExGlobalEditHook)WvExGlobalEditHook(globalOffset,0,Amount);
	return (TRUE);
}

WV_ULONG CountBytes(PPOSITION Pos)
{	WV_ULONG  Ret  = Pos->i;
	LPPAGE Page = Pos->p;

	if (Page != NULL)
		while ((Page = Page->Prev) != NULL) Ret += Page->Fill;
	return Ret;
}

WV_ULONG CountLines(PPOSITION Pos)
{	WV_ULONG Ret;
	BYTE  Nl = CharSet==CS_EBCDIC ? MapAnsiToEbcdic['\n'] : '\n';

	if (HexEditMode) return CountBytes(Pos) >> 4;
	if (ByteAt(Pos) == C_ERROR) return 0;
	Ret = Pos->p->Prev ? Pos->p->Prev->Newlines : 0;
	Ret += CountNewlinesInBuf(Pos->p->PageBuf, Pos->i, NULL, NULL);
	if (Pos->i > 0 && (Pos->p->PageBuf[0] == Nl ||
			(UtfEncoding == 16 && Pos->p->PageBuf[1] == Nl))) {
		POSITION Pos2 = *Pos;

		Pos2.i = 0;
		Pos2.f = 0;
		if (GoBack(&Pos2, UtfEncoding == 16 ? 2 : 1) && CharAt(&Pos2) == C_CRLF)
			--Ret;
	}
	return Ret;
}

void VerifyPageStructure(void)
	/*correct cr/lf sequences across page boundaries and count newlines...*/
{
#if 0	/*TODO*/
	LPPAGE Test, CrFound = NULL;
	WV_ULONG  NlCount = 0, NlDiff = 0;

	for (Test=FirstPage; Test; Test=Test->Next) {
		if (Test->Fill && (Test->Flags & CHECK_CRLF || CrFound != NULL)) {
			if (!Test->PageBuf && !ReloadPage(Test)) continue;
			if (CrFound!=NULL && Test->PageBuf[0]=='\n') {
				if (CrFound->Fill < PAGESIZE) {
					--Test->Fill;
					_fmemmove(Test->PageBuf, Test->PageBuf+1, Test->Fill);
					CrFound->PageBuf[CrFound->Fill++] = '\n';
				} else if (Test->Fill < PAGESIZE) {
					_fmemmove(Test->PageBuf+1, Test->PageBuf, Test->Fill);
					++Test->Fill;
					Test->PageBuf[0] = '\r';
					--CrFound->Fill;
					do --CrFound->Newlines;
					while ((CrFound = CrFound->Next) != Test);
					--NlCount;
				} else {
					--Test->Fill;
					_fmemmove(Test->PageBuf, Test->PageBuf+1, Test->Fill);
					--CrFound->Fill;
					do --CrFound->Newlines;
					while ((CrFound = CrFound->Next) != Test);
					CrFound = NewPage();
					CrFound->Next = Test;
					CrFound->Prev = Test->Prev;
					CrFound->Prev->Next = CrFound;
					Test->Prev = CrFound;
					lstrcpy(CrFound->PageBuf, "\r\n");
					CrFound->Fill = 2;
					CrFound->Newlines = NlCount;
				}
			}
			NlCount += CountNewlinesInBuf(Test->PageBuf, Test->Fill, NULL,
										  NULL /*TODO: check prev char*/);
			if (Test->Newlines != NlCount) {
				NlDiff = NlCount - Test->Newlines;
				Test->Newlines += NlDiff;
			}
			CrFound = Test->PageBuf[Test->Fill-1]=='\r' ? Test : NULL;
			Test->Flags &= ~CHECK_CRLF;
		} else NlCount = Test->Newlines += NlDiff;
	}
	LinesInFile = NlCount;
#endif
	LinesInFile = LastPage->Newlines;
}
