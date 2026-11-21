#ifndef STUBS_H
#define STUBS_H

#include <stdint.h>

extern void (*stub_irq_hook)(void);

struct stub_midi_ev {
	uint8_t port, status, d1, d2;
};
extern struct stub_midi_ev stub_midi_log[64];
extern int stub_midi_n;
extern int stub_midi_total;
extern struct stub_midi_ev stub_midi_last;
extern uint8_t stub_sysex_buf[320];
extern uint16_t stub_sysex_len;
extern uint8_t stub_sysex_port;
extern int stub_sysex_calls;

void stubs_reset(void);

#endif
