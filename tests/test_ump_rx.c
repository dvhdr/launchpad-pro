#include "test.h"
#include "ump/ump.h"
#include "stubs/stubs.h"
#include "app_defs.h"

static void test_rx_skips_unsupported_between_notes(void)
{
	uint32_t w[] = {0x40903C00u, 0xFFFF0000u, 0x50000000u, 1, 2, 3,
			0x45903C00u, 0xFFFF0000u, 0x41903E00u, 0x80000000u};
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(w, 10);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 2);
	CHECK_EQ(stub_midi_log[0].port, USBSTANDALONE);
	CHECK_EQ(stub_midi_log[0].d2, 127);
	CHECK_EQ(stub_midi_log[1].port, USBMIDI);
	CHECK_EQ(stub_midi_log[1].d1, 62);
}

static void test_rx_partial_message_across_pushes(void)
{
	uint32_t a = 0x40903C00u, b = 0xFFFF0000u;
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(&a, 1);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 0);
	ump_rx_push_words(&b, 1);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 1);
}

static void test_rx_interleaved_sysex_groups(void)
{
	/* g0: F0 01 02 | g1: F0 0A | g0 end: 03 F7 | g1 end: 0B F7 (F0/F7 stripped on the wire) */
	uint32_t w[] = {0x30120102u, 0x00000000u,  /* g0 start, 2 bytes: 01 02 */
			0x31110A00u, 0x00000000u,  /* g1 start, 1 byte: 0A */
			0x30310300u, 0x00000000u,  /* g0 end, 1 byte: 03 */
			0x31310B00u, 0x00000000u}; /* g1 end, 1 byte: 0B */
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(w, 4);
	ump_rx_poll();
	CHECK_EQ(stub_sysex_calls, 0);
	ump_rx_push_words(w + 4, 2);
	ump_rx_poll();
	CHECK_EQ(stub_sysex_calls, 1);
	CHECK_EQ(stub_sysex_port, USBSTANDALONE);
	{
		const uint8_t e[] = {0xF0, 1, 2, 3, 0xF7};
		CHECK_EQ(stub_sysex_len, 5);
		CHECK_MEM(stub_sysex_buf, e, 5);
	}
	ump_rx_push_words(w + 6, 2);
	ump_rx_poll();
	CHECK_EQ(stub_sysex_calls, 2);
	CHECK_EQ(stub_sysex_port, USBMIDI);
	{
		const uint8_t e[] = {0xF0, 0x0A, 0x0B, 0xF7};
		CHECK_EQ(stub_sysex_len, 4);
		CHECK_MEM(stub_sysex_buf, e, 4);
	}
}

static void test_rx_stream_routed(void)
{
	uint32_t req[4] = {0xF0000101u, 0x00000001u, 0, 0};
	uint8_t pkt[64];
	uint8_t n;
	stubs_reset();
	ump_tx_reset();
	ump_rx_reset();
	ump_rx_push_words(req, 4);
	ump_rx_poll();
	n = ump_tx_fill_packet(pkt);
	CHECK(n >= 16);
	CHECK_EQ((uint32_t)pkt[0] | ((uint32_t)pkt[1] << 8) | ((uint32_t)pkt[2] << 16) |
		 ((uint32_t)pkt[3] << 24), 0xF0010101u);
	CHECK_EQ(stub_midi_n, 0);
}

static void test_rx_system_clock(void)
{
	uint32_t c = 0x10F80000u;
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(&c, 1);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 1);
	CHECK_EQ(stub_midi_log[0].status, 0xF8);
}

static void test_rx_overflow(void)
{
	uint32_t w[UMP_RX_WORDS + 4];
	for (int i = 0; i < UMP_RX_WORDS; i++)
		w[i] = 0x20900000u | ((uint32_t)i << 8) | 0x7Fu; /* d1 = i */
	for (int i = 0; i < 4; i++) /* would-be dropped: d1 = 100 + i */
		w[UMP_RX_WORDS + i] = 0x20900000u | ((uint32_t)(100 + i) << 8) | 0x7Fu;
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(w, UMP_RX_WORDS + 4);
	CHECK_EQ(ump_rx_free_words(), 0);
	CHECK_EQ(ump_rx_overflows(), 4);
	ump_rx_poll();
	CHECK_EQ(stub_midi_total, UMP_RX_WORDS);
	for (int i = 0; i < 64; i++)
		CHECK_EQ(stub_midi_log[i].d1, i); /* oldest delivered in order */
	CHECK_EQ(stub_midi_last.d1, UMP_RX_WORDS - 1); /* newest (100..103) dropped */
	CHECK_EQ(ump_rx_free_words(), UMP_RX_WORDS);
}

static void test_rx_reset_mid_message_is_isr_safe(void)
{
	uint32_t a[] = {0x40903C00u, 0x30120102u}; /* MT4 first word; SysEx7 g0 start */
	uint32_t b[] = {0x40903E00u, 0xFFFF0000u, 0x30020A0Bu, 0x0u};
	/* a[0] is a partial MT4 (needs 2 words); a[1] would be its 2nd word, so
	   feed the SysEx start in a way that is held: push SysEx start first. */
	uint32_t sx[] = {0x30120102u, 0x00000000u};
	stubs_reset();
	ump_rx_reset();
	ump_rx_push_words(sx, 2);
	ump_rx_push_words(&a[0], 1);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 0);
	CHECK_EQ(stub_sysex_calls, 0);
	ump_rx_reset();
	ump_rx_push_words(b, 4);
	ump_rx_poll();
	CHECK_EQ(stub_midi_n, 1);
	CHECK_EQ(stub_midi_log[0].d1, 62);
	CHECK_EQ(stub_sysex_calls, 1);
	{
		const uint8_t e[] = {0xF0, 0x0A, 0x0B, 0xF7};
		CHECK_EQ(stub_sysex_len, 4);
		CHECK_MEM(stub_sysex_buf, e, 4);
	}
}

void test_ump_rx_all(void)
{
	RUN(test_rx_skips_unsupported_between_notes);
	RUN(test_rx_partial_message_across_pushes);
	RUN(test_rx_interleaved_sysex_groups);
	RUN(test_rx_stream_routed);
	RUN(test_rx_system_clock);
	RUN(test_rx_overflow);
	RUN(test_rx_reset_mid_message_is_isr_safe);
}
