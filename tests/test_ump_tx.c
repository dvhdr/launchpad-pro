#include "test.h"
#include "ump/ump.h"
#include "stubs/stubs.h"

static void test_tx_priority_and_le(void)
{
	uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0}, on[2] = {0x40903C00u, 0xFFFF0000u};
	uint8_t pkt[64];
	ump_tx_reset();
	ump_tx_push_attack(on, 2);
	ump_tx_push(UMP_TX_CONTROL, ctl, 4);
	CHECK_EQ(ump_tx_fill_packet(pkt), 24);
	CHECK_EQ(pkt[0], 0x01);
	CHECK_EQ(pkt[3], 0xF0); /* control first, LE */
	CHECK_EQ(pkt[16], 0x00);
	CHECK_EQ(pkt[19], 0x40);
	CHECK_EQ(ump_tx_fill_packet(pkt), 0);
}

static void test_tx_pressure_coalesce_and_stale(void)
{
	uint32_t p1[2] = {0x40A03C00u, 1}, p2[2] = {0x40A03C00u, 2}, off[2] = {0x40803C00u, 0};
	uint32_t on[2] = {0x40903C00u, 0xFFFF0000u};
	uint8_t pkt[64];
	ump_tx_reset();
	ump_tx_push_attack(on, 2);
	ump_tx_push_pressure(0, 0, 60, p1);
	ump_tx_push_pressure(0, 0, 60, p2);
	CHECK_EQ(ump_tx_fill_packet(pkt), 16); /* note on + one pressure (=2) */
	CHECK_EQ(pkt[12], 2);
	ump_tx_push_pressure(0, 0, 60, p1);
	ump_tx_push_release(off, 2);
	CHECK_EQ(ump_tx_fill_packet(pkt), 8); /* stale pressure dropped */
}

static void test_tx_saturated_release_always_fits(void)
{
	uint32_t on[2] = {0x40903C00u, 0x80000000u}, off[2] = {0x40803C00u, 0};
	int accepted = 0;
	ump_tx_reset();
	while (ump_tx_push_attack(on, 2) == 0)
		accepted++; /* host not reading */
	CHECK(accepted > 0);
	CHECK(accepted <= UMP_TX_NOTE_WORDS / 4);
	for (int i = 0; i < accepted; i++)
		CHECK_EQ(ump_tx_push_release(off, 2), 0);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK(ump_tx_push_release(off, 2) == -1); /* no matching reserve */
}

static uint32_t g_hook_msg[2] = {0x40B00000u, 0x11111111u};
static int g_hook_fired;

static void hook_push_control(void)
{
	if (g_hook_fired)
		return;
	g_hook_fired = 1;
	ump_tx_push(UMP_TX_CONTROL, g_hook_msg, 2);
}

static void test_tx_push_inside_fill(void)
{
	uint32_t a[2] = {0x40B00001u, 0xAAAAAAAAu}, on[2] = {0x40903C00u, 0xFFFF0000u};
	uint32_t got[16];
	uint8_t pkt[64];
	int n;
	ump_tx_reset();
	ump_tx_push(UMP_TX_CONTROL, a, 2);
	ump_tx_push_attack(on, 2);
	g_hook_fired = 0;
	stub_irq_hook = hook_push_control; /* fires at the start of the fill's irq_save */
	n = ump_tx_fill_packet(pkt);
	stub_irq_hook = 0;
	CHECK(g_hook_fired);
	CHECK_EQ(n, 24); /* a, hook message, note on */
	for (int i = 0; i < 6; i++)
		got[i] = (uint32_t)pkt[4 * i] | ((uint32_t)pkt[4 * i + 1] << 8) |
		         ((uint32_t)pkt[4 * i + 2] << 16) | ((uint32_t)pkt[4 * i + 3] << 24);
	CHECK_EQ(got[0], a[0]);
	CHECK_EQ(got[1], a[1]);
	CHECK_EQ(got[2], g_hook_msg[0]);
	CHECK_EQ(got[3], g_hook_msg[1]);
	CHECK_EQ(got[4], on[0]);
	CHECK_EQ(got[5], on[1]);
	CHECK_EQ(ump_tx_fill_packet(pkt), 0);
	CHECK_EQ(ump_tx_reserve_words(), 2);
}

static void test_tx_sysex_all_or_nothing(void)
{
	static uint32_t big[200];
	uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0};
	uint8_t pkt[64];
	for (int i = 0; i < 200; i += 2)
		big[i] = 0x30000000u; /* 2-word SysEx7 messages */
	ump_tx_reset();
	ump_tx_push(UMP_TX_CONTROL, ctl, 4);
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, big, 200), -1);
	{
		uint32_t pr[2] = {0x40A03C00u, 1};
		CHECK_EQ(ump_tx_push(UMP_TX_PRESSURE, pr, 2), -1);
	}
	CHECK_EQ(ump_tx_fill_packet(pkt), 16); /* only the first message is queued */
	CHECK_EQ(ump_tx_fill_packet(pkt), 0);
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, big, 192), 0); /* exactly full fits */
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, ctl, 4), -1);
}

static void test_tx_midi1_notes_no_reserve_leak(void)
{
	uint32_t on = 0x20903C64u, off = 0x20803C00u;
	uint8_t pkt[64];
	ump_tx_reset();
	for (int i = 0; i < 200; i++) {
		CHECK_EQ(ump_tx_push_attack(&on, 1), 0);
		CHECK_EQ(ump_tx_fill_packet(pkt), 4);
		CHECK_EQ(ump_tx_push_release(&off, 1), 0);
		CHECK_EQ(ump_tx_fill_packet(pkt), 4);
	}
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(ump_tx_push_attack(&on, 1), 0);
}

/* Drains the queue; returns the number of words written to out. */
static uint16_t drain_words(uint32_t *out, uint16_t max)
{
	uint8_t pkt[64];
	uint16_t n = 0;
	uint8_t len;
	while ((len = ump_tx_fill_packet(pkt)) > 0) {
		for (uint8_t i = 0; i < len; i += 4)
			if (n < max)
				out[n++] = (uint32_t)pkt[i] | ((uint32_t)pkt[i + 1] << 8) |
				           ((uint32_t)pkt[i + 2] << 16) | ((uint32_t)pkt[i + 3] << 24);
	}
	return n;
}

static void test_tx_sysex7_matches_pack(void)
{
	static const uint16_t lens[] = {2, 3, 8, 9, 22, 320};
	static uint8_t data[320];
	static uint32_t ref[120], got[120];
	for (unsigned k = 0; k < sizeof lens / sizeof lens[0]; k++) {
		uint16_t len = lens[k], nref, ngot;
		data[0] = 0xF0;
		for (uint16_t i = 1; i + 1 < len; i++)
			data[i] = (uint8_t)((i * 37 + k) & 0x7F);
		data[len - 1] = 0xF7;
		nref = ump_sysex7_pack(ref, 120, 1, data, len);
		CHECK(nref > 0);
		ump_tx_reset();
		CHECK_EQ(ump_tx_push_sysex7(1, data, len), 0);
		ngot = drain_words(got, 120);
		CHECK_EQ(ngot, nref);
		CHECK_MEM(got, ref, 4u * nref);
	}
}

static void test_tx_sysex7_all_or_nothing_ring(void)
{
	uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0};
	static const uint8_t sx8[] = {0xF0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0xF7};
	static const uint8_t sx6[] = {0xF0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 0xF7};
	static const uint8_t no_f7[] = {0xF0, 1, 2};
	static const uint8_t no_f0[] = {1, 2, 0xF7};
	static const uint8_t only_f0[] = {0xF0};
	static const uint8_t empty[] = {0xF0, 0xF7};
	static uint32_t got[200];
	int i;

	uint32_t two[2] = {0x40B00000u, 0};

	ump_tx_reset();
	for (i = 0; i < 38; i++)
		CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, ctl, 4), 0);
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, two, 2), 0);
	/* 38 words free (154 used): app SysEx leaves the stream headroom, so
	 * 6 words fit; sx8 needs 8 words (19 bytes -> 4 packets) */
	CHECK_EQ(ump_tx_push_sysex7(0, sx8, sizeof sx8), -1);
	CHECK_EQ(ump_tx_push_sysex7(0, no_f7, sizeof no_f7), -1);
	CHECK_EQ(ump_tx_push_sysex7(0, no_f0, sizeof no_f0), -1);
	CHECK_EQ(ump_tx_push_sysex7(0, only_f0, sizeof only_f0), -1);
	CHECK_EQ(ump_tx_push_sysex7(0, only_f0, 0), -1);
	/* 18 bytes -> 3 packets = 6 words: exactly fits above the headroom */
	CHECK_EQ(ump_tx_push_sysex7(0, sx6, sizeof sx6), 0);
	CHECK_EQ(ump_tx_push_sysex7(1, empty, 2), -1);		/* would eat the headroom */
	for (i = 0; i < UMP_TX_CONTROL_STREAM_HEADROOM / 4; i++)
		CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, ctl, 4), 0);	/* stream replies use it */
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, two, 2), -1);	/* ring full */
	CHECK_EQ(drain_words(got, 200), UMP_TX_CONTROL_WORDS);
	CHECK_EQ(got[154] >> 20, 0x301);			/* group 0, start */
	CHECK_EQ(got[158] >> 20, 0x303);			/* end */

	/* empty sysex: one complete packet with no bytes */
	CHECK_EQ(ump_tx_push_sysex7(1, empty, 2), 0);
	CHECK_EQ(drain_words(got, 200), 2);
	CHECK_EQ(got[0], 0x31000000u);
	CHECK_EQ(got[1], 0);
}

/* App system messages (clock) stop short of the stream-reply headroom; the
 * stream responder's plain pushes can still use it. */
static void test_tx_control_app_headroom(void)
{
	static const uint32_t clk[1] = {0x10F80000u};
	static const uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0};
	static const uint32_t bad[1] = {0x40903C00u};		/* MT 4 needs 2 words */
	static uint32_t got[200];
	int n = 0, i;

	ump_tx_reset();
	CHECK_EQ(ump_tx_push_control_app(bad, 1), -1);
	while (ump_tx_push_control_app(clk, 1) == 0 && n < 1000)
		n++;
	CHECK_EQ(n, UMP_TX_CONTROL_WORDS - UMP_TX_CONTROL_STREAM_HEADROOM);
	CHECK(UMP_TX_CONTROL_STREAM_HEADROOM >= 28);		/* full Endpoint Discovery reply */
	for (i = 0; i < UMP_TX_CONTROL_STREAM_HEADROOM / 4; i++)
		CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, ctl, 4), 0);
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, clk, 1), -1);
	CHECK_EQ(drain_words(got, 200), UMP_TX_CONTROL_WORDS);
	CHECK_EQ(got[0], clk[0]);
	CHECK_EQ(got[n], ctl[0]);
	CHECK_EQ(ump_tx_push_control_app(clk, 1), 0);		/* room again */
	CHECK_EQ(drain_words(got, 200), 1);
}

static void test_tx_drop_group_pressure(void)
{
	uint32_t p0[2] = {0x40A03C00u, 1}, p1[2] = {0x41A03C00u, 2}, p2[2] = {0x40D10000u, 3};
	uint32_t got[16];
	ump_tx_reset();
	ump_tx_push_pressure(0, 0, 60, p0);
	ump_tx_push_pressure(1, 0, 60, p1);
	ump_tx_push_pressure(0, 1, 0xFF, p2);
	ump_tx_drop_group_pressure(0);
	CHECK_EQ(drain_words(got, 16), 2);
	CHECK_EQ(got[0], p1[0]);
	CHECK_EQ(got[1], p1[1]);
	ump_tx_push_pressure(0, 0, 60, p0);			/* slots are free again */
	ump_tx_drop_group_pressure(1);
	CHECK_EQ(drain_words(got, 16), 2);
	CHECK_EQ(got[0], p0[0]);
}

void test_ump_tx_all(void)
{
	RUN(test_tx_sysex7_matches_pack);
	RUN(test_tx_sysex7_all_or_nothing_ring);
	RUN(test_tx_control_app_headroom);
	RUN(test_tx_drop_group_pressure);
	RUN(test_tx_priority_and_le);
	RUN(test_tx_pressure_coalesce_and_stale);
	RUN(test_tx_saturated_release_always_fits);
	RUN(test_tx_push_inside_fill);
	RUN(test_tx_sysex_all_or_nothing);
	RUN(test_tx_midi1_notes_no_reserve_leak);
}
