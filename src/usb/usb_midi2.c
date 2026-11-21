/*
 * USB MIDI 2.0 EP1 ownership: which side (c_midi's USB MIDI 1.0 path in alt 0,
 * the UMP path in alt 1) owns EP1, the switch between them, and the alt 1
 * data path behind the linker wraps of main.o and the app's hal_send_*.
 *
 * usb_midi2_set_alt runs in the USB interrupt and touches only what is safe
 * there; the cleanup of c_midi's main-loop-owned fifos and parser is deferred
 * to usb_midi2_service, which the main-loop wraps run before any legacy code.
 * hal_send_* may run in the app's timer interrupt: they never run the service
 * and keep no large stack buffers.
 */
#include "usb/usb_midi2.h"
#include "usb/usb_compat.h"
#include "platform/lib_private.h"
#include "platform/irq.h"
#include "ump/ump.h"
#include "midi2.h"
#include <string.h>

static volatile uint8_t alt;
static volatile uint8_t pending_service;
static volatile uint8_t rx_paused;

/* USBParserToDev right after midi_parsers_init (its initial state) */
static uint8_t parser_snapshot[sizeof USBParserToDev];
static uint8_t parser_snapshot_valid;

/* Accepted attacks on groups 0 (Standalone) and 1 (MIDI), one bit per
 * (group, channel, note): sounding[group][ch][note >> 3] bit (note & 7). A set
 * bit holds one release reserve in the TX queue (ump_tx_push_attack), so the
 * reserve always equals 2 words per set bit. Every read-modify-write runs
 * under irq_save: set_alt (USB interrupt) clears the table and the reserve. */
static uint8_t sounding[2][16][16];

uint8_t usb_midi2_alt(void)
{
	return alt;
}

uint8_t usb_midi2_iface_alt(uint8_t iface)
{
	return iface == 1 ? alt : 0;
}

void usb_midi2_set_alt(uint8_t new_alt)
{
	SetEPTxStatus(1, EP_TX_NAK);
	SetEPRxStatus(1, EP_RX_NAK);
	ClearDTOG_RX(1);
	ClearDTOG_TX(1);
	alt = new_alt ? 1 : 0;
	ump_rx_reset();
	ump_tx_reset();
	ump_stream_reset();
	memset(sounding, 0, sizeof sounding);
	pending_service = 1;
}

void usb_midi2_usb_reset(void)
{
	usb_midi2_set_alt(0);
}

void usb_midi2_service(void)
{
	uint32_t s;

	if (!pending_service)
		return;
	/* atomic with respect to the USB interrupt: EP1_OUT_Callback pushes into
	 * MIDIToDev and set_alt may run again */
	s = irq_save();
	if (pending_service) {
		pending_service = 0;
		fifo_flush(&MIDIToDev);
		fifo1k_flush(&MIDIToHst);
		if (parser_snapshot_valid)
			memcpy(USBParserToDev, parser_snapshot, sizeof parser_snapshot);
		SetEPRxValid(1);
	}
	irq_restore(s);
}

/* ---- sounding table and note traffic ---- */

static void send_attack(uint8_t group, uint8_t ch, uint8_t note, const uint32_t *msg,
                        uint8_t words)
{
	uint8_t *byte = &sounding[group & 1][ch & 0xF][(note & 0x7F) >> 3];
	uint8_t bit = (uint8_t)(1u << (note & 7));
	uint32_t s = irq_save();

	if (*byte & bit)
		ump_tx_push(UMP_TX_NOTE, msg, words);	/* retrigger: reserve already held */
	else if (ump_tx_push_attack(msg, words) == 0)
		*byte |= bit;
	irq_restore(s);
}

static void send_release(uint8_t group, uint8_t ch, uint8_t note, const uint32_t *msg,
                         uint8_t words)
{
	uint8_t *byte = &sounding[group & 1][ch & 0xF][(note & 0x7F) >> 3];
	uint8_t bit = (uint8_t)(1u << (note & 7));
	uint32_t s = irq_save();

	if (*byte & bit) {
		*byte = (uint8_t)(*byte & ~bit);
		if (ump_tx_push_release(msg, words) == 0) {
			irq_restore(s);
			return;
		}
	}
	ump_tx_push(UMP_TX_NOTE, msg, words);		/* no accepted attack: best effort */
	irq_restore(s);
}

void midi2_protocol_changing(uint8_t old_proto)
{
	uint32_t s = irq_save();
	uint8_t ch, i, b;

	/* queued note traffic is in the old protocol and the reserve belongs to
	 * the table being cleared; Stream replies already queued are kept */
	ump_tx_reset_notes();
	for (ch = 0; ch < 16; ch++) {
		for (i = 0; i < 16; i++) {
			uint8_t bits = sounding[0][ch][i];
			for (b = 0; bits; b++, bits >>= 1) {
				uint32_t m[2];
				uint8_t note = (uint8_t)(i * 8 + b);
				uint8_t words;
				if (!(bits & 1))
					continue;
				if (old_proto == UMP_PROTO_MIDI2)
					words = ump_m2_note_off(m, 0, ch, note, 0);
				else
					words = ump_from_midi1(m, 0, (uint8_t)(0x80 | ch), note, 0, 0);
				/* control ring: ahead of the Configuration Notification
				 * that ump_stream queues after this hook returns */
				if (ump_tx_push(UMP_TX_CONTROL, m, words) != 0)
					ump_tx_push(UMP_TX_NOTE, m, words);
			}
		}
	}
	memset(sounding, 0, sizeof sounding);
	irq_restore(s);
}

/* ---- midi2.h ---- */

u8 midi2_usb_alt(void)
{
	return usb_midi2_alt();
}

u8 midi2_active(void)
{
	return alt && ump_stream_protocol() == UMP_PROTO_MIDI2;
}

void midi2_debug_pause_rx(u8 paused)
{
	rx_paused = paused ? 1 : 0;
}

/* the Standalone group in MIDI 2.0 Protocol on the alt 1 interface */
static uint8_t standalone_midi2(uint8_t port)
{
	return port == USBSTANDALONE && alt && ump_stream_protocol() == UMP_PROTO_MIDI2;
}

void __wrap_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2);

void hal_send_note_on2(u8 port, u8 ch, u8 note, u16 velocity)
{
	if (standalone_midi2(port)) {
		uint32_t m[2];
		ump_m2_note_on(m, 0, ch, note, velocity);
		send_attack(0, ch, note, m, 2);
	} else {
		uint8_t v = (uint8_t)ump_scale_down(velocity, 16, 7);
		__wrap_hal_send_midi(port, (uint8_t)(0x90 | (ch & 0xF)), note & 0x7F, v ? v : 1);
	}
}

void hal_send_note_off2(u8 port, u8 ch, u8 note, u16 velocity)
{
	if (standalone_midi2(port)) {
		uint32_t m[2];
		ump_m2_note_off(m, 0, ch, note, velocity);
		send_release(0, ch, note, m, 2);
	} else {
		__wrap_hal_send_midi(port, (uint8_t)(0x80 | (ch & 0xF)), note & 0x7F,
		                     (uint8_t)ump_scale_down(velocity, 16, 7));
	}
}

void hal_send_poly_pressure2(u8 port, u8 ch, u8 note, u32 value)
{
	if (standalone_midi2(port)) {
		uint32_t m[2];
		ump_m2_poly_pressure(m, 0, ch, note, (uint32_t)value);
		ump_tx_push_pressure(0, ch & 0xF, note & 0x7F, m);
	} else {
		__wrap_hal_send_midi(port, (uint8_t)(0xA0 | (ch & 0xF)), note & 0x7F,
		                     (uint8_t)ump_scale_down((uint32_t)value, 32, 7));
	}
}

void hal_send_channel_pressure2(u8 port, u8 ch, u32 value)
{
	if (standalone_midi2(port)) {
		uint32_t m[2];
		ump_m2_channel_pressure(m, 0, ch, (uint32_t)value);
		ump_tx_push_pressure(0, ch & 0xF, 0xFF, m);
	} else {
		__wrap_hal_send_midi(port, (uint8_t)(0xD0 | (ch & 0xF)),
		                     (uint8_t)ump_scale_down((uint32_t)value, 32, 7), 0);
	}
}

/* ---- linker wraps (-Wl,--wrap=...) of main.o and app call sites ---- */

void __wrap_midi_parsers_init(void)
{
	__real_midi_parsers_init();
	memcpy(parser_snapshot, USBParserToDev, sizeof parser_snapshot);
	parser_snapshot_valid = 1;
	ump_stream_set_protocol_hook(midi2_protocol_changing);
}

/* Main-loop wraps: run the pending service first. */

void __wrap_midi_parsers_poll(void)
{
	uint32_t s;

	usb_midi2_service();
	if (!alt) {
		__real_midi_parsers_poll();
		return;
	}
	if (!rx_paused)
		ump_rx_poll();
	/* re-arm EP1 OUT once the ring has room for a whole 64-byte packet */
	s = irq_save();
	if (alt && !pending_service && GetEPRxStatus(1) == EP_RX_NAK && ump_rx_free_words() >= 16)
		SetEPRxValid(1);
	irq_restore(s);
	midi_parse_rx_byte_stream(&MIDIDinIn, DinRx, DINMIDI);
}

void __wrap_usbmidi_poll(void)
{
	uint8_t pkt[64];
	uint32_t s;

	usb_midi2_service();
	if (!alt) {
		__real_usbmidi_poll();
		return;
	}
	/* atomic with respect to set_alt, which NAKs EP1 and resets the queue */
	s = irq_save();
	if (alt && bDeviceState == USB_STATE_CONFIGURED && GetEPTxStatus(1) != EP_TX_VALID) {
		uint8_t n = ump_tx_fill_packet(pkt);
		if (n) {
			USB_SIL_Write(0x81, pkt, n);
			SetEPTxStatus(1, EP_TX_VALID);
		}
	}
	irq_restore(s);
}

void __wrap_midi_flush_windows_buffer(void)
{
	usb_midi2_service();
	if (!alt)
		__real_midi_flush_windows_buffer();
}

/* hal_send_* wraps: may run in the timer interrupt, so no service here. */

void __wrap_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2)
{
	uint32_t m[2];
	uint8_t words, kind = (uint8_t)(status >> 4);

	if (port > USBMIDI || !alt) {
		__real_hal_send_midi(port, status, d1, d2);
		return;
	}
	words = ump_from_midi1(m, port, status, d1, d2,
	                       port == USBSTANDALONE && ump_stream_protocol() == UMP_PROTO_MIDI2);
	if (!words)
		return;					/* SysEx bytes, invalid status */
	if (status >= 0xF0)
		ump_tx_push(UMP_TX_CONTROL, m, words);
	else if (kind == 0x9 && (d2 & 0x7F))
		send_attack(port, status & 0xF, d1, m, words);
	else if (kind == 0x8 || kind == 0x9)
		send_release(port, status & 0xF, d1, m, words);
	else
		ump_tx_push(UMP_TX_NOTE, m, words);
}

void __wrap_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len)
{
	if (port > USBMIDI || !alt) {
		__real_hal_send_sysex(port, data, len);
		return;
	}
	ump_tx_push_sysex7(port, data, len);		/* all or nothing */
}
