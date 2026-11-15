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
 *		3-Dec-2000	first publication of source code
 */

#include <windows.h>
#include <memory.h>
#include "winvi.h"

static HBITMAP	hBkgndBmp[2];
static BITMAP	sBkgndBmp[2];
static HPALETTE	hPalette[2];
static BOOL		Init[2], Fail[2];

static BOOL CheckSize(UINT Size1, WV_LONG Size2) {
	return (Size1 > 10000000 || Size2 < 0 || Size2 > 10000000);
}

BOOL InitBmp(HDC hDC, int BmpInx) {
	static BITMAPFILEHEADER	sBmpFile;
	static BITMAPINFOHEADER sBmpInfo;
	LOGPALETTE	 *pPalette = NULL;
	LPBYTE		 lpPattern = NULL;
	LPBITMAPINFO lpBmpInfo = NULL;
	HGLOBAL		 hPattern  = NULL, hBmpInfo  = NULL;
	HFILE		 hf		   = HFILE_ERROR;
	UINT		 RgbSize, Colors;
	WV_LONG		 BmpSize;
	HPALETTE	 hOldPalette;

	do {	/*for break only*/
		BOOL UsePalette;
		UsePalette = (GetDeviceCaps(hDC, RASTERCAPS) & RC_PALETTE) != 0;

		if (*BitmapFilename == '\0') break;
		if ((hf = (HFILE)CreateFileW(BitmapFilename, GENERIC_READ,
									 FILE_SHARE_READ | FILE_SHARE_WRITE,
									 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
									 NULL)) == HFILE_ERROR) {
			OutputDebugString("WinVi: cannot open bitmap file\n");
			break;
		}
		if (_lread(hf, &sBmpFile, sizeof(sBmpFile)) != sizeof(sBmpFile)) {
			OutputDebugString("WinVi: cannot read bitmap header\n");
			break;
		}
		if (_lread(hf, &sBmpInfo, sizeof(sBmpInfo)) != sizeof(sBmpInfo)) {
			OutputDebugString("WinVi: cannot read bitmap info\n");
			break;
		}
		if (*(short*)&sBmpFile != *(short*)"BM") {
			OutputDebugString("WinVi: invalid bitmap signature\n");
			break;
		}
        /* Validate all dimensions, palette counts and extents before any GDI call. */
        {WV_LONG fileSize=_llseek(hf,0,FILE_END);DWORD header=sBmpInfo.biSize;
         WV_ULONG stride,pixels,height,paletteOffset,paletteEnd;
         if(fileSize<0 || (header!=40 && header!=108 && header!=124) ||
            sBmpInfo.biPlanes!=1 || sBmpInfo.biWidth<=0 || !sBmpInfo.biHeight ||
            sBmpInfo.biCompression!=BI_RGB ||
            (sBmpInfo.biBitCount!=1 && sBmpInfo.biBitCount!=4 && sBmpInfo.biBitCount!=8 && sBmpInfo.biBitCount!=16 && sBmpInfo.biBitCount!=24 && sBmpInfo.biBitCount!=32))break;
         height=sBmpInfo.biHeight<0?-(WV_LONG)sBmpInfo.biHeight:sBmpInfo.biHeight;
         stride=(((WV_ULONG)sBmpInfo.biWidth*sBmpInfo.biBitCount+31)/32)*4;
         if(!WvSizeMultiply(stride,height,&pixels) || CheckSize(0,pixels))break;
         Colors=sBmpInfo.biClrUsed;
         if(sBmpInfo.biBitCount<=8) {
          UINT maximum=1U<<sBmpInfo.biBitCount;if(!Colors)Colors=maximum;if(Colors>maximum)break;
         }
         if(Colors>10000000U/sizeof(RGBQUAD))break;
         RgbSize=Colors*sizeof(RGBQUAD);paletteOffset=sizeof(sBmpFile)+(WV_ULONG)header;
         if(!WvSizeAdd(paletteOffset,RgbSize,&paletteEnd) || paletteEnd>sBmpFile.bfOffBits ||
            sBmpFile.bfOffBits>(WV_ULONG)fileSize || pixels>(WV_ULONG)fileSize-sBmpFile.bfOffBits)break;
         if(_llseek(hf,paletteOffset,FILE_BEGIN)!=(WV_LONG)paletteOffset)break;
         BmpSize=pixels;
         /* Extended color/profile fields are not used by the background renderer. */
         sBmpInfo.biSize=sizeof(sBmpInfo);sBmpInfo.biSizeImage=(DWORD)pixels;
        }
		hBmpInfo = GlobalAlloc(GMEM_MOVEABLE, RgbSize+sizeof(sBmpInfo));
		if (!hBmpInfo) break;
		lpBmpInfo = (LPBITMAPINFO)GlobalLock(hBmpInfo);
		if (lpBmpInfo == NULL) break;
		lpBmpInfo->bmiHeader = sBmpInfo;
		if (_lread(hf, lpBmpInfo->bmiColors, RgbSize) != RgbSize) break;

		if (UsePalette) {
			if (!Colors) RgbSize = (6*6*6+15)*sizeof(RGBQUAD);
			pPalette = (LOGPALETTE*)LocalAlloc
							(LPTR, RgbSize + sizeof(LOGPALETTE));
			if (pPalette == NULL) break;
			pPalette->palVersion = 0x300;
			if (!(pPalette->palNumEntries = (int)Colors)) {
				/*no palette available, choose a default palette...*/
				int r, g, b;
				for (r=0; r<6; ++r)
					for (g=0; g<6; ++g)
						for (b=0; b<6; ++b) {
							static BYTE Comp[6] = {0, 51, 102, 153, 204, 255};
							pPalette->palPalEntry[Colors].peRed   = Comp[r];
							pPalette->palPalEntry[Colors].peGreen = Comp[g];
							pPalette->palPalEntry[Colors].peBlue  = Comp[b];
							++Colors;
						}
				/*add some more gray shades...*/
				for (r=0; r<15; ++r) {
					static BYTE Comp[15] = {13,25,38, 63,76,89, 114,127,140,
											165,178,191, 216,229,242};
					pPalette->palPalEntry[Colors].peRed   = Comp[r];
					pPalette->palPalEntry[Colors].peGreen = Comp[r];
					pPalette->palPalEntry[Colors].peBlue  = Comp[r];
					++Colors;
				}
				pPalette->palNumEntries = (int)Colors;
			} else while (Colors--) {
				pPalette->palPalEntry[Colors].peRed =
					lpBmpInfo->bmiColors[Colors].rgbRed;
				pPalette->palPalEntry[Colors].peGreen =
					lpBmpInfo->bmiColors[Colors].rgbGreen;
				pPalette->palPalEntry[Colors].peBlue =
					lpBmpInfo->bmiColors[Colors].rgbBlue;
				pPalette->palPalEntry[Colors].peFlags = 0;
			}
			hPalette[BmpInx] = CreatePalette(pPalette);
			if (!hPalette[BmpInx]) break;
		}

		hPattern = GlobalAlloc(GMEM_MOVEABLE, BmpSize);
		if (!hPattern) break;
		lpPattern = GlobalLock(hPattern);
		if (lpPattern == NULL) break;
		_llseek(hf, sBmpFile.bfOffBits, 0);
		if (_hread(hf, lpPattern, BmpSize) != BmpSize) break;

		if (hPalette[BmpInx]) {
			hOldPalette = SelectPalette(hDC, hPalette[BmpInx], FALSE);
			RealizePalette(hDC);
		}
		hBkgndBmp[BmpInx] = CreateDIBitmap(hDC, &sBmpInfo, CBM_INIT, lpPattern,
										   lpBmpInfo, DIB_RGB_COLORS);
		GetObject(hBkgndBmp[BmpInx], sizeof(sBkgndBmp[0]), &sBkgndBmp[BmpInx]);
		if (hPalette[BmpInx]) SelectPalette(hDC, hOldPalette, FALSE);
	} while (FALSE);
	if (lpPattern)		   GlobalUnlock(hPattern);
	if (hPattern)		   GlobalFree(hPattern);
	if (lpBmpInfo)		   GlobalUnlock(hBmpInfo);
	if (hBmpInfo)		   GlobalFree(hBmpInfo);
	if (pPalette)		   LocalFree((HLOCAL)pPalette);
	if (hf != HFILE_ERROR) _lclose(hf);
	return (hBkgndBmp[BmpInx] != 0);
}

void ReleaseBmp(int BmpInx) {
	if (hPalette[BmpInx]) {
		DeleteObject(hPalette[BmpInx]);
		hPalette[BmpInx] = NULL;
	}
	if (hBkgndBmp[BmpInx]) {
		DeleteObject(hBkgndBmp[BmpInx]);
		hBkgndBmp[BmpInx] = NULL;
	}
	Init[BmpInx] = Fail[BmpInx] = FALSE;
}

BOOL PaintBmp(HDC hDC, int BmpInx, PRECT Rect, WV_LONG xOff, WV_LONG yOff) {
	HDC			hCompDC;
	int			sy, dx, dy, xStart;
	HBITMAP		hOldBmp;
	HPALETTE	hOldPalette;

	PARAM_NOT_USED(xOff);
	if (!Init[BmpInx]) {
		if (Fail[BmpInx] || (Fail[BmpInx]=!InitBmp(hDC, BmpInx), Fail[BmpInx]))
			return (FALSE);
		Init[BmpInx] = TRUE;
	}
	if (!hBkgndBmp[BmpInx] || sBkgndBmp[BmpInx].bmWidth <=0 ||
							  sBkgndBmp[BmpInx].bmHeight<=0 ||
							  (hCompDC = CreateCompatibleDC(hDC), !hCompDC))
		return (FALSE);
	hOldBmp = SelectObject(hCompDC, hBkgndBmp[BmpInx]);
	if (hPalette[BmpInx]) {
		hOldPalette = SelectPalette(hDC, hPalette[BmpInx], FALSE);
		RealizePalette(hDC);
	}
	sy = (int)(yOff % sBkgndBmp[BmpInx].bmHeight);
	{	extern WV_LONG xOffset;
		xStart = -(int)(xOffset % sBkgndBmp[BmpInx].bmWidth);
		if (WinVersion >= MAKEWORD(95,3)) xStart += 2;
	}
	dy = Rect->top;
	while (dy < Rect->bottom) {
		int Height = sBkgndBmp[BmpInx].bmHeight - sy;
		if (dy + Height > Rect->bottom) Height = Rect->bottom - dy;
		dx = xStart;
		while (dx < Rect->right) {
			int Width = sBkgndBmp[BmpInx].bmWidth;
			if (dx + Width > Rect->left) {
				if (dx + Width > Rect->right) Width = Rect->right - dx;
				BitBlt(hDC, dx, dy, Width, Height, hCompDC, 0, sy, SRCCOPY);
			}
			dx += Width;
		}
		dy += Height;
		sy = 0;
	}
	if (hPalette[BmpInx]) SelectPalette(hDC, hOldPalette, FALSE);
	SelectObject(hCompDC, hOldBmp);
	DeleteDC(hCompDC);
	return (TRUE);
}
