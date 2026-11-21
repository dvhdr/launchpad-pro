/*
 * STM32F103 implementation of platform.h (firmware only; the host tests use
 * the stand-in in tests/stubs/stubs.c).
 */
#include "platform/platform.h"
#include <stdint.h>

/* 96-bit unique device ID (RM0008 §30.2), byte-addressable */
#define UID_BASE 0x1FFFF7E8u
#define UID_BYTES 12

void platform_product_instance_id(char out[25])
{
	static const char hex[16] = "0123456789ABCDEF";
	const volatile uint8_t *uid = (const volatile uint8_t *)UID_BASE;
	int i;

	for (i = 0; i < UID_BYTES; i++) {
		uint8_t b = uid[i];
		out[2 * i] = hex[b >> 4];
		out[2 * i + 1] = hex[b & 0xF];
	}
	out[2 * UID_BYTES] = 0;
}
