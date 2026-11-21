/*
 * EP1 OUT (host → device) callback. Replaces usb_endp.o of the closed archive
 * with the same alt 0 (USB MIDI 1.0) behaviour (docs/notes/usb_prop_original.md):
 * the packet goes byte by byte into MIDIToDev for c_midi's parser, or is
 * dropped with EP1 RX NAK if MIDIToDev still holds data. EP1 RX is left NAK;
 * midi_parsers_poll re-arms it once the fifos are empty.
 */
#include "usb/usb_compat.h"
#include "platform/lib_private.h"

uint8_t Bulk_Data_Buff[64];

void EP1_OUT_Callback(void)
{
	uint16_t n = (uint16_t)USB_SIL_Read(1, Bulk_Data_Buff);
	uint16_t i;

	if (fifo_unempty(&MIDIToDev)) {
		SetEPRxStatus(1, EP_RX_NAK);
		return;
	}
	for (i = 0; i < n; i++)
		fifo_push1(&MIDIToDev, Bulk_Data_Buff[i]);
}
