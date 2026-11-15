#ifndef WINVI_COUNTS_H
#define WINVI_COUNTS_H
/* File positions, line counts, selections and undo spans are native 64-bit. */
typedef __int64 WV_LONG;
typedef unsigned __int64 WV_ULONG;
#define WV_MAX_FILE_BYTES (16ULL * 1024 * 1024 * 1024)
#endif
