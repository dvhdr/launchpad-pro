#include "ump/ump.h"

uint8_t ump_word_count(uint32_t word0)
{
	switch (word0 >> 28) {
	case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
		return 2;
	case 0xB: case 0xC:
		return 3;
	case 0x5: case 0xD: case 0xE: case 0xF:
		return 4;
	default: /* 0, 1, 2, 6, 7 */
		return 1;
	}
}

uint32_t ump_scale_up(uint32_t v, uint8_t src_bits, uint8_t dst_bits)
{
	uint8_t scale_bits = dst_bits - src_bits;
	uint32_t shifted = v << scale_bits;
	uint32_t center = 1u << (src_bits - 1);
	if (v <= center)
		return shifted;
	uint8_t repeat_bits = src_bits - 1;
	uint32_t repeat = v & ((1u << repeat_bits) - 1);
	repeat = scale_bits > repeat_bits ? repeat << (scale_bits - repeat_bits)
	                                  : repeat >> (repeat_bits - scale_bits);
	while (repeat) {
		shifted |= repeat;
		repeat >>= repeat_bits;
	}
	return shifted;
}

uint32_t ump_scale_down(uint32_t v, uint8_t src_bits, uint8_t dst_bits)
{
	return v >> (src_bits - dst_bits);
}

static uint32_t m2_word0(uint8_t group, uint8_t status, uint8_t ch, uint8_t b2, uint8_t b3)
{
	return (4u << 28) | ((uint32_t)(group & 0xF) << 24) | ((uint32_t)(status & 0xF) << 20) |
	       ((uint32_t)(ch & 0xF) << 16) | ((uint32_t)b2 << 8) | b3;
}

uint8_t ump_m2_note_on(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel)
{
	out[0] = m2_word0(group, 0x9, ch, note & 0x7F, 0);
	out[1] = (uint32_t)vel << 16;
	return 2;
}

uint8_t ump_m2_note_off(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint16_t vel)
{
	out[0] = m2_word0(group, 0x8, ch, note & 0x7F, 0);
	out[1] = (uint32_t)vel << 16;
	return 2;
}

uint8_t ump_m2_poly_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint8_t note, uint32_t value)
{
	out[0] = m2_word0(group, 0xA, ch, note & 0x7F, 0);
	out[1] = value;
	return 2;
}

uint8_t ump_m2_channel_pressure(uint32_t out[2], uint8_t group, uint8_t ch, uint32_t value)
{
	out[0] = m2_word0(group, 0xD, ch, 0, 0);
	out[1] = value;
	return 2;
}

uint8_t ump_from_midi1(uint32_t out[2], uint8_t group, uint8_t status, uint8_t d1, uint8_t d2,
                       uint8_t as_midi2)
{
	uint8_t kind = status >> 4;
	uint8_t ch = status & 0xF;
	uint16_t bend;

	d1 &= 0x7F;
	d2 &= 0x7F;

	if (status < 0x80 || status == 0xF0 || status == 0xF7)
		return 0;

	if (status >= 0xF1) {
		out[0] = (1u << 28) | ((uint32_t)(group & 0xF) << 24) | ((uint32_t)status << 16) |
		         ((uint32_t)d1 << 8) | d2;
		return 1;
	}

	if (!as_midi2) {
		out[0] = (2u << 28) | ((uint32_t)(group & 0xF) << 24) | ((uint32_t)status << 16) |
		         ((uint32_t)d1 << 8) | d2;
		return 1;
	}

	out[1] = 0;
	switch (kind) {
	case 0x8:
		out[0] = m2_word0(group, 0x8, ch, d1, 0);
		out[1] = ump_scale_up(d2, 7, 16) << 16;
		break;
	case 0x9:
		if (d2 == 0) { /* Appendix D.3.1: Note On vel 0 -> Note Off, velocity 0x8000 */
			out[0] = m2_word0(group, 0x8, ch, d1, 0);
			out[1] = 0x8000u << 16;
		} else {
			out[0] = m2_word0(group, 0x9, ch, d1, 0);
			out[1] = ump_scale_up(d2, 7, 16) << 16;
		}
		break;
	case 0xA:
		out[0] = m2_word0(group, 0xA, ch, d1, 0);
		out[1] = ump_scale_up(d2, 7, 32);
		break;
	case 0xB:
		out[0] = m2_word0(group, 0xB, ch, d1, 0);
		out[1] = ump_scale_up(d2, 7, 32);
		break;
	case 0xC: /* bank valid clear, bank fields zero (Appendix D.3.4) */
		out[0] = m2_word0(group, 0xC, ch, 0, 0);
		out[1] = (uint32_t)d1 << 24;
		break;
	case 0xD:
		out[0] = m2_word0(group, 0xD, ch, 0, 0);
		out[1] = ump_scale_up(d1, 7, 32);
		break;
	case 0xE:
		bend = (uint16_t)(d1 | (d2 << 7));
		out[0] = m2_word0(group, 0xE, ch, 0, 0);
		out[1] = ump_scale_up(bend, 14, 32);
		break;
	default:
		return 0;
	}
	return 2;
}

static void put3(uint8_t out[3], uint8_t a, uint8_t b, uint8_t c)
{
	out[0] = a;
	out[1] = b;
	out[2] = c;
}

uint8_t ump_to_midi1(const uint32_t *msg, uint8_t out[][3], uint8_t max_out)
{
	uint32_t w0 = msg[0];
	uint8_t mt = (uint8_t)(w0 >> 28);
	uint8_t status, ch, b2, b3, vel, cc;
	uint32_t w1, bend;

	if (mt == 1 || mt == 2) {
		if (max_out < 1)
			return 0;
		put3(out[0], (uint8_t)(w0 >> 16), (uint8_t)(w0 >> 8), (uint8_t)w0);
		return 1;
	}
	if (mt != 4)
		return 0;

	w1 = msg[1];
	status = (uint8_t)((w0 >> 20) & 0xF);
	ch = (uint8_t)((w0 >> 16) & 0xF);
	b2 = (uint8_t)((w0 >> 8) & 0x7F);
	b3 = (uint8_t)(w0 & 0x7F);

	switch (status) {
	case 0x8:
	case 0x9:
		if (max_out < 1)
			return 0;
		vel = (uint8_t)((w1 >> 16) >> 9);
		if (status == 0x9 && vel == 0)
			vel = 1;
		put3(out[0], (uint8_t)((status << 4) | ch), b2, vel);
		return 1;
	case 0xA:
	case 0xB:
		if (max_out < 1)
			return 0;
		put3(out[0], (uint8_t)((status << 4) | ch), b2, (uint8_t)(w1 >> 25));
		return 1;
	case 0xC:
		if (w0 & 1) {
			if (max_out < 3)
				return 0;
			put3(out[0], (uint8_t)(0xB0 | ch), 0, (uint8_t)((w1 >> 8) & 0x7F));
			put3(out[1], (uint8_t)(0xB0 | ch), 32, (uint8_t)(w1 & 0x7F));
			put3(out[2], (uint8_t)(0xC0 | ch), (uint8_t)((w1 >> 24) & 0x7F), 0);
			return 3;
		}
		if (max_out < 1)
			return 0;
		put3(out[0], (uint8_t)(0xC0 | ch), (uint8_t)((w1 >> 24) & 0x7F), 0);
		return 1;
	case 0xD:
		if (max_out < 1)
			return 0;
		put3(out[0], (uint8_t)(0xD0 | ch), (uint8_t)(w1 >> 25), 0);
		return 1;
	case 0xE:
		if (max_out < 1)
			return 0;
		bend = w1 >> 18;
		put3(out[0], (uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)(bend >> 7));
		return 1;
	case 0x2: /* RPN */
	case 0x3: /* NRPN */
		if (max_out < 4)
			return 0;
		cc = (status == 0x2) ? 101 : 99; /* MSB; LSB is cc - 1 */
		put3(out[0], (uint8_t)(0xB0 | ch), cc, b2);
		put3(out[1], (uint8_t)(0xB0 | ch), (uint8_t)(cc - 1), b3);
		put3(out[2], (uint8_t)(0xB0 | ch), 6, (uint8_t)(w1 >> 25));
		put3(out[3], (uint8_t)(0xB0 | ch), 38, (uint8_t)((w1 >> 18) & 0x7F));
		return 4;
	default: /* per-note controllers, per-note management, etc. */
		return 0;
	}
}

uint16_t ump_sysex7_pack(uint32_t *out, uint16_t max_words, uint8_t group, const uint8_t *data,
                         uint16_t len)
{
	uint16_t packets, i, p;
	uint32_t n;

	if (len && data[0] == 0xF0) {
		data++;
		len--;
	}
	if (len && data[len - 1] == 0xF7)
		len--;

	packets = len ? (uint16_t)((len + 5) / 6) : 1;
	n = (uint32_t)packets * 2;
	if (n > max_words)
		return 0;

	for (p = 0; p < packets; p++) {
		uint8_t b[6] = {0, 0, 0, 0, 0, 0};
		uint16_t remaining = (uint16_t)(len - p * 6);
		uint8_t cnt = remaining > 6 ? 6 : (uint8_t)remaining;
		uint8_t st;

		if (packets == 1)
			st = 0;
		else if (p == 0)
			st = 1;
		else if (p == packets - 1)
			st = 3;
		else
			st = 2;
		for (i = 0; i < cnt; i++)
			b[i] = data[p * 6 + i] & 0x7F;
		out[p * 2] = (3u << 28) | ((uint32_t)(group & 0xF) << 24) | ((uint32_t)st << 20) |
		             ((uint32_t)cnt << 16) | ((uint32_t)b[0] << 8) | b[1];
		out[p * 2 + 1] = ((uint32_t)b[2] << 24) | ((uint32_t)b[3] << 16) |
		                 ((uint32_t)b[4] << 8) | b[5];
	}
	return (uint16_t)n;
}

void ump_sysex7_rx_reset(struct ump_sysex7_rx *rx)
{
	rx->len = 0;
	rx->active = 0;
}

int ump_sysex7_rx_feed(struct ump_sysex7_rx *rx, const uint32_t msg[2])
{
	uint32_t w0 = msg[0], w1 = msg[1];
	uint8_t st = (uint8_t)((w0 >> 20) & 0xF);
	uint8_t cnt = (uint8_t)((w0 >> 16) & 0xF);
	uint8_t b[6];
	uint8_t i;
	int rc = 0;

	if ((w0 >> 28) != 3 || cnt > 6 || st > 3) {
		ump_sysex7_rx_reset(rx);
		return -1;
	}
	b[0] = (uint8_t)(w0 >> 8);
	b[1] = (uint8_t)w0;
	b[2] = (uint8_t)(w1 >> 24);
	b[3] = (uint8_t)(w1 >> 16);
	b[4] = (uint8_t)(w1 >> 8);
	b[5] = (uint8_t)w1;

	if (st == 0 || st == 1) {
		if (st == 1 && rx->active)
			rc = -1; /* start while active: discard old, begin new */
		rx->buf[0] = 0xF0;
		rx->len = 1;
		rx->active = 1;
	} else if (!rx->active) {
		return -1; /* continue/end without start */
	}

	/* reserve room for the trailing F7 */
	if ((uint32_t)rx->len + cnt + 1 > sizeof(rx->buf)) {
		ump_sysex7_rx_reset(rx);
		return -1;
	}
	for (i = 0; i < cnt; i++)
		rx->buf[rx->len++] = b[i] & 0x7F;

	if (st == 0 || st == 3) {
		rx->buf[rx->len++] = 0xF7;
		rx->active = 0;
		return 1;
	}
	return rc;
}
