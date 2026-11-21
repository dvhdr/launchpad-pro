#ifndef PLATFORM_IRQ_H
#define PLATFORM_IRQ_H

#include <stdint.h>

#ifdef HOST_TEST

/* Defined in tests/stubs/stubs.c. Called at the start of irq_save, before the
 * caller touches any shared state, so tests can simulate an interrupt there. */
extern void (*stub_irq_hook)(void);

static inline uint32_t irq_save(void)
{
	if (stub_irq_hook)
		stub_irq_hook();
	return 0;
}

static inline void irq_restore(uint32_t s)
{
	(void)s;
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
