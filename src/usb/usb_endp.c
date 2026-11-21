/*
 * EP1 OUT (host → device) callback; replaces usb_endp.o of the closed archive.
 *
 * Alt 0 (USB MIDI 1.0): the original behaviour (docs/notes/usb_prop_original.md):
 * the packet goes byte by byte into MIDIToDev for c_midi's parser, or is
 * dropped with EP1 RX NAK if MIDIToDev still holds data. EP1 RX is left NAK;
 * midi_parsers_poll re-arms it once the fifos are empty.
 *
 * Alt 1 (UMP): the packet's whole little-endian words go into the UMP RX ring
 * (a trailing partial word is dropped). EP1 RX is left NAK; the
 * midi_parsers_poll wrap re-arms it once the ring has room for a full packet.
 */
#include "usb/usb_compat.h"
#include "usb/usb_midi2.h"
#include "platform/lib_private.h"
#include "ump/ump.h"

/* 64 bytes, word-aligned so alt 1 can decode the words in place. Only
 * usb_endp.o used this symbol in the archive. */
uint32_t Bulk_Data_Buff[16];

void EP1_OUT_Callback(void)
{
	uint8_t *b = (uint8_t *)Bulk_Data_Buff;
	uint16_t n = (uint16_t)USB_SIL_Read(1, b);
	uint16_t i;

	if (usb_midi2_alt()) {
		if (n > sizeof Bulk_Data_Buff)
			n = sizeof Bulk_Data_Buff;
		n /= 4;
		/* in place: word i is built from bytes 4i..4i+3 before it is stored */
		for (i = 0; i < n; i++)
			Bulk_Data_Buff[i] = (uint32_t)b[4 * i] | ((uint32_t)b[4 * i + 1] << 8) |
			                    ((uint32_t)b[4 * i + 2] << 16) |
			                    ((uint32_t)b[4 * i + 3] << 24);
		ump_rx_push_words(Bulk_Data_Buff, n);
		return;
	}

	if (fifo_unempty(&MIDIToDev)) {
		SetEPRxStatus(1, EP_RX_NAK);
		return;
	}
	for (i = 0; i < n; i++)
		fifo_push1(&MIDIToDev, b[i]);
}
