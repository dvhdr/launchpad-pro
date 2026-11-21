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
	CHECK_EQ(ump_tx_push(UMP_TX_PRESSURE, ctl, 2), -1);
	CHECK_EQ(ump_tx_fill_packet(pkt), 16); /* only the first message is queued */
	CHECK_EQ(ump_tx_fill_packet(pkt), 0);
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, big, 192), 0); /* exactly full fits */
	CHECK_EQ(ump_tx_push(UMP_TX_CONTROL, ctl, 4), -1);
}

void test_ump_tx_all(void)
{
	RUN(test_tx_priority_and_le);
	RUN(test_tx_pressure_coalesce_and_stale);
	RUN(test_tx_saturated_release_always_fits);
	RUN(test_tx_push_inside_fill);
	RUN(test_tx_sysex_all_or_nothing);
}
