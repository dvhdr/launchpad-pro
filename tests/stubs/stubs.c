#include "stubs.h"

void (*stub_irq_hook)(void) = 0;

#include "platform/platform.h"

void platform_product_instance_id(char out[25])
{
	const char *id = "TEST0001";
	int i = 0;
	while (id[i]) {
		out[i] = id[i];
		i++;
	}
	out[i] = 0;
}
