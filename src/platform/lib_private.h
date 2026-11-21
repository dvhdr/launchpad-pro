#ifndef LIB_PRIVATE_H
#define LIB_PRIVATE_H

/*
 * Internals of the closed lib/launchpad_pro.a that the USB MIDI 2.0 code
 * needs. Layouts come from the archive disassembly
 * (docs/notes/usb_prop_original.md). DinRx and USBParserToDev are local
 * symbols of c_midi.o that the build makes global with objcopy.
 */

#include <stdint.h>

/* cx_fifo.o: 258 bytes; head (pop index) at 0x100, tail (push index) at 0x101 */
typedef struct {
	uint8_t buf[256];
	uint8_t head, tail;
} cx_fifo;

typedef char lib_private_cx_fifo_size[sizeof(cx_fifo) == 258 ? 1 : -1];

/* cx_fifo1k.o: 0x408 bytes, layout not needed here */
typedef struct cx_fifo1k cx_fifo1k;

extern cx_fifo MIDIToDev, MIDIDinIn;
/* MIDIToHst: c_midi's USB IN queue; MIDIDinOut (c_midi.o, common): DIN Out */
extern cx_fifo1k MIDIToHst, MIDIDinOut;

void fifo_flush(cx_fifo *f);
void fifo1k_flush(cx_fifo1k *f);
/* 1 if not empty, else 0 ((push ^ pop) & 0x3FF, cx_fifo1k.o) */
uint8_t fifo1k_unempty(cx_fifo1k *f);
uint8_t fifo_unempty(cx_fifo *f);
void fifo_push1(cx_fifo *f, uint8_t b);

/* c_midi.o parser state (24 bytes each) */
extern uint8_t DinRx[24], USBParserToDev[24];

void midi_parse_rx_byte_stream(cx_fifo *f, void *parser, uint8_t port);

/* cx_pad.o */
extern uint16_t ADC_Max[64];

/* c_init.o: 1 = bus powered, 2 = PSU (see Device_Reset notes) */
extern uint8_t PowerMode;

/* main.o: flags for remember_USBConfigured / remember_USBUnconfigured */
void USBConfigured(void);
void USBUnconfigured(void);

/* the originals behind the linker wraps (-Wl,--wrap=...) */
void __real_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2);
void __real_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len);
void __real_usbmidi_poll(void);
void __real_midi_flush_windows_buffer(void);
void __real_midi_parsers_poll(void);
void __real_midi_parsers_init(void);

#endif
