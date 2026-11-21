#include "test.h"
#include "stubs/stubs.h"
#include "usb/usb_compat.h"
#include "usb/usb_midi2.h"
#include "platform/lib_private.h"
#include "ump/ump.h"
#include "midi2.h"
#include <stdint.h>
#include <string.h>

void EP1_OUT_Callback(void);
void __wrap_midi_parsers_init(void);
void __wrap_midi_parsers_poll(void);
void __wrap_usbmidi_poll(void);
void __wrap_midi_flush_windows_buffer(void);
void __wrap_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2);
void __wrap_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len);

/* ---- helpers ---- */

/* configured, alt 0, main loop ran once */
static void enter_alt0(void)
{
	stub_irq_hook = 0;
	stubs_reset();
	usb_midi2_usb_reset();
	usb_midi2_service();
	stubs_reset();
	bDeviceState = USB_STATE_CONFIGURED;
	midi2_debug_pause_rx(0);
	ump_stream_set_protocol_hook(midi2_protocol_changing);
}

/* configured, alt 1 (MIDI 2.0 protocol), main loop ran once; EP1 RX armed */
static void enter_alt1(void)
{
	stub_irq_hook = 0;
	stubs_reset();
	usb_midi2_usb_reset();
	usb_midi2_set_alt(1);
	usb_midi2_service();
	stub.ep_rx_valid_calls = 0;
	stub.fifo_flush_calls = 0;
	stub.fifo1k_flush_calls = 0;
	bDeviceState = USB_STATE_CONFIGURED;
	midi2_debug_pause_rx(0);
	ump_stream_set_protocol_hook(midi2_protocol_changing);
}

/* host → device: one EP1 OUT packet of n words, little-endian */
static void host_out(const uint32_t *w, uint16_t n)
{
	uint8_t p[64];
	uint16_t i;
	for (i = 0; i < n && i < 16; i++) {
		p[4 * i] = (uint8_t)w[i];
		p[4 * i + 1] = (uint8_t)(w[i] >> 8);
		p[4 * i + 2] = (uint8_t)(w[i] >> 16);
		p[4 * i + 3] = (uint8_t)(w[i] >> 24);
	}
	stub_rx_packet(p, (uint16_t)(4 * i));
	EP1_OUT_Callback();
}

/* the host took the last IN packet; the main loop sends the next one.
 * Returns the words of the packet written (0 if none). */
static uint16_t host_in(uint32_t out[16])
{
	uint16_t i;
	int writes = stub.tx_writes;
	stub.ep_tx_status[1] = EP_TX_NAK;
	__wrap_usbmidi_poll();
	if (stub.tx_writes == writes)
		return 0;
	for (i = 0; i < stub.last_tx_len / 4; i++)
		out[i] = (uint32_t)stub.last_tx[4 * i] | ((uint32_t)stub.last_tx[4 * i + 1] << 8) |
		         ((uint32_t)stub.last_tx[4 * i + 2] << 16) |
		         ((uint32_t)stub.last_tx[4 * i + 3] << 24);
	return (uint16_t)(stub.last_tx_len / 4);
}

/* ---- EP1 OUT / RX ---- */

static void test_ep1_out_odd_length(void)
{
	uint8_t p1[11] = {0x00,0x3C,0x90,0x40, 0x00,0x00,0xFF,0xFF, 0xAA,0xBB,0xCC};
	uint8_t p2[8]  = {0x00,0x3E,0x90,0x40, 0x00,0x00,0x00,0x80};
	enter_alt1(); stub_rx_packet(p1, 11); EP1_OUT_Callback();
	CHECK_EQ(ump_rx_free_words(), UMP_RX_WORDS - 2);	/* trailing 3 bytes dropped */
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub_midi_n, 1);
	CHECK_EQ(stub_midi_log[0].port, 0);
	CHECK_EQ(stub_midi_log[0].status, 0x90);
	CHECK_EQ(stub_midi_log[0].d1, 60);
	CHECK_EQ(stub_midi_log[0].d2, 127);
	CHECK_EQ(MIDIToDev.tail - MIDIToDev.head, 0);		/* not the legacy path */
	stub_rx_packet(p2, 8); EP1_OUT_Callback(); __wrap_midi_parsers_poll();
	CHECK_EQ(stub_midi_n, 2); CHECK_EQ(stub_midi_log[1].d1, 62);
	CHECK_EQ(stub_midi_log[1].d2, 64);
	CHECK_EQ(ump_rx_overflows(), 0);
}

static void test_ep1_out_leaves_nak_until_poll(void)
{
	static const uint32_t w[1] = {0x20903C40};
	enter_alt1();
	host_out(w, 1);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);
	CHECK_EQ(stub.ep_rx_valid_calls, 0);
	CHECK_EQ(stub_midi_n, 0);				/* dispatched in the main loop */
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub_midi_n, 1);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_VALID);
	CHECK_EQ(stub.ep_rx_valid_calls, 1);
	__wrap_midi_parsers_poll();				/* already VALID: no re-arm */
	CHECK_EQ(stub.ep_rx_valid_calls, 1);
}

static void test_backpressure_nak_until_space(void)
{
	static const uint32_t zeros[16];			/* MT 0 NOOPs */
	int packets = 0;
	enter_alt1(); midi2_debug_pause_rx(1);
	/* feed 64-byte packets until ump_rx_free_words() < 16 */
	while (ump_rx_free_words() >= 16) {
		host_out(zeros, 16);
		packets++;
	}
	CHECK_EQ(packets, UMP_RX_WORDS / 16);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);
	__wrap_midi_parsers_poll(); CHECK_EQ(stub.ep_rx_valid_calls, 0);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);
	CHECK_EQ(stub.din_parse_calls, 1);			/* DIN still parsed */
	midi2_debug_pause_rx(0); __wrap_midi_parsers_poll(); CHECK_EQ(stub.ep_rx_valid_calls, 1);
	CHECK_EQ(ump_rx_free_words(), UMP_RX_WORDS);
	CHECK_EQ(ump_rx_overflows(), 0);
}

static void test_rearm_threshold(void)
{
	static const uint32_t zeros[16];
	int i;
	enter_alt1(); midi2_debug_pause_rx(1);
	for (i = 0; i < UMP_RX_WORDS / 16 - 1; i++)
		host_out(zeros, 16);
	CHECK_EQ(ump_rx_free_words(), 16);			/* exactly one packet's room */
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub.ep_rx_valid_calls, 1);
	host_out(zeros, 1);
	CHECK_EQ(ump_rx_free_words(), 15);			/* one word short */
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub.ep_rx_valid_calls, 1);
}

static void test_din_parsed_in_both_alts(void)
{
	stubs_reset(); usb_midi2_usb_reset(); __wrap_midi_parsers_poll();
	CHECK_EQ(stub.real_parsers_poll_calls, 1);
	CHECK_EQ(stub.din_parse_calls, 0);			/* real does it in alt 0 */
	enter_alt1(); __wrap_midi_parsers_poll();
	CHECK_EQ(stub.real_parsers_poll_calls, 0);
	CHECK_EQ(stub.din_parse_calls, 1); CHECK_EQ(stub.din_parse_last_port, 2);
	CHECK(stub.din_parse_last_parser == DinRx);
}

static void test_legacy_paths_gated_in_alt1(void)
{
	enter_alt1(); __wrap_usbmidi_poll(); __wrap_midi_flush_windows_buffer();
	CHECK_EQ(stub.real_usbmidi_poll_calls, 0); CHECK_EQ(stub.real_flush_calls, 0);
	enter_alt0(); __wrap_usbmidi_poll(); __wrap_midi_flush_windows_buffer();
	CHECK_EQ(stub.real_usbmidi_poll_calls, 1); CHECK_EQ(stub.real_flush_calls, 1);
}

static void test_alt1_entry_runs_service_first(void)
{
	static const uint32_t w[1] = {0x20903C40};
	enter_alt0();
	usb_midi2_set_alt(1);					/* ISR */
	MIDIToDev.head = 1;
	MIDIToDev.tail = 5;
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub.fifo_flush_calls, 1);
	CHECK_EQ(stub.fifo1k_flush_calls, 1);
	CHECK_EQ(stub.ep_rx_valid_calls, 1);
	CHECK_EQ(stub.real_parsers_poll_calls, 0);
	host_out(w, 1);
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub_midi_n, 1);
}

/* ---- EP1 IN / TX ---- */

static void test_usbmidi_poll_tx_conditions(void)
{
	uint32_t w[16];
	enter_alt1();
	__wrap_hal_send_midi(USBMIDI, 0xB0, 1, 2);
	bDeviceState = USB_STATE_ADDRESSED;			/* not configured: hold */
	CHECK_EQ(host_in(w), 0);
	bDeviceState = USB_STATE_CONFIGURED;
	stub.ep_tx_status[1] = EP_TX_VALID;			/* previous packet in flight */
	__wrap_usbmidi_poll();
	CHECK_EQ(stub.tx_writes, 0);
	CHECK_EQ(host_in(w), 1);
	CHECK_EQ(w[0], 0x21B00102u);
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_VALID);
	CHECK_EQ(host_in(w), 0);				/* nothing queued: stays NAK */
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_NAK);
	CHECK_EQ(stub.real_usbmidi_poll_calls, 0);
}

static void test_note_on2_paths(void)
{
	stubs_reset(); usb_midi2_usb_reset();                 /* alt 0 → 7-bit via real */
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x0100);
	CHECK_EQ(stub.real_midi_calls, 1);
	CHECK_EQ(stub.real_midi_last[0], USBSTANDALONE);
	CHECK_EQ(stub.real_midi_last[1], 0x90);
	CHECK_EQ(stub.real_midi_last[2], 60);
	CHECK_EQ(stub.real_midi_last[3], 1);                  /* clamped ≥ 1 */
	enter_alt1(); hal_send_note_on2(USBSTANDALONE, 0, 60, 0xABCD); __wrap_usbmidi_poll();
	CHECK_EQ(stub.last_tx_len, 8); CHECK_EQ(stub.last_tx[2], 0x90); CHECK_EQ(stub.last_tx[6], 0xCD);
	CHECK_EQ(stub.last_tx[7], 0xAB);
	CHECK_EQ(stub.last_tx[3], 0x40);
	stub.ep_tx_status[1] = EP_TX_NAK;			/* host took it */
	hal_send_note_on2(USBMIDI, 0, 60, 0xFFFF); __wrap_usbmidi_poll();
	CHECK_EQ(stub.last_tx_len, 4); CHECK_EQ(stub.last_tx[0], 0x7F);   /* MT 2, group 1 */
	CHECK_EQ(stub.last_tx[3], 0x21);
	/* DIN in alt 1: 7-bit through the real path */
	hal_send_note_on2(DINMIDI, 3, 61, 0x0000);
	CHECK_EQ(stub.real_midi_calls, 1);
	CHECK_EQ(stub.real_midi_last[0], DINMIDI);
	CHECK_EQ(stub.real_midi_last[1], 0x93);
	CHECK_EQ(stub.real_midi_last[2], 61);
	CHECK_EQ(stub.real_midi_last[3], 1);
	CHECK_EQ(ump_tx_reserve_words(), 4);			/* two accepted attacks */
}

static void test_note_off2_and_reserve(void)
{
	uint32_t w[16];
	enter_alt0();
	hal_send_note_off2(USBSTANDALONE, 2, 64, 0xFFFF);
	CHECK_EQ(stub.real_midi_last[1], 0x82);
	CHECK_EQ(stub.real_midi_last[3], 127);

	enter_alt1();
	hal_send_note_on2(USBSTANDALONE, 2, 64, 0x1234);
	hal_send_note_on2(USBSTANDALONE, 2, 64, 0x2345);	/* retrigger: no extra reserve */
	CHECK_EQ(ump_tx_reserve_words(), 2);
	hal_send_note_off2(USBSTANDALONE, 2, 64, 0x5678);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	hal_send_note_off2(USBSTANDALONE, 2, 64, 0);		/* unmatched: still sent */
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(host_in(w), 8);
	CHECK_EQ(w[0], 0x40924000u);
	CHECK_EQ(w[1], 0x12340000u);
	CHECK_EQ(w[2], 0x40924000u);
	CHECK_EQ(w[3], 0x23450000u);
	CHECK_EQ(w[4], 0x40824000u);
	CHECK_EQ(w[5], 0x56780000u);
	CHECK_EQ(w[6], 0x40824000u);
	CHECK_EQ(w[7], 0);
}

static void test_saturated_note_offs_always_queue(void)
{
	static uint32_t got[UMP_TX_NOTE_WORDS + 16];
	uint32_t w[16];
	uint16_t n = 0, k;
	int accepted, offs = 0;
	uint8_t note;

	enter_alt1();						/* host not reading */
	for (note = 0; note < 128; note++)
		hal_send_note_on2(USBSTANDALONE, 0, note, 0x8000);
	accepted = ump_tx_reserve_words() / 2;
	CHECK(accepted > 0);
	CHECK(accepted < 128);
	for (note = 0; note < 128; note++)
		hal_send_note_off2(USBSTANDALONE, 0, note, 0);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	while ((k = host_in(w)) > 0) {
		memcpy(got + n, w, 4u * k);
		n = (uint16_t)(n + k);
	}
	for (k = 0; k < n; k += 2)
		if ((got[k] & 0xFFF00000u) == 0x40800000u && ((got[k] >> 8) & 0x7F) < accepted)
			offs++;
	CHECK_EQ(offs, accepted);				/* every accepted note is released */
}

static void test_hal_send_midi_alt1(void)
{
	uint32_t w[16];
	enter_alt1();
	__wrap_hal_send_midi(USBSTANDALONE, 0x90, 60, 100);	/* MT 4 attack */
	__wrap_hal_send_midi(USBSTANDALONE, 0xB0, 7, 64);	/* MT 4 CC, note ring */
	__wrap_hal_send_midi(USBSTANDALONE, 0xF8, 0, 0);	/* system: control ring */
	CHECK_EQ(ump_tx_reserve_words(), 2);
	CHECK_EQ(host_in(w), 5);
	CHECK_EQ(w[0], 0x10F80000u);
	CHECK_EQ(w[1], 0x40903C00u);
	CHECK_EQ(w[2], ump_scale_up(100, 7, 16) << 16);
	CHECK_EQ(w[3], 0x40B00700u);
	CHECK_EQ(w[4], 0x80000000u);

	__wrap_hal_send_midi(USBMIDI, 0x91, 60, 100);		/* group 1 is MIDI 1.0 */
	__wrap_hal_send_midi(USBMIDI, 0x91, 60, 0);		/* note on vel 0 = release */
	CHECK_EQ(ump_tx_reserve_words(), 2);
	__wrap_hal_send_midi(USBSTANDALONE, 0x90, 60, 0);	/* MT 4 Note Off */
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(host_in(w), 4);
	CHECK_EQ(w[0], 0x21913C64u);
	CHECK_EQ(w[1], 0x21913C00u);
	CHECK_EQ(w[2], 0x40803C00u);
	CHECK_EQ(w[3], 0x80000000u);

	__wrap_hal_send_midi(USBSTANDALONE, 0xF0, 0, 0);	/* SysEx byte: dropped */
	__wrap_hal_send_midi(5, 0x90, 1, 1);			/* no such port */
	CHECK_EQ(host_in(w), 0);
	__wrap_hal_send_midi(DINMIDI, 0x90, 1, 2);
	CHECK_EQ(stub.real_midi_calls, 2);			/* port 5 and DIN via real */
	CHECK_EQ(stub.real_midi_last[0], DINMIDI);

	/* MIDI 1.0 protocol: Standalone goes as MT 2 too */
	{
		static const uint32_t req[4] = {0xF0050100u, 0, 0, 0};
		host_out(req, 4);
		__wrap_midi_parsers_poll();
		CHECK_EQ(host_in(w), 4);			/* notification */
		__wrap_hal_send_midi(USBSTANDALONE, 0x90, 60, 100);
		CHECK_EQ(host_in(w), 1);
		CHECK_EQ(w[0], 0x20903C64u);
		CHECK_EQ(ump_tx_reserve_words(), 2);
	}
}

static void test_pressure2(void)
{
	uint32_t w[16];
	enter_alt0();
	hal_send_poly_pressure2(USBMIDI, 1, 60, 0xFFFFFFFFu);
	CHECK_EQ(stub.real_midi_last[1], 0xA1);
	CHECK_EQ(stub.real_midi_last[3], 127);
	hal_send_channel_pressure2(DINMIDI, 1, 0x80000000u);
	CHECK_EQ(stub.real_midi_last[0], DINMIDI);
	CHECK_EQ(stub.real_midi_last[1], 0xD1);
	CHECK_EQ(stub.real_midi_last[2], 64);

	enter_alt1();
	hal_send_poly_pressure2(USBSTANDALONE, 1, 60, 0x11111111u);
	hal_send_poly_pressure2(USBSTANDALONE, 1, 60, 0x22222222u);	/* coalesced */
	hal_send_channel_pressure2(USBSTANDALONE, 1, 0x33333333u);
	hal_send_poly_pressure2(USBMIDI, 1, 60, 0x80000000u);		/* MT 2 */
	CHECK_EQ(host_in(w), 5);
	CHECK_EQ(w[0], 0x21A13C40u);					/* note ring first */
	CHECK_EQ(w[1], 0x40A13C00u);
	CHECK_EQ(w[2], 0x22222222u);
	CHECK_EQ(w[3], 0x40D10000u);
	CHECK_EQ(w[4], 0x33333333u);
}

static void test_protocol_change_releases_notes(void)
{
	static const uint32_t to_midi1[4] = {0xF0050100u, 0, 0, 0};
	static const uint32_t to_midi2[4] = {0xF0050200u, 0, 0, 0};
	uint32_t w[16];

	/* alt 1 MIDI 2: note 60 sent; note 62 on + off still queued; Stream
	   Config request to MIDI 1 via RX → on the wire, in order: the queued
	   Note On 62, its Note Off, the hook's MIDI 2.0 Note Off 0x40803C00 for
	   the still-sounding 60, then 0xF0060100; nothing dropped */
	enter_alt1();
	CHECK_EQ(midi2_active(), 1);
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
	CHECK_EQ(host_in(w), 2);
	hal_send_note_on2(USBSTANDALONE, 0, 62, 0x9000);
	hal_send_note_off2(USBSTANDALONE, 0, 62, 0);
	host_out(to_midi1, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(ump_stream_protocol(), UMP_PROTO_MIDI1);
	CHECK_EQ(midi2_active(), 0);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(host_in(w), 10);
	CHECK_EQ(w[0], 0x40903E00u);
	CHECK_EQ(w[1], 0x90000000u);
	CHECK_EQ(w[2], 0x40803E00u);
	CHECK_EQ(w[3], 0);
	CHECK_EQ(w[4], 0x40803C00u);
	CHECK_EQ(w[5], 0);
	CHECK_EQ(w[6], 0xF0060100u);
	CHECK_EQ(w[7], 0);
	CHECK_EQ(host_in(w), 0);
	/* the sounding bit was cleared: a later note off is sent once, as MT 2 */
	hal_send_note_off2(USBSTANDALONE, 0, 60, 0);
	CHECK_EQ(host_in(w), 1);
	CHECK_EQ(w[0], 0x20803C00u);

	/* back to MIDI 2: a MIDI 1.0 note is released as MIDI 1.0 */
	hal_send_note_on2(USBSTANDALONE, 5, 70, 0xFFFF);
	CHECK_EQ(host_in(w), 1);
	CHECK_EQ(w[0], 0x2095467Fu);
	host_out(to_midi2, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(midi2_active(), 1);
	CHECK_EQ(host_in(w), 5);
	CHECK_EQ(w[0], 0x20854600u);
	CHECK_EQ(w[1], 0xF0060200u);

	/* a request for the current protocol releases nothing */
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
	CHECK_EQ(host_in(w), 2);
	host_out(to_midi2, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(host_in(w), 4);
	CHECK_EQ(w[0], 0xF0060200u);
	CHECK_EQ(ump_tx_reserve_words(), 2);
}

static void test_protocol_change_keeps_group1(void)
{
	static const uint32_t req[4] = {0xF0050100u, 0, 0, 0};
	uint32_t w[16];
	enter_alt1();
	hal_send_note_on2(USBMIDI, 0, 61, 0x8000);
	hal_send_note_on2(USBMIDI, 0, 63, 0x8000);
	CHECK_EQ(host_in(w), 2);
	hal_send_note_off2(USBMIDI, 0, 61, 0);			/* queued at the change */
	hal_send_poly_pressure2(USBSTANDALONE, 0, 60, 5);	/* stale group 0 pressure */
	CHECK_EQ(ump_tx_reserve_words(), 2);
	host_out(req, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(host_in(w), 5);
	CHECK_EQ(w[0], 0x21803D00u);				/* group 1 Note Off survives */
	CHECK_EQ(w[1], 0xF0060100u);				/* no Note Off for group 1 note 63 */
	CHECK_EQ(host_in(w), 0);				/* group 0 pressure dropped */
	CHECK_EQ(ump_tx_reserve_words(), 2);			/* note 63 still holds its reserve */
	hal_send_note_off2(USBMIDI, 0, 63, 0);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(host_in(w), 1);
	CHECK_EQ(w[0], 0x21803F00u);
}

static void test_protocol_change_notify_survives_full_control_ring(void)
{
	static const uint32_t req[4] = {0xF0050100u, 0, 0, 0};
	static const uint32_t ctl[4] = {0xF0010101u, 0x82000300u, 0, 0};
	static uint32_t got[256];
	uint32_t w[16];
	uint16_t n = 0, k;
	int pushed = 0;
	enter_alt1();
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
	CHECK_EQ(host_in(w), 2);
	while (ump_tx_push(UMP_TX_CONTROL, ctl, 4) == 0)
		pushed++;
	CHECK_EQ(pushed, UMP_TX_CONTROL_WORDS / 4);
	host_out(req, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(ump_stream_protocol(), UMP_PROTO_MIDI1);
	while ((k = host_in(w)) > 0 && n + k <= 256) {
		memcpy(got + n, w, 4u * k);
		n = (uint16_t)(n + k);
	}
	CHECK_EQ(n, UMP_TX_CONTROL_WORDS + 6);
	CHECK_EQ(got[n - 6], 0x40803C00u);
	CHECK_EQ(got[n - 4], 0xF0060100u);
}

static void test_protocol_change_keeps_queued_replies(void)
{
	static const uint32_t req[8] = {0xF0000101u, 0x00000001u, 0, 0,	/* EP discovery: info */
	                                0xF0050100u, 0, 0, 0};		/* then config → MIDI 1 */
	uint32_t w[16];
	enter_alt1();
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
	host_out(req, 8);
	__wrap_midi_parsers_poll();
	CHECK_EQ(host_in(w), 12);
	CHECK_EQ(w[0], 0xF0010101u);				/* Endpoint Info kept */
	CHECK_EQ(w[4], 0x40903C00u);				/* queued Note On kept */
	CHECK_EQ(w[6], 0x40803C00u);
	CHECK_EQ(w[8], 0xF0060100u);
}

static void test_parsers_init_registers_hook(void)
{
	static const uint32_t req[4] = {0xF0050100u, 0, 0, 0};
	uint32_t w[16];
	enter_alt1();
	ump_stream_set_protocol_hook(0);
	__wrap_midi_parsers_init();
	hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
	CHECK_EQ(host_in(w), 2);
	host_out(req, 4);
	__wrap_midi_parsers_poll();
	CHECK_EQ(host_in(w), 6);
	CHECK_EQ(w[0], 0x40803C00u);
}

static void test_hal_send_sysex_alt1(void)
{
	/* 20-byte F0..F7 → 4 SysEx7 packets on group 0 sent in order through EP1 IN */
	uint8_t sx[22];
	uint32_t ref[8], w[16];
	int i;
	sx[0] = 0xF0;
	for (i = 1; i <= 20; i++)
		sx[i] = (uint8_t)i;
	sx[21] = 0xF7;
	CHECK_EQ(ump_sysex7_pack(ref, 8, 0, sx, 22), 8);

	enter_alt1();
	__wrap_hal_send_midi(USBSTANDALONE, 0x90, 60, 100);	/* note ring: after SysEx */
	__wrap_hal_send_sysex(USBSTANDALONE, sx, 22);
	CHECK_EQ(host_in(w), 10);
	CHECK_MEM(w, ref, sizeof ref);
	CHECK_EQ(w[0] >> 20, 0x301);
	CHECK_EQ(w[2] >> 20, 0x302);
	CHECK_EQ(w[6] >> 20, 0x303);
	CHECK_EQ(w[8], 0x40903C00u);
	CHECK_EQ(stub.real_sysex_calls, 0);

	__wrap_hal_send_sysex(USBMIDI, sx, 22);			/* group 1 */
	CHECK_EQ(host_in(w), 8);
	CHECK_EQ(w[0] >> 24, 0x31);
	__wrap_hal_send_sysex(USBMIDI, sx, 21);			/* no F7: dropped */
	CHECK_EQ(host_in(w), 0);
	__wrap_hal_send_sysex(DINMIDI, sx, 22);
	CHECK_EQ(stub.real_sysex_calls, 1);
	CHECK(stub.real_sysex_data == sx);

	enter_alt0();
	__wrap_hal_send_sysex(USBSTANDALONE, sx, 22);
	CHECK_EQ(stub.real_sysex_calls, 1);
	CHECK_EQ(stub.real_sysex_port, USBSTANDALONE);
}

static void test_hal_send_does_not_run_service(void)
{
	static const uint8_t sx[3] = {0xF0, 0x01, 0xF7};
	enter_alt1();
	usb_midi2_set_alt(1);					/* ISR: service pending */
	__wrap_hal_send_midi(USBSTANDALONE, 0x90, 60, 100);
	__wrap_hal_send_sysex(USBSTANDALONE, sx, 3);
	hal_send_note_on2(USBSTANDALONE, 0, 61, 0x8000);
	CHECK_EQ(stub.fifo_flush_calls, 0);
	CHECK_EQ(stub.ep_rx_valid_calls, 0);
	__wrap_usbmidi_poll();
	CHECK_EQ(stub.fifo_flush_calls, 1);
}

static void test_alt_and_active(void)
{
	enter_alt0();
	CHECK_EQ(midi2_usb_alt(), 0);
	CHECK_EQ(midi2_active(), 0);
	enter_alt1();
	CHECK_EQ(midi2_usb_alt(), 1);
	CHECK_EQ(midi2_active(), 1);
	usb_midi2_usb_reset();
	CHECK_EQ(midi2_usb_alt(), 0);
	CHECK_EQ(midi2_active(), 0);
}

/* An alt switch (USB interrupt) at any irq_save of a note on or off must leave
 * the release reserve matching the sounding table: no leak, no lost release. */
static int irq_count, irq_fire_at;

static void irq_set_alt1(void)
{
	if (++irq_count == irq_fire_at) {
		stub_irq_hook = 0;
		usb_midi2_set_alt(1);
	}
}

static void test_sounding_vs_set_alt_interrupt(void)
{
	int at;
	for (at = 1; at <= 6; at++) {
		uint16_t r;
		enter_alt1();
		hal_send_note_on2(USBSTANDALONE, 0, 60, 0x8000);
		irq_count = 0;
		irq_fire_at = at;
		stub_irq_hook = irq_set_alt1;
		hal_send_note_on2(USBSTANDALONE, 0, 62, 0x8000);
		hal_send_note_off2(USBSTANDALONE, 0, 60, 0);
		stub_irq_hook = 0;
		r = ump_tx_reserve_words();
		CHECK(r == 0 || r == 2);
		hal_send_note_off2(USBSTANDALONE, 0, 62, 0);
		hal_send_note_off2(USBSTANDALONE, 0, 60, 0);
		CHECK_EQ(ump_tx_reserve_words(), 0);
	}
}

void test_usb_data_all(void)
{
	RUN(test_ep1_out_odd_length);
	RUN(test_ep1_out_leaves_nak_until_poll);
	RUN(test_backpressure_nak_until_space);
	RUN(test_rearm_threshold);
	RUN(test_din_parsed_in_both_alts);
	RUN(test_legacy_paths_gated_in_alt1);
	RUN(test_alt1_entry_runs_service_first);
	RUN(test_usbmidi_poll_tx_conditions);
	RUN(test_note_on2_paths);
	RUN(test_note_off2_and_reserve);
	RUN(test_saturated_note_offs_always_queue);
	RUN(test_hal_send_midi_alt1);
	RUN(test_pressure2);
	RUN(test_protocol_change_releases_notes);
	RUN(test_protocol_change_keeps_queued_replies);
	RUN(test_protocol_change_keeps_group1);
	RUN(test_protocol_change_notify_survives_full_control_ring);
	RUN(test_parsers_init_registers_hook);
	RUN(test_hal_send_sysex_alt1);
	RUN(test_hal_send_does_not_run_service);
	RUN(test_alt_and_active);
	RUN(test_sounding_vs_set_alt_interrupt);
	enter_alt0();
	ump_stream_set_protocol_hook(0);
}
