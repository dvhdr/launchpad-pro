#include "ump/ump.h"
#include "platform/irq.h"
#include "app.h"

static uint32_t ring[UMP_RX_WORDS];
static volatile uint16_t head; /* written by push (interrupt) */
static volatile uint16_t tail; /* written by poll (main loop) */
static volatile uint32_t overflows;

/* owned by ump_rx_poll */
static uint32_t msg[4];
static uint8_t have;
static uint8_t need;
static struct ump_sysex7_rx sysex[2];

void ump_rx_reset(void)
{
	uint32_t s = irq_save();
	head = 0;
	tail = 0;
	overflows = 0;
	have = 0;
	need = 0;
	ump_sysex7_rx_reset(&sysex[0]);
	ump_sysex7_rx_reset(&sysex[1]);
	irq_restore(s);
}

uint16_t ump_rx_free_words(void)
{
	uint32_t s = irq_save();
	uint16_t used = (uint16_t)(head - tail);
	irq_restore(s);
	return (uint16_t)(UMP_RX_WORDS - used);
}

void ump_rx_push_words(const uint32_t *w, uint16_t n)
{
	uint32_t s = irq_save();
	uint16_t h = head;
	uint16_t t = tail;
	for (uint16_t i = 0; i < n; i++) {
		if ((uint16_t)(h - t) >= UMP_RX_WORDS) {
			overflows++;
			continue;
		}
		ring[h % UMP_RX_WORDS] = w[i];
		h++;
	}
	head = h;
	irq_restore(s);
}

uint32_t ump_rx_overflows(void)
{
	return overflows;
}

static void dispatch(void)
{
	uint8_t mt = (uint8_t)(msg[0] >> 28);
	uint8_t group = (uint8_t)((msg[0] >> 24) & 0xF);

	if (mt == 0xF) {
		ump_stream_handle(msg);
		return;
	}
	if (group > 1)
		return;
	if (mt == 1 || mt == 2 || mt == 4) {
		uint8_t out[8][3];
		uint8_t n = ump_to_midi1(msg, out, 8);
		for (uint8_t i = 0; i < n; i++)
			app_midi_event(group, out[i][0], out[i][1], out[i][2]);
	} else if (mt == 3) {
		struct ump_sysex7_rx *rx = &sysex[group];
		if (ump_sysex7_rx_feed(rx, msg) == 1) {
			/* stage 3: MIDI-CI intercept */
			app_sysex_event(group, rx->buf, rx->len);
			ump_sysex7_rx_reset(rx);
		}
	}
}

void ump_rx_poll(void)
{
	for (;;) {
		uint32_t s = irq_save();
		uint16_t t = tail;
		uint16_t h = head;
		uint32_t w = 0;
		int got = (h != t);
		if (got) {
			w = ring[t % UMP_RX_WORDS];
			tail = (uint16_t)(t + 1);
		}
		irq_restore(s);
		if (!got)
			return;

		if (have == 0) {
			need = ump_word_count(w);
			if (need == 0 || need > 4)
				need = 1;
		}
		msg[have++] = w;
		if (have == need) {
			have = 0;
			dispatch(); /* outside the critical section */
		}
	}
}
