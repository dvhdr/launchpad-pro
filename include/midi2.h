#ifndef MIDI2_H
#define MIDI2_H

/*
 * USB MIDI 2.0 additions to the app API (app.h stays unchanged).
 *
 * Ports and groups: USBSTANDALONE is UMP group 0, USBMIDI is group 1, DINMIDI
 * is the 5-pin DIN port. Only the Standalone group uses the MIDI 2.0 Protocol
 * (when the host selects it); the MIDI port is always MIDI 1.0 (MT 2).
 *
 * All functions here may be called from the main loop or from the app's timer
 * interrupt (app_timer_event).
 */

#include "app_defs.h"

/* 1 when the MIDI 2.0 alternate setting is active and the Standalone group is
 * in the MIDI 2.0 Protocol, else 0. */
u8 midi2_active(void);

/* The current USB MIDIStreaming alternate setting: 0 (USB MIDI 1.0) or 1 (UMP). */
u8 midi2_usb_alt(void);

/* High-resolution sends. Where the destination is MIDI 1.0 (DINMIDI, USB alt 0,
 * USBMIDI, or the Standalone group in MIDI 1.0 Protocol) the value is reduced
 * to 7 bits (velocity: top 7 of 16 bits, Note On at least 1; pressure: top 7
 * of 32 bits). Otherwise a MIDI 2.0 Channel Voice message is sent. */
void hal_send_note_on2(u8 port, u8 ch, u8 note, u16 velocity);
void hal_send_note_off2(u8 port, u8 ch, u8 note, u16 velocity);
void hal_send_poly_pressure2(u8 port, u8 ch, u8 note, u32 value);
void hal_send_channel_pressure2(u8 port, u8 ch, u32 value);

/* Debug checkpoint only: while paused, received UMP is not dispatched to the
 * app (the RX ring fills and EP1 OUT stays NAK). */
void midi2_debug_pause_rx(u8 paused);

#endif
