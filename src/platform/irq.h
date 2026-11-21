#ifndef PLATFORM_IRQ_H
#define PLATFORM_IRQ_H

#include <stdint.h>

#ifdef HOST_TEST

/* Defined in tests/stubs/stubs.c. The hook is called at the start of
 * irq_save, before the caller touches any shared state, so tests can simulate
 * an interrupt there. The depth counts open irq_save sections. */
extern void (*stub_irq_hook)(void);
extern int stub_irq_depth;

static inline uint32_t irq_save(void)
{
	if (stub_irq_hook)
		stub_irq_hook();
	stub_irq_depth++;
	return 0;
}

static inline void irq_restore(uint32_t s)
{
	(void)s;
	stub_irq_depth--;
}

#else

static inline uint32_t irq_save(void)
{
	uint32_t primask;
	__asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) : : "memory");
	return primask;
}

static inline void irq_restore(uint32_t s)
{
	__asm__ volatile("msr primask, %0" : : "r"(s) : "memory");
}

#endif

#endif
