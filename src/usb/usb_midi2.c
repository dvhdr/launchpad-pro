/*
 * USB MIDI 2.0 EP1 ownership: which side (c_midi's USB MIDI 1.0 path in alt 0,
 * the UMP path in alt 1) owns EP1, and the switch between them.
 *
 * usb_midi2_set_alt runs in the USB interrupt and touches only what is safe
 * there; the cleanup of c_midi's main-loop-owned fifos and parser is deferred
 * to usb_midi2_service, which the main-loop wraps run before any legacy code.
 */
#include "usb/usb_midi2.h"
#include "usb/usb_compat.h"
#include "platform/lib_private.h"
#include "platform/irq.h"
#include "ump/ump.h"
#include <string.h>

static volatile uint8_t alt;
static volatile uint8_t pending_service;

/* USBParserToDev right after midi_parsers_init (its initial state) */
static uint8_t parser_snapshot[sizeof USBParserToDev];
static uint8_t parser_snapshot_valid;

/* Notes sounding on the Standalone group, one bit per (channel, note):
 * sounding[ch][note >> 3] bit (note & 7). Filled by the data path. */
static uint8_t sounding[16][16];

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

/* ---- linker wraps (-Wl,--wrap=...) of main.o and app call sites ---- */

void __wrap_midi_parsers_init(void)
{
	__real_midi_parsers_init();
	memcpy(parser_snapshot, USBParserToDev, sizeof parser_snapshot);
	parser_snapshot_valid = 1;
}

/* Data-path wraps: pass-throughs until the UMP data path lands. The main-loop
 * ones run the pending service first; hal_send_* may be called from the
 * app's timer interrupt, so they do not. */

void __wrap_midi_parsers_poll(void)
{
	usb_midi2_service();
	__real_midi_parsers_poll();
}

void __wrap_usbmidi_poll(void)
{
	usb_midi2_service();
	__real_usbmidi_poll();
}

void __wrap_midi_flush_windows_buffer(void)
{
	usb_midi2_service();
	__real_midi_flush_windows_buffer();
}

void __wrap_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2)
{
	__real_hal_send_midi(port, status, d1, d2);
}

void __wrap_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len)
{
	__real_hal_send_sysex(port, data, len);
}
