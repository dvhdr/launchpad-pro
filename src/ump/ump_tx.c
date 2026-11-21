#include "ump.h"
#include "platform/irq.h"

/* Every public operation calls irq_save() first, before reading or modifying
 * any queue state, and irq_restore() last. */

struct ring {
	uint32_t *buf;
	uint16_t size;
	uint16_t head;
	uint16_t count;
};

struct pressure_slot {
	uint8_t used, group, ch, note;
	uint32_t w[2];
};

static uint32_t g_ctl_buf[UMP_TX_CONTROL_WORDS];
static uint32_t g_note_buf[UMP_TX_NOTE_WORDS];
static struct ring g_ctl = {g_ctl_buf, UMP_TX_CONTROL_WORDS, 0, 0};
static struct ring g_note = {g_note_buf, UMP_TX_NOTE_WORDS, 0, 0};
static struct pressure_slot g_pressure[UMP_TX_PRESSURE_SLOTS];
static uint16_t g_reserve;

static void ring_put(struct ring *r, const uint32_t *msg, uint8_t words)
{
	uint16_t tail = (uint16_t)((r->head + r->count) % r->size);
	for (uint8_t i = 0; i < words; i++) {
		r->buf[tail] = msg[i];
		tail = (uint16_t)((tail + 1) % r->size);
	}
	r->count = (uint16_t)(r->count + words);
}

/* Words of the message at the head of r, clamped to what is queued (never 0 if non-empty). */
static uint8_t ring_head_words(const struct ring *r)
{
	uint8_t wc = ump_word_count(r->buf[r->head]);
	return wc > r->count ? (uint8_t)r->count : wc;
}

static void ring_take(struct ring *r, uint32_t *out, uint8_t words)
{
	for (uint8_t i = 0; i < words; i++) {
		out[i] = r->buf[r->head];
		r->head = (uint16_t)((r->head + 1) % r->size);
	}
	r->count = (uint16_t)(r->count - words);
}

/* True if msg[0..words) is a whole number of complete UMPs. */
static int msgs_valid(const uint32_t *msg, uint8_t words)
{
	uint16_t pos = 0;
	if (words == 0)
		return 0;
	while (pos < words)
		pos = (uint16_t)(pos + ump_word_count(msg[pos]));
	return pos == words;
}

void ump_tx_reset(void)
{
	uint32_t s = irq_save();
	g_ctl.head = g_ctl.count = 0;
	g_note.head = g_note.count = 0;
	g_reserve = 0;
	for (int i = 0; i < UMP_TX_PRESSURE_SLOTS; i++)
		g_pressure[i].used = 0;
	irq_restore(s);
}

int ump_tx_push(enum ump_tx_class cls, const uint32_t *msg, uint8_t words)
{
	int rc = -1;
	uint32_t s = irq_save();
	if (msgs_valid(msg, words)) {
		if (cls == UMP_TX_CONTROL && g_ctl.size - g_ctl.count >= words) {
			ring_put(&g_ctl, msg, words);
			rc = 0;
		} else if (cls == UMP_TX_NOTE &&
		           g_note.size - g_note.count >= words + g_reserve) {
			ring_put(&g_note, msg, words);
			rc = 0;
		}
	}
	irq_restore(s);
	return rc;
}

int ump_tx_push_attack(const uint32_t *msg, uint8_t words)
{
	int rc = -1;
	uint32_t s = irq_save();
	if (msgs_valid(msg, words) &&
	    g_note.size - g_note.count >= words + g_reserve + 2) {
		ring_put(&g_note, msg, words);
		g_reserve = (uint16_t)(g_reserve + 2);
		rc = 0;
	}
	irq_restore(s);
	return rc;
}

int ump_tx_push_release(const uint32_t *msg, uint8_t words)
{
	int rc = -1;
	uint32_t s = irq_save();
	if (msgs_valid(msg, words) && g_reserve >= 2 && words <= 2) {
		/* pressure key: group (bits 27..24), channel (19..16), note (15..8) */
		uint8_t group = (uint8_t)((msg[0] >> 24) & 0xF);
		uint8_t ch = (uint8_t)((msg[0] >> 16) & 0xF);
		uint8_t note = (uint8_t)((msg[0] >> 8) & 0xFF);
		for (int i = 0; i < UMP_TX_PRESSURE_SLOTS; i++) {
			struct pressure_slot *p = &g_pressure[i];
			if (p->used && p->group == group && p->ch == ch && p->note == note)
				p->used = 0;
		}
		g_reserve = (uint16_t)(g_reserve - 2); /* 2-word unit per attack, whatever the release size */
		ring_put(&g_note, msg, words); /* free >= reserve >= words, always fits */
		rc = 0;
	}
	irq_restore(s);
	return rc;
}

void ump_tx_push_pressure(uint8_t group, uint8_t ch, uint8_t note, const uint32_t msg[2])
{
	struct pressure_slot *target = 0;
	uint32_t s = irq_save();
	for (int i = 0; i < UMP_TX_PRESSURE_SLOTS; i++) {
		struct pressure_slot *p = &g_pressure[i];
		if (p->used) {
			if (p->group == group && p->ch == ch && p->note == note) {
				target = p;
				break;
			}
		} else if (!target) {
			target = p;
		}
	}
	if (target) {
		target->used = 1;
		target->group = group;
		target->ch = ch;
		target->note = note;
		target->w[0] = msg[0];
		target->w[1] = msg[1];
	}
	irq_restore(s);
}

static uint8_t put_le(uint8_t *out, uint8_t pos, const uint32_t *w, uint8_t words)
{
	for (uint8_t i = 0; i < words; i++) {
		out[pos++] = (uint8_t)w[i];
		out[pos++] = (uint8_t)(w[i] >> 8);
		out[pos++] = (uint8_t)(w[i] >> 16);
		out[pos++] = (uint8_t)(w[i] >> 24);
	}
	return pos;
}

uint8_t ump_tx_fill_packet(uint8_t out[64])
{
	uint8_t pos = 0;
	int full = 0;
	uint32_t tmp[4];
	uint32_t s = irq_save();
	struct ring *rings[2] = {&g_ctl, &g_note};

	for (int k = 0; k < 2 && !full; k++) {
		struct ring *r = rings[k];
		while (r->count) {
			uint8_t wc = ring_head_words(r);
			if (pos + 4 * wc > 64) {
				full = 1;
				break;
			}
			ring_take(r, tmp, wc);
			pos = put_le(out, pos, tmp, wc);
		}
	}
	for (int i = 0; i < UMP_TX_PRESSURE_SLOTS && !full; i++) {
		struct pressure_slot *p = &g_pressure[i];
		if (!p->used)
			continue;
		if (pos + 8 > 64) {
			full = 1;
			break;
		}
		pos = put_le(out, pos, p->w, 2);
		p->used = 0;
	}
	irq_restore(s);
	return pos;
}

uint16_t ump_tx_reserve_words(void)
{
	uint32_t s = irq_save();
	uint16_t r = g_reserve;
	irq_restore(s);
	return r;
}
