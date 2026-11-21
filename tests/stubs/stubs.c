#include "stubs.h"
#include "app.h"
#include <string.h>

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

struct stub_midi_ev stub_midi_log[64];
int stub_midi_n;
int stub_midi_total;
struct stub_midi_ev stub_midi_last;
uint8_t stub_sysex_buf[320];
uint16_t stub_sysex_len;
uint8_t stub_sysex_port;
int stub_sysex_calls;

void stubs_reset(void)
{
	stub_midi_n = 0;
	stub_midi_total = 0;
	stub_sysex_len = 0;
	stub_sysex_port = 0;
	stub_sysex_calls = 0;
	memset(stub_sysex_buf, 0, sizeof stub_sysex_buf);
}

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
	stub_midi_total++;
	stub_midi_last.port = port;
	stub_midi_last.status = status;
	stub_midi_last.d1 = d1;
	stub_midi_last.d2 = d2;
	if (stub_midi_n < 64) {
		stub_midi_log[stub_midi_n].port = port;
		stub_midi_log[stub_midi_n].status = status;
		stub_midi_log[stub_midi_n].d1 = d1;
		stub_midi_log[stub_midi_n].d2 = d2;
		stub_midi_n++;
	}
}

void app_sysex_event(u8 port, u8 *data, u16 count)
{
	stub_sysex_calls++;
	stub_sysex_port = port;
	stub_sysex_len = count;
	if (count > sizeof stub_sysex_buf)
		count = sizeof stub_sysex_buf;
	memcpy(stub_sysex_buf, data, count);
}
