#include "test.h"
#include "ump/ump.h"

static void test_word_count(void)
{
	CHECK_EQ(ump_word_count(0x00000000u), 1);
	CHECK_EQ(ump_word_count(0x10F80000u), 1);
	CHECK_EQ(ump_word_count(0x20903C64u), 1);
	CHECK_EQ(ump_word_count(0x30160000u), 2);
	CHECK_EQ(ump_word_count(0x40903C00u), 2);
	CHECK_EQ(ump_word_count(0x50000000u), 4);
	CHECK_EQ(ump_word_count(0x60000000u), 1);
	CHECK_EQ(ump_word_count(0x70000000u), 1);
	CHECK_EQ(ump_word_count(0x80000000u), 2);
	CHECK_EQ(ump_word_count(0x90000000u), 2);
	CHECK_EQ(ump_word_count(0xA0000000u), 2);
	CHECK_EQ(ump_word_count(0xB0000000u), 3);
	CHECK_EQ(ump_word_count(0xC0000000u), 3);
	CHECK_EQ(ump_word_count(0xD0000000u), 4);
	CHECK_EQ(ump_word_count(0xE0000000u), 4);
	CHECK_EQ(ump_word_count(0xF0000000u), 4);
}

static void test_scale_up(void)
{
	CHECK_EQ(ump_scale_up(0, 7, 16), 0x0000);
	CHECK_EQ(ump_scale_up(64, 7, 16), 0x8000);
	CHECK_EQ(ump_scale_up(127, 7, 16), 0xFFFF);
	CHECK_EQ(ump_scale_up(127, 7, 32), 0xFFFFFFFFu);
	CHECK_EQ(ump_scale_up(4095, 12, 16), 0xFFFF);
	CHECK_EQ(ump_scale_up(0x2000, 14, 32), 0x80000000u);
	CHECK_EQ(ump_scale_up(1, 7, 16), 0x0200);
	CHECK_EQ(ump_scale_down(0xFFFF, 16, 7), 0x7F);
}

static void test_encoders(void)
{
	uint32_t w[2];
	CHECK_EQ(ump_m2_note_on(w, 0, 0, 60, 0xFFFF), 2);
	CHECK_EQ(w[0], 0x40903C00u); CHECK_EQ(w[1], 0xFFFF0000u);
	ump_m2_note_off(w, 1, 2, 60, 0);  CHECK_EQ(w[0], 0x41823C00u);
	ump_m2_poly_pressure(w, 0, 0, 60, 0x12345678u);
	CHECK_EQ(w[0], 0x40A03C00u); CHECK_EQ(w[1], 0x12345678u);
	ump_m2_channel_pressure(w, 0, 3, 7); CHECK_EQ(w[0], 0x40D30000u); CHECK_EQ(w[1], 7);
}

static void test_from_midi1(void)
{
	uint32_t w[2];
	CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 100, 0), 1); CHECK_EQ(w[0], 0x20903C64u);
	CHECK_EQ(ump_from_midi1(w, 0, 0xF8, 0, 0, 1), 1);    CHECK_EQ(w[0], 0x10F80000u);
	CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 127, 1), 2); CHECK_EQ(w[1] >> 16, 0xFFFF);
	CHECK_EQ(ump_from_midi1(w, 0, 0x90, 60, 0, 1), 2);   /* Note On vel 0 -> Note Off */
	CHECK_EQ((w[0] >> 20) & 0xF, 0x8);                   /* velocity per Appendix D */
	CHECK_EQ(w[1], 0x80000000u);                         /* Appendix D.3.1: 0x8000 */
	CHECK_EQ(ump_from_midi1(w, 0, 0xF0, 0, 0, 1), 0);
	CHECK_EQ(ump_from_midi1(w, 0, 0xF7, 0, 0, 1), 0);
	CHECK_EQ(ump_from_midi1(w, 0, 0x40, 0, 0, 1), 0);
	/* program change: bank valid clear */
	CHECK_EQ(ump_from_midi1(w, 0, 0xC1, 5, 0, 1), 2);
	CHECK_EQ(w[0], 0x40C10000u); CHECK_EQ(w[1], 0x05000000u);
	/* pitch bend centre */
	CHECK_EQ(ump_from_midi1(w, 0, 0xE0, 0x00, 0x40, 1), 2);
	CHECK_EQ(w[0], 0x40E00000u); CHECK_EQ(w[1], 0x80000000u);
	/* CC */
	CHECK_EQ(ump_from_midi1(w, 2, 0xB3, 7, 127, 1), 2);
	CHECK_EQ(w[0], 0x42B30700u); CHECK_EQ(w[1], 0xFFFFFFFFu);
	/* channel pressure */
	CHECK_EQ(ump_from_midi1(w, 0, 0xD0, 127, 0, 1), 2);
	CHECK_EQ(w[0], 0x40D00000u); CHECK_EQ(w[1], 0xFFFFFFFFu);
	/* MIDI 1.0 poly pressure as MT 2 */
	CHECK_EQ(ump_from_midi1(w, 1, 0xA0, 60, 9, 0), 1); CHECK_EQ(w[0], 0x21A03C09u);
}

static void test_to_midi1(void)
{
	uint8_t m[4][3];
	uint32_t on[2] = {0x40903C00u, 0x00010000u}; /* tiny velocity */
	CHECK_EQ(ump_to_midi1(on, m, 4), 1);
	CHECK_EQ(m[0][0], 0x90); CHECK_EQ(m[0][1], 60); CHECK_EQ(m[0][2], 1);
	uint32_t pc[2] = {0x40C00001u, 0x05000203u};          /* program 5, bank 2/3, valid */
	CHECK_EQ(ump_to_midi1(pc, m, 4), 3);
	CHECK_EQ(m[0][1], 0); CHECK_EQ(m[0][2], 2); CHECK_EQ(m[1][1], 32); CHECK_EQ(m[1][2], 3);
	CHECK_EQ(m[2][0], 0xC0); CHECK_EQ(m[2][1], 5);
	uint32_t pn[2] = {0x40F03C00u, 0};                    /* per-note management */
	CHECK_EQ(ump_to_midi1(pn, m, 4), 0);

	uint32_t nc[2] = {0x40C20000u, 0x07000000u};          /* program only */
	CHECK_EQ(ump_to_midi1(nc, m, 4), 1);
	CHECK_EQ(m[0][0], 0xC2); CHECK_EQ(m[0][1], 7);

	uint32_t rpn[2] = {0x40200102u, 0xFFFFFFFFu};         /* RPN bank 1 index 2 */
	CHECK_EQ(ump_to_midi1(rpn, m, 4), 4);
	CHECK_EQ(m[0][0], 0xB0); CHECK_EQ(m[0][1], 101); CHECK_EQ(m[0][2], 1);
	CHECK_EQ(m[1][1], 100); CHECK_EQ(m[1][2], 2);
	CHECK_EQ(m[2][1], 6);   CHECK_EQ(m[2][2], 0x7F);
	CHECK_EQ(m[3][1], 38);  CHECK_EQ(m[3][2], 0x7F);
	uint32_t nrpn[2] = {0x40330304u, 0x80000000u};        /* NRPN ch3 */
	CHECK_EQ(ump_to_midi1(nrpn, m, 4), 4);
	CHECK_EQ(m[0][0], 0xB3); CHECK_EQ(m[0][1], 99); CHECK_EQ(m[0][2], 3);
	CHECK_EQ(m[1][1], 98);   CHECK_EQ(m[1][2], 4);
	CHECK_EQ(m[2][2], 0x40); CHECK_EQ(m[3][2], 0);

	uint32_t pb[2] = {0x40E00000u, 0x80000000u};          /* pitch bend centre */
	CHECK_EQ(ump_to_midi1(pb, m, 4), 1);
	CHECK_EQ(m[0][0], 0xE0); CHECK_EQ(m[0][1], 0); CHECK_EQ(m[0][2], 0x40);

	uint32_t cc[2] = {0x40B10700u, 0xFFFFFFFFu};
	CHECK_EQ(ump_to_midi1(cc, m, 4), 1);
	CHECK_EQ(m[0][0], 0xB1); CHECK_EQ(m[0][1], 7); CHECK_EQ(m[0][2], 0x7F);

	uint32_t cp[2] = {0x40D00000u, 0xFFFFFFFFu};
	CHECK_EQ(ump_to_midi1(cp, m, 4), 1);
	CHECK_EQ(m[0][0], 0xD0); CHECK_EQ(m[0][1], 0x7F);

	uint32_t mt1[1] = {0x10F80000u};
	CHECK_EQ(ump_to_midi1(mt1, m, 4), 1); CHECK_EQ(m[0][0], 0xF8);
	uint32_t mt2[1] = {0x20903C64u};
	CHECK_EQ(ump_to_midi1(mt2, m, 4), 1);
	CHECK_EQ(m[0][0], 0x90); CHECK_EQ(m[0][1], 60); CHECK_EQ(m[0][2], 0x64);

	/* not enough room for a multi-message translation */
	CHECK_EQ(ump_to_midi1(pc, m, 2), 0);
	/* Note Off with vel 0 stays 0, Note On is bumped to 1 */
	uint32_t off[2] = {0x40803C00u, 0x00000000u};
	CHECK_EQ(ump_to_midi1(off, m, 4), 1); CHECK_EQ(m[0][2], 0);
}

static void test_sysex7(void)
{
	uint8_t sx[15] = {0xF0, 1,2,3,4,5,6,7,8,9,10,11,12,13, 0xF7};
	uint32_t w[8]; struct ump_sysex7_rx rx; ump_sysex7_rx_reset(&rx);
	CHECK_EQ(ump_sysex7_pack(w, 8, 1, sx, 15), 6);        /* start 6, continue 6, end 1 */
	CHECK_EQ(w[0] >> 16, 0x3116); CHECK_EQ(w[2] >> 16, 0x3126); CHECK_EQ(w[4] >> 16, 0x3131);
	CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[0]), 0);
	CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[2]), 0);
	CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[4]), 1);
	CHECK_EQ(rx.len, 15); CHECK_MEM(rx.buf, sx, 15);
	CHECK_EQ(ump_sysex7_pack(w, 4, 1, sx, 15), 0);        /* doesn't fit -> nothing */

	/* complete single packet */
	uint8_t s2[5] = {0xF0, 0x7E, 0x7F, 0x06, 0xF7};
	CHECK_EQ(ump_sysex7_pack(w, 8, 0, s2, 5), 2);
	CHECK_EQ(w[0] >> 16, 0x3003); CHECK_EQ(ump_sysex7_rx_feed(&rx, &w[0]), 1);
	CHECK_EQ(rx.len, 5); CHECK_MEM(rx.buf, s2, 5);

	/* error cases */
	uint32_t cont[2] = {0x30210000u | (1u << 8), 0};
	CHECK_EQ(ump_sysex7_rx_feed(&rx, cont), -1);          /* continue without start */
	uint32_t st[2] = {0x30110100u, 0};
	CHECK_EQ(ump_sysex7_rx_feed(&rx, st), 0);
	CHECK_EQ(ump_sysex7_rx_feed(&rx, st), -1);            /* start while active */
	CHECK_EQ(rx.active, 1);                               /* new one begun */
	uint32_t bad[2] = {0x30370000u, 0};
	CHECK_EQ(ump_sysex7_rx_feed(&rx, bad), -1);           /* count > 6 */
	CHECK_EQ(rx.active, 0);
}

static int sysex_roundtrip(uint16_t payload)
{
	static uint8_t sx[330];
	static uint32_t w[200];
	struct ump_sysex7_rx rx;
	uint16_t i, n, words;
	int r = 0, last = 0;
	sx[0] = 0xF0;
	for (i = 0; i < payload; i++)
		sx[1 + i] = (uint8_t)(i & 0x7F);
	sx[1 + payload] = 0xF7;
	words = ump_sysex7_pack(w, 200, 0, sx, (uint16_t)(payload + 2));
	CHECK(words > 0);
	ump_sysex7_rx_reset(&rx);
	for (n = 0; n < words; n += 2) {
		r = ump_sysex7_rx_feed(&rx, &w[n]);
		if (r != 0) last = r;
	}
	if (last == 1) {
		CHECK_EQ(rx.len, payload + 2u);
		CHECK_MEM(rx.buf, sx, payload + 2u);
	}
	return last;
}

static void test_sysex7_limit(void)
{
	/* 318 payload bytes + F0/F7 = 320 -> accepted; 319 payload -> discarded (-1) */
	CHECK_EQ(sysex_roundtrip(318), 1);
	CHECK_EQ(sysex_roundtrip(319), -1);
}

void test_ump_codec_all(void)
{
	RUN(test_word_count);
	RUN(test_scale_up);
	RUN(test_encoders);
	RUN(test_from_midi1);
	RUN(test_to_midi1);
	RUN(test_sysex7);
	RUN(test_sysex7_limit);
}
