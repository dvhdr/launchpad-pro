#include "app_defs.h"

u8 isFlagOn32(u32 flags, u8 bit)
{
    return ((flags & (1 << bit)) != 0);
}

u8 isFlagOn8(u8 flags, u8 bit)
{
    return ((flags & (1 << bit)) != 0);
}