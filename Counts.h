#ifndef WINVI_COUNTS_H
#define WINVI_COUNTS_H
/* File positions, line counts, selections and undo spans are native 64-bit. */
typedef __int64 WV_LONG;
typedef unsigned __int64 WV_ULONG;
/* Windows file positions and signed distance arithmetic use signed 64-bit values. */
#define WV_MAX_POSITION 0x7fffffffffffffffULL
static __inline int WvSizeAdd(WV_ULONG a, WV_ULONG b, WV_ULONG *out) {
 if (a > WV_MAX_POSITION || b > WV_MAX_POSITION-a) return 0;
 *out=a+b; return 1;
}
static __inline int WvSizeMultiply(WV_ULONG a, WV_ULONG b, WV_ULONG *out) {
 if (a > WV_MAX_POSITION || (b && a > WV_MAX_POSITION/b)) return 0;
 *out=a*b; return 1;
}
static __inline WV_ULONG WvMovementProduct(WV_ULONG a, WV_ULONG b) {
 WV_ULONG result;
 return WvSizeMultiply(a,b,&result) ? result : WV_MAX_POSITION;
}
#endif
