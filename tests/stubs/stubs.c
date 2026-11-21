#include "stubs.h"
#include "app.h"
#include <string.h>

void (*stub_irq_hook)(void) = 0;

#include "platform/platform.h"

void platform_product_instance_id(char out[25])
{
	const char *id = "TEST0001";
	int i = 0;
	while (id[i]) {
		out[i] = id[i];
		i++;
	}
	out[i] = 0;
}

struct stub_midi_ev stub_midi_log[64];
int stub_midi_n;
int stub_midi_total;
struct stub_midi_ev stub_midi_last;
uint8_t stub_sysex_buf[320];
uint16_t stub_sysex_len;
uint8_t stub_sysex_port;
int stub_sysex_calls;

static void stub_usb_reset(void);

void stubs_reset(void)
{
	stub_usb_reset();
	stub_midi_n = 0;
	stub_midi_total = 0;
	stub_sysex_len = 0;
	stub_sysex_port = 0;
	stub_sysex_calls = 0;
	memset(stub_sysex_buf, 0, sizeof stub_sysex_buf);
}

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
	stub_midi_total++;
	stub_midi_last.port = port;
	stub_midi_last.status = status;
	stub_midi_last.d1 = d1;
	stub_midi_last.d2 = d2;
	if (stub_midi_n < 64) {
		stub_midi_log[stub_midi_n].port = port;
		stub_midi_log[stub_midi_n].status = status;
		stub_midi_log[stub_midi_n].d1 = d1;
		stub_midi_log[stub_midi_n].d2 = d2;
		stub_midi_n++;
	}
}

void app_sysex_event(u8 port, u8 *data, u16 count)
{
	stub_sysex_calls++;
	stub_sysex_port = port;
	stub_sysex_len = count;
	if (count > sizeof stub_sysex_buf)
		count = sizeof stub_sysex_buf;
	memcpy(stub_sysex_buf, data, count);
}

/* ---- ST USB library, c_midi and fifo stand-ins (Task 7) ---- */

#include "usb/usb_compat.h"
#include "platform/lib_private.h"

struct stub_state stub;

/* closed-library data the firmware takes from the archive */
DEVICE_INFO Device_Info;
DEVICE_INFO *pInformation = &Device_Info;
volatile uint32_t bDeviceState;
uint8_t PowerMode;
cx_fifo MIDIToDev, MIDIDinIn;
struct cx_fifo1k {
	uint32_t a, b;
	uint8_t buf[1024];
};
cx_fifo1k MIDIToHst;
uint8_t DinRx[24], USBParserToDev[24];
uint16_t ADC_Max[64];

void stub_rx_packet(const uint8_t *p, uint16_t n)
{
	if (n > sizeof stub.rx_packet)
		n = sizeof stub.rx_packet;
	memcpy(stub.rx_packet, p, n);
	stub.rx_len = n;
}

static void stub_usb_reset(void)
{
	memset(&stub, 0, sizeof stub);
}

void SetEPType(uint8_t ep, uint16_t type)
{
	stub.ep_type[ep & 7] = type;
}

void SetEPTxStatus(uint8_t ep, uint16_t state)
{
	stub.ep_tx_status[ep & 7] = state;
}

void SetEPRxStatus(uint8_t ep, uint16_t state)
{
	stub.ep_rx_status[ep & 7] = state;
}

void SetEPRxValid(uint8_t ep)
{
	stub.ep_rx_status[ep & 7] = EP_RX_VALID;
	if ((ep & 7) == 1)
		stub.ep_rx_valid_calls++;
}

void SetEPTxAddr(uint8_t ep, uint16_t addr)
{
	stub.ep_tx_addr[ep & 7] = addr;
}

void SetEPRxAddr(uint8_t ep, uint16_t addr)
{
	stub.ep_rx_addr[ep & 7] = addr;
}

void SetEPTxCount(uint8_t ep, uint16_t count)
{
	stub.ep_tx_count[ep & 7] = count;
}

void SetEPRxCount(uint8_t ep, uint16_t count)
{
	stub.ep_rx_count[ep & 7] = count;
}

void ClearDTOG_RX(uint8_t ep)
{
	stub.dtog_rx_clears[ep & 7]++;
}

void ClearDTOG_TX(uint8_t ep)
{
	stub.dtog_tx_clears[ep & 7]++;
}

uint16_t GetEPTxStatus(uint8_t ep)
{
	return stub.ep_tx_status[ep & 7];
}

uint16_t GetEPRxStatus(uint8_t ep)
{
	return stub.ep_rx_status[ep & 7];
}

uint32_t USB_SIL_Read(uint8_t ep_addr, uint8_t *buf)
{
	(void)ep_addr;
	memcpy(buf, stub.rx_packet, stub.rx_len);
	/* the hardware leaves the endpoint NAK after a reception */
	stub.ep_rx_status[ep_addr & 7] = EP_RX_NAK;
	return stub.rx_len;
}

uint32_t USB_SIL_Write(uint8_t ep_addr, uint8_t *buf, uint32_t n)
{
	(void)ep_addr;
	if (n > sizeof stub.last_tx)
		n = sizeof stub.last_tx;
	memcpy(stub.last_tx, buf, n);
	stub.last_tx_len = (uint16_t)n;
	stub.tx_writes++;
	return n;
}

/* same behaviour as usb_core.o's (see docs/notes/usb_prop_original.md) */
uint8_t *Standard_GetDescriptorData(uint16_t len, ONE_DESCRIPTOR *desc)
{
	uint16_t off = pInformation->Ctrl_Info.Usb_wOffset;
	if (len == 0) {
		pInformation->Ctrl_Info.Usb_wLength = (uint16_t)(desc->Descriptor_Size - off);
		return 0;
	}
	return desc->Descriptor + off;
}

void SetDeviceAddress(uint8_t addr)
{
	stub.device_address = addr;
	stub.set_device_address_calls++;
}

void Clear_Status_Out(uint8_t ep)
{
	(void)ep;
	stub.clear_status_out_calls++;
}

void NOP_Process(void)
{
}

void SetBTABLE(uint16_t addr)
{
	stub.btable = addr;
	stub.btable_calls++;
}

RESULT PowerOn(void)
{
	stub.power_on_calls++;
	return USB_SUCCESS;
}

uint32_t USB_SIL_Init(void)
{
	stub.sil_init_calls++;
	return 0;
}

void USBConfigured(void)
{
	stub.usb_configured_calls++;
}

void USBUnconfigured(void)
{
	stub.usb_unconfigured_calls++;
}

void fifo_flush(cx_fifo *f)
{
	if (f == &MIDIToDev)
		stub.fifo_flush_calls++;
	else
		stub.fifo_flush_other_calls++;
	f->head = 0;
	f->tail = 0;
}

void fifo1k_flush(cx_fifo1k *f)
{
	if (f == &MIDIToHst)
		stub.fifo1k_flush_calls++;
	f->b = f->a;
}

uint8_t fifo_unempty(cx_fifo *f)
{
	return f->head != f->tail;
}

void fifo_push1(cx_fifo *f, uint8_t b)
{
	f->buf[f->tail++] = b;
}

void midi_parse_rx_byte_stream(cx_fifo *f, void *parser, uint8_t port)
{
	(void)f;
	stub.din_parse_calls++;
	stub.din_parse_last_parser = parser;
	stub.din_parse_last_port = port;
}

void __real_midi_parsers_init(void)
{
	int i;
	stub.real_parsers_init_calls++;
	for (i = 0; i < 24; i++)
		USBParserToDev[i] = STUB_PARSER_INIT_BYTE(i);
}

void __real_midi_parsers_poll(void)
{
	stub.real_parsers_poll_calls++;
}

void __real_usbmidi_poll(void)
{
	stub.real_usbmidi_poll_calls++;
}

void __real_midi_flush_windows_buffer(void)
{
	stub.real_flush_calls++;
}

void __real_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2)
{
	stub.real_midi_calls++;
	stub.real_midi_last[0] = port;
	stub.real_midi_last[1] = status;
	stub.real_midi_last[2] = d1;
	stub.real_midi_last[3] = d2;
}

void __real_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len)
{
	stub.real_sysex_calls++;
	stub.real_sysex_port = port;
	stub.real_sysex_data = data;
	stub.real_sysex_len = len;
}
