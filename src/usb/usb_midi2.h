#ifndef USB_MIDI2_H
#define USB_MIDI2_H

#include <stdint.h>

/* Current alternate setting of the MIDIStreaming interface (1): 0 = USB MIDI
 * 1.0 (c_midi owns EP1), 1 = USB MIDI 2.0 / UMP. */
uint8_t usb_midi2_alt(void);

/* Interrupt context (SET_INTERFACE, USB reset, SET_CONFIGURATION). Quiesces
 * EP1 (TX and RX NAK), clears both data toggles, sets the alt, resets the UMP
 * RX, TX and stream state and the sounding-note table, and marks the legacy
 * cleanup pending for usb_midi2_service(). */
void usb_midi2_set_alt(uint8_t alt);

/* usb_midi2_set_alt(0); called from Device_Reset and Device_SetConfiguration. */
void usb_midi2_usb_reset(void);

/* Alternate setting of a given interface: interface 1 → usb_midi2_alt(), others 0. */
uint8_t usb_midi2_iface_alt(uint8_t iface);

/* Main-loop context; no-op unless an alt change is pending. Flushes the
 * legacy MIDIToDev / MIDIToHst fifos, restores the USBParserToDev snapshot
 * taken after midi_parsers_init, then re-arms EP1 RX (TX stays NAK). Called
 * first by the main-loop data-path wraps. */
void usb_midi2_service(void);

#endif
