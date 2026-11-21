#include "ump.h"
#include "platform/platform.h"

#define ST_EP_DISCOVERY 0x000
#define ST_EP_INFO 0x001
#define ST_DEV_IDENTITY 0x002
#define ST_EP_NAME 0x003
#define ST_PRODUCT_ID 0x004
#define ST_CONFIG_REQUEST 0x005
#define ST_CONFIG_NOTIFY 0x006
#define ST_FB_DISCOVERY 0x010
#define ST_FB_INFO 0x011
#define ST_FB_NAME 0x012

#define FORM_COMPLETE 0
#define FORM_START 1
#define FORM_CONTINUE 2
#define FORM_END 3

#define FB_COUNT 2

static const char EP_NAME[] = "Launchpad Pro MIDI 2.0";
static const char *const FB_NAMES[FB_COUNT] = {"Standalone Port", "MIDI Port"};
/* word0 low 16 bits: active | FB number, then UI hint / MIDI 1.0 / direction. */
static const uint16_t FB_WORD0[FB_COUNT] = {0x8033, 0x813B};
static const uint32_t FB_WORD1[FB_COUNT] = {0x00010000u, 0x01010000u};

static uint8_t g_protocol = UMP_PROTO_MIDI2;
static void (*g_hook)(uint8_t old_proto);

void ump_stream_reset(void)
{
	g_protocol = UMP_PROTO_MIDI2;
}

uint8_t ump_stream_protocol(void)
{
	return g_protocol;
}

void ump_stream_set_protocol_hook(void (*fn)(uint8_t old_proto))
{
	g_hook = fn;
}

static uint32_t head(uint32_t form, uint32_t status, uint32_t low16)
{
	return 0xF0000000u | (form << 26) | (status << 16) | low16;
}

static void push4(const uint32_t m[4])
{
	ump_tx_push(UMP_TX_CONTROL, m, 4);
}

/* Queue a text as one all-or-nothing push. lead = bytes in word 0 (2 or 1);
 * fb_num < 0 for endpoint-level text. */
static void push_text(uint32_t status, int fb_num, const char *s)
{
	uint32_t out[16];
	uint8_t lead = fb_num < 0 ? 2 : 1;
	uint8_t per = (uint8_t)(lead + 12);
	uint16_t len = 0, pos = 0;
	uint8_t words = 0;
	while (s[len])
		len++;
	do {
		uint16_t rem = (uint16_t)(len - pos);
		uint32_t form = pos == 0 ? (rem <= per ? FORM_COMPLETE : FORM_START)
					 : (rem <= per ? FORM_END : FORM_CONTINUE);
		uint8_t b[14];
		for (uint8_t i = 0; i < per; i++)
			b[i] = (uint16_t)(pos + i) < len ? (uint8_t)s[pos + i] : 0;
		if (words + 4 > 16)
			return;
		if (fb_num < 0)
			out[words] = head(form, status, ((uint32_t)b[0] << 8) | b[1]);
		else
			out[words] = head(form, status, ((uint32_t)fb_num << 8) | b[0]);
		for (uint8_t w = 0; w < 3; w++)
			out[words + 1 + w] = ((uint32_t)b[lead + 4 * w] << 24) |
					     ((uint32_t)b[lead + 4 * w + 1] << 16) |
					     ((uint32_t)b[lead + 4 * w + 2] << 8) | b[lead + 4 * w + 3];
		words = (uint8_t)(words + 4);
		pos = (uint16_t)(pos + per);
	} while (pos < len);
	ump_tx_push(UMP_TX_CONTROL, out, words);
}

/* behind_notes: after a protocol change the notification goes in the note
 * FIFO, behind the note traffic already queued in the old protocol and the
 * hook's Note Offs (control ring if the note FIFO is full). */
static void send_config_notify(int behind_notes)
{
	uint32_t m[4] = {head(0, ST_CONFIG_NOTIFY, (uint32_t)g_protocol << 8), 0, 0, 0};
	if (behind_notes && ump_tx_push(UMP_TX_NOTE, m, 4) == 0)
		return;
	push4(m);
}

static void endpoint_discovery(const uint32_t msg[4])
{
	uint8_t filter = (uint8_t)msg[1];
	if (filter & 0x01) {
		/* UMP 1.1; static FBs, 2 FBs, MIDI 2.0 and MIDI 1.0 supported, no JR */
		uint32_t m[4] = {head(0, ST_EP_INFO, 0x0101), 0x82000300u, 0, 0};
		push4(m);
	}
	if (filter & 0x02) {
		/* manufacturer 00 20 29, family 0x0051, model 0x0000, version 00 01 00 00 */
		uint32_t m[4] = {head(0, ST_DEV_IDENTITY, 0), 0x00002029u, 0x51000000u, 0x00010000u};
		push4(m);
	}
	if (filter & 0x04)
		push_text(ST_EP_NAME, -1, EP_NAME);
	if (filter & 0x08) {
		char id[25];
		platform_product_instance_id(id);
		push_text(ST_PRODUCT_ID, -1, id);
	}
	if (filter & 0x10)
		send_config_notify(0);
}

static void fb_discovery(const uint32_t msg[4])
{
	uint8_t fb = (uint8_t)(msg[0] >> 8);
	uint8_t filter = (uint8_t)msg[0];
	for (uint8_t i = 0; i < FB_COUNT; i++) {
		if (fb != 0xFF && fb != i)
			continue;
		if (filter & 0x01) {
			uint32_t m[4] = {head(0, ST_FB_INFO, FB_WORD0[i]), FB_WORD1[i], 0, 0};
			push4(m);
		}
		if (filter & 0x02)
			push_text(ST_FB_NAME, i, FB_NAMES[i]);
	}
}

int ump_stream_handle(const uint32_t msg[4])
{
	int changed = 0;
	if ((msg[0] >> 28) != 0xF)
		return 0;
	switch ((msg[0] >> 16) & 0x3FF) {
	case ST_EP_DISCOVERY:
		endpoint_discovery(msg);
		break;
	case ST_FB_DISCOVERY:
		fb_discovery(msg);
		break;
	case ST_CONFIG_REQUEST: {
		uint8_t want = (uint8_t)(msg[0] >> 8);
		if ((want == UMP_PROTO_MIDI1 || want == UMP_PROTO_MIDI2) && want != g_protocol) {
			if (g_hook)
				g_hook(g_protocol);
			g_protocol = want;
			changed = 1;
		}
		send_config_notify(changed);
		break;
	}
	default:
		break;
	}
	return changed;
}
