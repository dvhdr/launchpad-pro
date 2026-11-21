#ifndef STUBS_H
#define STUBS_H

#include <stdint.h>

extern void (*stub_irq_hook)(void);

struct stub_midi_ev {
	uint8_t port, status, d1, d2;
};
extern struct stub_midi_ev stub_midi_log[64];
extern int stub_midi_n;
extern int stub_midi_total;
extern struct stub_midi_ev stub_midi_last;
extern uint8_t stub_sysex_buf[320];
extern uint16_t stub_sysex_len;
extern uint8_t stub_sysex_port;
extern int stub_sysex_calls;

/* Recording stand-ins for the ST USB library, the c_midi/fifo internals of the
 * closed library and the __real_ side of the linker wraps. stubs_reset()
 * zeroes all of it. */
struct stub_state {
	/* endpoint registers, as last written */
	uint16_t ep_type[8];
	uint16_t ep_tx_status[8];
	uint16_t ep_rx_status[8];
	uint16_t ep_tx_addr[8];
	uint16_t ep_rx_addr[8];
	uint16_t ep_tx_count[8];
	uint16_t ep_rx_count[8];
	int dtog_rx_clears[8];
	int dtog_tx_clears[8];
	int ep_rx_valid_calls;		/* SetEPRxValid(1) only */
	int clear_status_out_calls;
	uint16_t btable;
	int btable_calls;
	uint8_t device_address;
	int set_device_address_calls;
	int power_on_calls;
	int sil_init_calls;
	int usb_configured_calls;
	int usb_unconfigured_calls;

	/* fifos */
	int fifo_flush_calls;		/* fifo_flush(&MIDIToDev) */
	int fifo_flush_other_calls;	/* fifo_flush on any other fifo */
	int fifo1k_flush_calls;		/* fifo1k_flush(&MIDIToHst) */

	/* c_midi (__real_ side of the wraps) */
	int real_parsers_init_calls;
	int real_parsers_poll_calls;
	int real_usbmidi_poll_calls;
	int real_flush_calls;
	int real_midi_calls;
	uint8_t real_midi_last[4];	/* port, status, d1, d2 */
	int real_sysex_calls;
	uint8_t real_sysex_port;
	const uint8_t *real_sysex_data;
	uint16_t real_sysex_len;
	int din_parse_calls;
	uint8_t din_parse_last_port;
	const void *din_parse_last_parser;

	/* EP1 packets */
	uint8_t rx_packet[64];		/* returned by USB_SIL_Read */
	uint16_t rx_len;
	uint8_t last_tx[64];		/* last USB_SIL_Write */
	uint16_t last_tx_len;
	int tx_writes;
};
extern struct stub_state stub;

/* Pattern __real_midi_parsers_init writes into USBParserToDev. */
#define STUB_PARSER_INIT_BYTE(i) ((uint8_t)(0xA0 + (i)))

void stub_rx_packet(const uint8_t *p, uint16_t n);
void stubs_reset(void);

#endif
