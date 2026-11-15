#include <windows.h>
#include <stdio.h>
#include "../Counts.h"
#include "../Page.h"
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)
int main(void) {
 WV_ULONG n=123; PAGE p={0};
 CHECK(sizeof(p.PageNo)==8 && sizeof(p.PageFilePos)==8 && sizeof(p.Newlines)==8);
 p.PageNo=4294967296ULL;CHECK(p.PageNo+1==4294967297ULL);
 p.Newlines=8589934593ULL;CHECK(p.Newlines+1==8589934594ULL);
 CHECK(WvSizeAdd(17179869184ULL,1,&n) && n==17179869185ULL);
 CHECK(WvSizeAdd(1099511627776ULL,1099511627776ULL,&n) && n==2199023255552ULL);
 CHECK(WvSizeAdd(WV_MAX_POSITION-1,1,&n) && n==WV_MAX_POSITION);
 CHECK(!WvSizeAdd(WV_MAX_POSITION,1,&n));
 CHECK(!WvSizeAdd(~0ULL,1,&n));
 CHECK(WvSizeMultiply(4294967296ULL,4096,&n) && n==17592186044416ULL);
 CHECK(!WvSizeMultiply(WV_MAX_POSITION,2,&n));
 CHECK(WvSizeMultiply(0,WV_MAX_POSITION,&n) && n==0);
 CHECK(WvMovementProduct(WV_MAX_POSITION,16)==WV_MAX_POSITION);
 puts("PASS 64-bit metadata, checked addition/multiplication and signed-position boundaries");return 0;
}
