#include "test.h"
#include "ump/ump.h"

/* ---- helpers on top of the real TX queue ---- */

static uint32_t g_log[256];
static int g_log_words;

static void drain(void)
{
	uint8_t pkt[64];
	uint8_t n;
	while ((n = ump_tx_fill_packet(pkt)) != 0) {
		for (int i = 0; i + 3 < n && g_log_words < 256; i += 4)
			g_log[g_log_words++] = (uint32_t)pkt[i] | ((uint32_t)pkt[i + 1] << 8) |
					       ((uint32_t)pkt[i + 2] << 16) | ((uint32_t)pkt[i + 3] << 24);
	}
}

static void fake_tx_clear(void)
{
	ump_tx_reset();
	g_log_words = 0;
}

/* Returns the offset of message msg in the log, or -1. */
static int msg_offset(int msg)
{
	int pos = 0;
	drain();
	for (int m = 0; pos < g_log_words; m++) {
		if (m == msg)
			return pos;
		pos += ump_word_count(g_log[pos]);
	}
	return -1;
}

static int fake_tx_msg_count(void)
{
	int pos = 0, n = 0;
	drain();
	while (pos < g_log_words) {
		pos += ump_word_count(g_log[pos]);
		n++;
	}
	return n;
}

static uint32_t fake_tx_word(int msg, int word)
{
	int off = msg_offset(msg);
	return off < 0 ? 0xDEADBEEFu : g_log[off + word];
}

static uint8_t g_hook_last = 0xEE;
static void proto_hook(uint8_t old_proto)
{
	g_hook_last = old_proto;
}
static uint8_t fake_proto_hook_last(void)
{
	return g_hook_last;
}

/* ---- tests ---- */

static void test_endpoint_discovery_all(void)
{
	uint32_t req[4] = {0xF0000101u, 0x0000001Fu, 0, 0};
	fake_tx_clear(); ump_stream_reset(); ump_stream_handle(req);
	/* replies in order: info, identity, name(2), product id, stream config */
	CHECK_EQ(fake_tx_msg_count(), 6);
	CHECK_EQ(fake_tx_word(0,0), 0xF0010101u); CHECK_EQ(fake_tx_word(0,1), 0x82000300u);
	CHECK_EQ(fake_tx_word(1,0), 0xF0020000u); CHECK_EQ(fake_tx_word(1,1), 0x00002029u);
	CHECK_EQ(fake_tx_word(1,2), 0x51000000u); CHECK_EQ(fake_tx_word(1,3), 0x00010000u);
	CHECK_EQ(fake_tx_word(2,0), 0xF4034C61u); CHECK_EQ(fake_tx_word(2,1), 0x756E6368u);
	CHECK_EQ(fake_tx_word(2,2), 0x70616420u); CHECK_EQ(fake_tx_word(2,3), 0x50726F20u);
	CHECK_EQ(fake_tx_word(3,0), 0xFC034D49u); CHECK_EQ(fake_tx_word(3,1), 0x44492032u);
	CHECK_EQ(fake_tx_word(3,2), 0x2E300000u); CHECK_EQ(fake_tx_word(3,3), 0);
	CHECK_EQ(fake_tx_word(4,0), 0xF0045445u); CHECK_EQ(fake_tx_word(4,1), 0x53543030u);
	CHECK_EQ(fake_tx_word(4,2), 0x30310000u);
	CHECK_EQ(fake_tx_word(5,0), 0xF0060200u);
}

static void test_endpoint_discovery_filter(void)
{
	uint32_t req[4] = {0xF0000101u, 0x00000001u, 0, 0};   /* info only */
	fake_tx_clear(); ump_stream_handle(req); CHECK_EQ(fake_tx_msg_count(), 1);
	uint32_t req2[4] = {0xF0000101u, 0x00000010u, 0, 0};  /* stream config only */
	fake_tx_clear(); ump_stream_handle(req2);
	CHECK_EQ(fake_tx_msg_count(), 1); CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
}

static void test_fb_discovery(void)
{
	uint32_t all[4] = {0xF010FF03u, 0, 0, 0};             /* all blocks, info+name */
	fake_tx_clear(); ump_stream_handle(all);
	CHECK_EQ(fake_tx_word(0,0), 0xF0118033u); CHECK_EQ(fake_tx_word(0,1), 0x00010000u);
	/* FB0 name: 15 bytes -> start + end packets; then FB1 info, FB1 name (complete) */
	CHECK_EQ(fake_tx_msg_count(), 5);
	CHECK_EQ(fake_tx_word(3,0), 0xF011813Bu); CHECK_EQ(fake_tx_word(3,1), 0x01010000u);
	uint32_t one[4] = {0xF0100101u, 0, 0, 0};             /* FB1, info only */
	fake_tx_clear(); ump_stream_handle(one);
	CHECK_EQ(fake_tx_msg_count(), 1); CHECK_EQ(fake_tx_word(0,0), 0xF011813Bu);
	uint32_t none[4] = {0xF0100203u, 0, 0, 0};            /* FB2 doesn't exist */
	fake_tx_clear(); ump_stream_handle(none); CHECK_EQ(fake_tx_msg_count(), 0);
}

static void test_fb_name_round_trip(void)
{
	uint32_t req[4] = {0xF0100002u, 0, 0, 0};             /* FB0, name only */
	char name[32];
	int len = 0;
	fake_tx_clear(); ump_stream_handle(req);
	CHECK_EQ(fake_tx_msg_count(), 2);
	for (int m = 0; m < fake_tx_msg_count(); m++) {
		uint32_t w0 = fake_tx_word(m, 0);
		CHECK_EQ((w0 >> 16) & 0x3FF, 0x012);
		CHECK_EQ((w0 >> 8) & 0xFF, 0);
		CHECK_EQ((w0 >> 26) & 3, m == 0 ? 1 : 3);
		name[len++] = (char)(w0 & 0xFF);
		for (int w = 1; w < 4; w++)
			for (int b = 3; b >= 0; b--)
				name[len++] = (char)(fake_tx_word(m, w) >> (b * 8));
	}
	name[len] = 0;
	CHECK(strcmp(name, "Standalone Port") == 0);
}

static void test_stream_config_cycle(void)
{
	uint32_t to1[4] = {0xF0050100u, 0, 0, 0}, to2jr[4] = {0xF0050203u, 0, 0, 0};
	ump_stream_set_protocol_hook(proto_hook);
	ump_stream_reset(); fake_tx_clear();
	CHECK_EQ(ump_stream_handle(to1), 1);
	CHECK_EQ(fake_proto_hook_last(), UMP_PROTO_MIDI2);
	CHECK_EQ(ump_stream_protocol(), UMP_PROTO_MIDI1); CHECK_EQ(fake_tx_word(0,0), 0xF0060100u);
	fake_tx_clear(); CHECK_EQ(ump_stream_handle(to2jr), 1);   /* JR refused */
	CHECK_EQ(fake_proto_hook_last(), UMP_PROTO_MIDI1);
	CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
	fake_tx_clear(); CHECK_EQ(ump_stream_handle(to2jr), 0);   /* already MIDI 2 */
	CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
	ump_stream_reset();                                       /* keeps the hook */
	g_hook_last = 0xEE;
	CHECK_EQ(ump_stream_handle(to1), 1);
	CHECK_EQ(fake_proto_hook_last(), UMP_PROTO_MIDI2);
	ump_stream_set_protocol_hook(0);
	ump_stream_reset();
}

/* A protocol change's notification goes behind queued note traffic (note
 * FIFO); a no-change notification stays in the control ring. */
static void test_stream_config_notify_ordering(void)
{
	uint32_t to1[4] = {0xF0050100u, 0, 0, 0}, to2[4] = {0xF0050200u, 0, 0, 0};
	uint32_t note[2] = {0x40903C00u, 0x80000000u};
	ump_stream_set_protocol_hook(0);
	ump_stream_reset(); fake_tx_clear();
	CHECK_EQ(ump_tx_push(UMP_TX_NOTE, note, 2), 0);
	CHECK_EQ(ump_stream_handle(to2), 0);                      /* no change: control */
	CHECK_EQ(fake_tx_msg_count(), 2);
	CHECK_EQ(fake_tx_word(0,0), 0xF0060200u); CHECK_EQ(fake_tx_word(1,0), note[0]);
	fake_tx_clear();
	CHECK_EQ(ump_tx_push(UMP_TX_NOTE, note, 2), 0);
	CHECK_EQ(ump_stream_handle(to1), 1);                      /* change: after the note */
	CHECK_EQ(fake_tx_msg_count(), 2);
	CHECK_EQ(fake_tx_word(0,0), note[0]); CHECK_EQ(fake_tx_word(1,0), 0xF0060100u);
	/* note FIFO full: the notification falls back to the control ring */
	fake_tx_clear();
	while (ump_tx_push(UMP_TX_NOTE, note, 2) == 0)
		;
	CHECK_EQ(ump_stream_handle(to2), 1);
	CHECK_EQ(fake_tx_word(0,0), 0xF0060200u);
	fake_tx_clear();
	ump_stream_reset();
}

static void test_unknown_status(void)
{
	uint32_t req[4] = {0xF0770000u, 0, 0, 0};
	fake_tx_clear(); ump_stream_reset();
	CHECK_EQ(ump_stream_handle(req), 0);
	CHECK_EQ(fake_tx_msg_count(), 0);
}

void test_ump_stream_all(void)
{
	RUN(test_endpoint_discovery_all);
	RUN(test_endpoint_discovery_filter);
	RUN(test_fb_discovery);
	RUN(test_fb_name_round_trip);
	RUN(test_stream_config_cycle);
	RUN(test_stream_config_notify_ordering);
	RUN(test_unknown_status);
}
