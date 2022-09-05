#ifndef UTILS_H
#define UTILS_H

#include "app_defs.h"

static u8 quantizerWhiteKeysLookupTable[] =
{
    0, 2, 4, 5, 7, 9, 11, 12
};

static u8 quantizerBlackKeysLookupTable[] =
{
    0, 1, 3, 0, 6, 8, 10, 0
};

static u8 quantizerUiKeyToPadLookupTable[] =
{
    11, 22, 12, 23, 13, 14, 25, 15, 26, 16, 27, 17, 18 
};

u8 isFlagOn32(u32 flags, u8 bit)
{
    return ((flags & (1 << bit)) != 0);
}

u8 isFlagOn16(u16 flags, u8 bit)
{
    return ((flags & (1 << bit)) != 0);
}

u8 isFlagOn8(u8 flags, u8 bit)
{
    return ((flags & (1 << bit)) != 0);
}

#endif