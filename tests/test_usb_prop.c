#include "test.h"
#include "stubs/stubs.h"
#include "usb/usb_compat.h"
#include "usb/usb_desc.h"
#include "usb/usb_midi2.h"
#include "platform/lib_private.h"
#include "ump/ump.h"
#include <stdint.h>
#include <string.h>

/* usb_prop.c / usb_endp.c / the wraps in usb_midi2.c */
extern DEVICE_PROP Device_Property;
extern USER_STANDARD_REQUESTS User_Standard_Requests;
extern DEVICE Device_Table;
extern uint8_t USBDesc_StringSerial[20];
extern uint8_t DeviceDescriptor[18];
void usb_unconnected(void);
void EP1_OUT_Callback(void);
void __wrap_midi_parsers_init(void);
void __wrap_midi_parsers_poll(void);
void __wrap_usbmidi_poll(void);
void __wrap_midi_flush_windows_buffer(void);
void __wrap_hal_send_midi(uint8_t port, uint8_t status, uint8_t d1, uint8_t d2);
void __wrap_hal_send_sysex(uint8_t port, const uint8_t *data, uint16_t len);

/* ---- control-transfer driver, mimicking usb_core.o (see docs/notes) ---- */

static void setup_request(uint8_t type, uint8_t req, uint16_t wValue, uint16_t wIndex,
                          uint16_t wLength)
{
	pInformation->USBbmRequestType = type;
	pInformation->USBbRequest = req;
	pInformation->USBwValues.bw.bb1 = (uint8_t)(wValue >> 8);
	pInformation->USBwValues.bw.bb0 = (uint8_t)wValue;
	pInformation->USBwIndexs.bw.bb1 = (uint8_t)(wIndex >> 8);
	pInformation->USBwIndexs.bw.bb0 = (uint8_t)wIndex;
	pInformation->USBwLengths.w = wLength;
}

/* Standard_SetConfiguration / Standard_SetInterface, then Class_NoData_Setup */
static RESULT drive_nodata_setup(void)
{
	DEVICE_INFO *p = pInformation;
	uint8_t recipient = p->USBbmRequestType & 0x7F;
	RESULT r = USB_UNSUPPORT;
	if (recipient == 0 && p->USBbRequest == 0x09) {
		if (p->USBwValue0 <= Device_Table.Total_Configuration && p->USBwValue1 == 0 &&
		    p->USBwIndexs.w == 0) {
			p->Current_Configuration = p->USBwValue0;
			User_Standard_Requests.User_SetConfiguration();
			r = USB_SUCCESS;
		}
	} else if (recipient == 1 && p->USBbRequest == 0x0B) {
		r = Device_Property.Class_Get_Interface_Setting(p->USBwIndex0, p->USBwValue0);
		if (p->Current_Configuration != 0 && r == USB_SUCCESS && p->USBwIndex1 == 0 &&
		    p->USBwValue1 == 0) {
			User_Standard_Requests.User_SetInterface();
			p->Current_Interface = p->USBwIndex0;
			p->Current_AlternateSetting = p->USBwValue0;
			r = USB_SUCCESS;
		} else {
			r = USB_UNSUPPORT;
		}
	}
	if (r != USB_SUCCESS)
		r = Device_Property.Class_NoData_Setup(p->USBbRequest);
	return r;
}

static uint8_t *std_get_interface(uint16_t len)
{
	if (len == 0) {
		pInformation->Ctrl_Info.Usb_wLength = 1;
		return 0;
	}
	User_Standard_Requests.User_GetInterface();
	return &pInformation->Current_AlternateSetting;
}

static RESULT data_result;
static uint16_t data_len;
static uint8_t data_buf[512];

/* Data_Setup0 + DataStageIn: route, CopyData(0), clamp to wLength, then read
 * the data in MaxPacketSize chunks through CopyData. */
static void drive_data_setup(void)
{
	DEVICE_INFO *p = pInformation;
	uint8_t *(*copy)(uint16_t) = 0;
	uint8_t recipient = p->USBbmRequestType & 0x7F;
	RESULT r;

	memset(data_buf, 0, sizeof data_buf);
	data_len = 0;
	p->Ctrl_Info.Usb_wLength = 0;
	p->Ctrl_Info.Usb_wOffset = 0;
	p->Ctrl_Info.CopyData = 0;

	if (p->USBbRequest == 0x06 && recipient == 0) {
		if (p->USBwValue1 == 1)
			copy = Device_Property.GetDeviceDescriptor;
		else if (p->USBwValue1 == 2)
			copy = Device_Property.GetConfigDescriptor;
		else if (p->USBwValue1 == 3)
			copy = Device_Property.GetStringDescriptor;
	} else if (p->USBbRequest == 0x0A && recipient == 1 && p->Current_Configuration != 0 &&
	           p->USBwValues.w == 0 && p->USBwIndex1 == 0 &&
	           Device_Property.Class_Get_Interface_Setting(p->USBwIndex0, 0) == USB_SUCCESS) {
		copy = std_get_interface;
	}

	if (copy) {
		p->Ctrl_Info.Usb_wOffset = 0;
		p->Ctrl_Info.CopyData = copy;
		copy(0);
		r = USB_SUCCESS;
	} else {
		r = Device_Property.Class_Data_Setup(p->USBbRequest);
	}
	data_result = r;
	if (r != USB_SUCCESS || p->Ctrl_Info.Usb_wLength == 0) {
		data_result = USB_UNSUPPORT;	/* STALL */
		return;
	}
	if (p->Ctrl_Info.Usb_wLength > p->USBwLengths.w)
		p->Ctrl_Info.Usb_wLength = p->USBwLengths.w;
	while (p->Ctrl_Info.Usb_wLength > 0 && data_len < sizeof data_buf) {
		uint16_t n = p->Ctrl_Info.Usb_wLength;
		uint8_t *src;
		if (n > Device_Property.MaxPacketSize)
			n = Device_Property.MaxPacketSize;
		src = p->Ctrl_Info.CopyData(n);
		memcpy(data_buf + data_len, src, n);
		data_len += n;
		p->Ctrl_Info.Usb_wLength -= n;
		p->Ctrl_Info.Usb_wOffset += n;
	}
}

static uint16_t drive_data_setup_len(void)
{
	drive_data_setup();
	return data_len;
}

static const uint8_t *drive_data_setup_bytes(void)
{
	drive_data_setup();
	return data_buf;
}

static RESULT drive_data_setup_result(void)
{
	drive_data_setup();
	return data_result;
}

/* bus reset, SET_CONFIGURATION 1, main loop ran once; counters cleared */
static void enter_configured(void)
{
	stubs_reset();
	memset(&Device_Info, 0, sizeof Device_Info);
	bDeviceState = 0;
	PowerMode = 0;
	Device_Property.Reset();
	setup_request(0x00, 0x09, 1, 0, 0);
	drive_nodata_setup();
	usb_midi2_service();
	stubs_reset();
}

static void enter_alt1(void)
{
	enter_configured();
	setup_request(0x01, 0x0B, 1, 1, 0);
	drive_nodata_setup();
	usb_midi2_service();
	stubs_reset();
}

/* ---- tests ---- */

static void test_set_interface_switches_owner(void)
{
	enter_configured();
	CHECK_EQ(usb_midi2_alt(), 0);
	setup_request(0x01, 0x0B, 1, 1, 0);			/* SET_INTERFACE if1 alt1 */
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	CHECK_EQ(usb_midi2_alt(), 1);
	CHECK_EQ(stub.dtog_rx_clears[1], 1);
	CHECK_EQ(stub.dtog_tx_clears[1], 1);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);		/* quiesced in the ISR */
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_NAK);
	CHECK_EQ(stub.fifo_flush_calls, 0);			/* legacy cleanup deferred */
	CHECK_EQ(stub.fifo1k_flush_calls, 0);
	CHECK_EQ(Device_Info.Current_Interface, 1);
	CHECK_EQ(Device_Info.Current_AlternateSetting, 1);

	usb_midi2_service();					/* main loop */
	CHECK_EQ(stub.fifo_flush_calls, 1);
	CHECK_EQ(stub.fifo1k_flush_calls, 1);
	CHECK_EQ(stub.fifo_flush_other_calls, 0);		/* MIDIDinIn untouched */
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_VALID);		/* 0x3000 */
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_NAK);
	CHECK_EQ(stub.ep_rx_valid_calls, 1);

	usb_midi2_service();					/* nothing pending: no-op */
	CHECK_EQ(stub.fifo_flush_calls, 1);
	CHECK_EQ(stub.ep_rx_valid_calls, 1);

	/* SET_INTERFACE on interface 0 does not touch the MIDI 2.0 owner */
	stubs_reset();
	setup_request(0x01, 0x0B, 0, 0, 0);
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	CHECK_EQ(usb_midi2_alt(), 1);
	CHECK_EQ(stub.dtog_rx_clears[1], 0);

	/* back to alt 0 on interface 1 */
	setup_request(0x01, 0x0B, 0, 1, 0);
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	CHECK_EQ(usb_midi2_alt(), 0);
	CHECK_EQ(stub.dtog_rx_clears[1], 1);
}

static void test_set_interface_needs_configuration(void)
{
	enter_configured();
	Device_Info.Current_Configuration = 0;
	setup_request(0x01, 0x0B, 1, 1, 0);
	CHECK_EQ(drive_nodata_setup(), USB_UNSUPPORT);
	CHECK_EQ(usb_midi2_alt(), 0);
	CHECK_EQ(stub.dtog_rx_clears[1], 0);
}

static void test_get_interface_per_interface(void)
{
	enter_configured();
	setup_request(0x81, 0x0A, 0, 1, 1);
	CHECK_EQ(drive_data_setup_len(), 1);
	CHECK_EQ(data_buf[0], 0);

	setup_request(0x01, 0x0B, 1, 1, 0);
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	setup_request(0x81, 0x0A, 0, 0, 1);			/* GET_INTERFACE if0 */
	CHECK_EQ(drive_data_setup_len(), 1);
	CHECK_EQ(data_buf[0], 0);
	setup_request(0x81, 0x0A, 0, 1, 1);			/* GET_INTERFACE if1 */
	CHECK_EQ(drive_data_setup_len(), 1);
	CHECK_EQ(data_buf[0], 1);
	setup_request(0x81, 0x0A, 0, 0, 1);			/* if0 again after if1 */
	CHECK_EQ(drive_data_setup_bytes()[0], 0);
	CHECK_EQ(usb_midi2_iface_alt(0), 0);
	CHECK_EQ(usb_midi2_iface_alt(1), 1);
	CHECK_EQ(usb_midi2_iface_alt(2), 0);
	setup_request(0x81, 0x0A, 0, 2, 1);			/* no interface 2 */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
}

static void test_invalid_alt_rejected(void)
{
	enter_configured();
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(0, 0), USB_SUCCESS);
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(1, 0), USB_SUCCESS);
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(1, 1), USB_SUCCESS);
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(0, 1), USB_UNSUPPORT);
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(1, 2), USB_UNSUPPORT);
	CHECK_EQ(Device_Property.Class_Get_Interface_Setting(2, 0), USB_UNSUPPORT);

	setup_request(0x01, 0x0B, 2, 1, 0);			/* SET_INTERFACE if1 alt2 */
	CHECK_EQ(drive_nodata_setup(), USB_UNSUPPORT);
	setup_request(0x01, 0x0B, 1, 0, 0);			/* SET_INTERFACE if0 alt1 */
	CHECK_EQ(drive_nodata_setup(), USB_UNSUPPORT);
	CHECK_EQ(usb_midi2_alt(), 0);
	CHECK_EQ(stub.dtog_rx_clears[1], 0);
}

static void test_gtb_request_in_alt0(void)
{
	enter_configured();
	CHECK_EQ(usb_midi2_alt(), 0);
	setup_request(0x81, 0x06, 0x2601, 1, 5);		/* header only */
	CHECK_EQ(drive_data_setup_len(), 5);
	CHECK_MEM(data_buf, usb_gtb_desc, 5);
	setup_request(0x81, 0x06, 0x2601, 1, 255);
	CHECK_EQ(drive_data_setup_len(), 31);
	CHECK_MEM(drive_data_setup_bytes(), usb_gtb_desc, 31);
	setup_request(0x81, 0x06, 0x2600, 1, 255);		/* alt 0 has no GTB */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0x81, 0x06, 0x2601, 0, 255);		/* interface 0 has none */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0x81, 0x06, 0x2401, 1, 255);		/* other type */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0x01, 0x06, 0x2601, 1, 255);		/* host-to-device: not a GET */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0xA1, 0x06, 0x2601, 1, 255);		/* class type */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);

	/* also served in alt 1 */
	enter_alt1();
	setup_request(0x81, 0x06, 0x2601, 1, 255);
	CHECK_EQ(drive_data_setup_len(), 31);
	CHECK_MEM(data_buf, usb_gtb_desc, 31);
}

static void test_class_requests_unsupported(void)
{
	enter_configured();
	setup_request(0xA1, 0x81, 0x0100, 0x0001, 2);		/* class GET_CUR (orig: jump to 0) */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0x21, 0x01, 0x0100, 0x0001, 2);		/* class SET_CUR */
	CHECK_EQ(Device_Property.Class_Data_Setup(0x01), USB_UNSUPPORT);
	setup_request(0x80, 0x06, 0x0F00, 0, 5);		/* BOS: no such descriptor */
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);
	setup_request(0x21, 0x0A, 0, 1, 0);			/* class no-data */
	CHECK_EQ(drive_nodata_setup(), USB_UNSUPPORT);
}

static void test_reset_restores_alt0(void)
{
	enter_alt1();
	CHECK_EQ(usb_midi2_alt(), 1);
	bDeviceState = 5;
	Device_Info.Current_AlternateSetting = 1;
	Device_Info.Current_Interface = 1;
	Device_Property.Reset();
	CHECK_EQ(usb_midi2_alt(), 0);
	CHECK_EQ(stub.usb_unconfigured_calls, 1);
	CHECK_EQ(bDeviceState, 1);
	CHECK_EQ(Device_Info.Current_Configuration, 0);
	CHECK_EQ(Device_Info.Current_Interface, 0);
	CHECK_EQ(Device_Info.Current_AlternateSetting, 0);
	CHECK_EQ(Device_Info.Current_Feature, 0x80);
	CHECK_EQ(stub.btable_calls, 1);
	CHECK_EQ(stub.btable, 0);
	CHECK_EQ(stub.set_device_address_calls, 1);
	CHECK_EQ(stub.device_address, 0);
	CHECK_EQ(stub.clear_status_out_calls, 1);
	/* EP0 */
	CHECK_EQ(stub.ep_type[0], EP_CONTROL);
	CHECK_EQ(stub.ep_tx_status[0], EP_TX_STALL);
	CHECK_EQ(stub.ep_rx_addr[0], 0x40);
	CHECK_EQ(stub.ep_tx_addr[0], 0x48);
	CHECK_EQ(stub.ep_rx_count[0], 8);
	CHECK_EQ(stub.ep_rx_status[0], EP_RX_VALID);
	/* EP1 */
	CHECK_EQ(stub.ep_type[1], EP_BULK);
	CHECK_EQ(stub.ep_tx_addr[1], 0x90);
	CHECK_EQ(stub.ep_rx_addr[1], 0x50);
	CHECK_EQ(stub.ep_tx_count[1], 64);
	CHECK_EQ(stub.ep_rx_count[1], 64);
	CHECK_EQ(stub.dtog_rx_clears[1], 1);
	CHECK_EQ(stub.dtog_tx_clears[1], 1);
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_NAK);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);		/* until the main loop runs */
	usb_midi2_service();
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_VALID);
	CHECK_EQ(stub.ep_tx_status[1], EP_TX_NAK);
	CHECK_EQ(stub.fifo_flush_calls, 1);

	/* a reset while not configured does not call USBUnconfigured */
	stubs_reset();
	Device_Property.Reset();
	CHECK_EQ(stub.usb_unconfigured_calls, 0);
}

static void test_reset_picks_power_descriptor(void)
{
	enter_configured();
	PowerMode = 1;
	Device_Info.Current_Feature = 0;
	Device_Property.Reset();
	CHECK_EQ(Device_Info.Current_Feature, usb_config_desc_bus[7]);
	PowerMode = 2;
	Device_Info.Current_Feature = 0;
	Device_Property.Reset();
	CHECK_EQ(Device_Info.Current_Feature, usb_config_desc_psu[7]);
	PowerMode = 0;
}

static void test_reset_mid_traffic(void)
{
	static const uint32_t sx_start[2] = {0x30160102, 0x03040506};	/* SysEx7 start */
	static const uint32_t sx_whole[2] = {0x30037E7F, 0x06000000};	/* complete: 7E 7F 06 */
	static const uint32_t note[2] = {0x40903C00, 0x80000000};
	static const uint8_t want[5] = {0xF0, 0x7E, 0x7F, 0x06, 0xF7};
	uint8_t din_before[24], pkt[64];
	int i;

	stubs_reset();
	__wrap_midi_parsers_init();				/* snapshot = A0.. pattern */
	CHECK_EQ(stub.real_parsers_init_calls, 1);
	enter_alt1();

	for (i = 0; i < 24; i++) {
		DinRx[i] = (uint8_t)(0x11 * i);
		USBParserToDev[i] = 0xEE;			/* legacy parser mid-message */
	}
	memcpy(din_before, DinRx, sizeof din_before);
	MIDIToDev.head = 3;
	MIDIToDev.tail = 9;

	ump_rx_push_words(sx_start, 2);
	ump_rx_poll();						/* reassembler holds a start */
	for (i = 0; i < 3; i++)
		CHECK_EQ(ump_tx_push_attack(note, 2), 0);
	CHECK(ump_rx_free_words() == UMP_RX_WORDS);
	ump_rx_push_words(sx_start, 2);				/* still queued at reset */
	CHECK(ump_rx_free_words() < UMP_RX_WORDS);

	Device_Property.Reset();				/* USB reset ISR */
	CHECK_EQ(ump_rx_free_words(), UMP_RX_WORDS);
	CHECK_EQ(ump_tx_fill_packet(pkt), 0);
	CHECK_EQ(ump_tx_reserve_words(), 0);
	CHECK_EQ(ump_stream_protocol(), UMP_PROTO_MIDI2);
	CHECK_EQ(usb_midi2_alt(), 0);

	usb_midi2_service();					/* main loop */
	CHECK_EQ(MIDIToDev.head, 0);
	CHECK_EQ(MIDIToDev.tail, 0);
	for (i = 0; i < 24; i++)
		CHECK_EQ(USBParserToDev[i], STUB_PARSER_INIT_BYTE(i));
	CHECK_MEM(DinRx, din_before, sizeof din_before);

	ump_rx_push_words(sx_whole, 2);
	ump_rx_poll();
	CHECK_EQ(stub_sysex_calls, 1);
	CHECK_EQ(stub_sysex_len, 5);
	CHECK_MEM(stub_sysex_buf, want, 5);
	ump_rx_poll();
	CHECK_EQ(stub_sysex_calls, 1);
}

static void test_set_configuration(void)
{
	stubs_reset();
	memset(&Device_Info, 0, sizeof Device_Info);
	bDeviceState = 4;
	setup_request(0x00, 0x09, 0, 0, 0);			/* configuration 0 */
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	CHECK_EQ(stub.usb_configured_calls, 0);
	CHECK_EQ(bDeviceState, 4);

	setup_request(0x00, 0x09, 1, 0, 0);
	CHECK_EQ(drive_nodata_setup(), USB_SUCCESS);
	CHECK_EQ(stub.usb_configured_calls, 1);
	CHECK_EQ(bDeviceState, 5);
	CHECK_EQ(usb_midi2_alt(), 0);
	CHECK(stub.dtog_rx_clears[1] >= 1);			/* alt state reset */

	setup_request(0x00, 0x09, 2, 0, 0);			/* only one configuration */
	CHECK_EQ(drive_nodata_setup(), USB_UNSUPPORT);
	CHECK_EQ(Device_Table.Total_Endpoint, 2);
	CHECK_EQ(Device_Table.Total_Configuration, 1);
}

static void test_set_device_address(void)
{
	stubs_reset();
	bDeviceState = 5;
	User_Standard_Requests.User_SetDeviceAddress();
	CHECK_EQ(stub.usb_unconfigured_calls, 1);
	CHECK_EQ(bDeviceState, 4);
	User_Standard_Requests.User_SetDeviceAddress();
	CHECK_EQ(stub.usb_unconfigured_calls, 1);
	CHECK_EQ(bDeviceState, 4);
}

static void test_init_and_unconnected(void)
{
	stubs_reset();
	Device_Property.Init();
	CHECK_EQ(stub.power_on_calls, 1);
	CHECK_EQ(stub.sil_init_calls, 1);
	bDeviceState = 5;
	usb_unconnected();
	CHECK_EQ(bDeviceState, 0);
	CHECK_EQ(Device_Property.MaxPacketSize, 8);
}

static void check_string(uint8_t idx, const char *ascii)
{
	uint16_t n = (uint16_t)strlen(ascii), i;
	setup_request(0x80, 0x06, (uint16_t)(0x0300 | idx), 0x0409, 255);
	CHECK_EQ(drive_data_setup_len(), 2 + 2 * n);
	CHECK_EQ(data_buf[0], 2 + 2 * n);
	CHECK_EQ(data_buf[1], 3);
	for (i = 0; i < n; i++) {
		CHECK_EQ(data_buf[2 + 2 * i], (uint8_t)ascii[i]);
		CHECK_EQ(data_buf[3 + 2 * i], 0);
	}
}

static void test_string_descriptors(void)
{
	static const uint8_t langid[4] = {4, 3, 0x09, 0x04};
	uint8_t saved[20];

	enter_configured();
	setup_request(0x80, 0x06, 0x0300, 0, 255);
	CHECK_EQ(drive_data_setup_len(), 4);
	CHECK_MEM(data_buf, langid, 4);
	check_string(1, "Focusrite A.E. Ltd");
	check_string(2, "Launchpad Pro MIDI 2.0");
	check_string(3, "Launchpad Open");
	check_string(USB_STR_PORT_STANDALONE, "Standalone Port");
	check_string(USB_STR_PORT_MIDI, "MIDI Port");
	setup_request(0x80, 0x06, 0x0306, 0, 255);
	CHECK_EQ(drive_data_setup_result(), USB_UNSUPPORT);

	/* header only (Windows asks for 2 bytes first) */
	setup_request(0x80, 0x06, 0x0302, 0x0409, 2);
	CHECK_EQ(drive_data_setup_len(), 2);
	CHECK_EQ(data_buf[0], 46);

	/* device-ID suffix written by device_setuppid (device_init.o) */
	memcpy(saved, USBDesc_StringSerial, sizeof saved);
	USBDesc_StringSerial[14] = ' ';
	USBDesc_StringSerial[15] = '1';
	USBDesc_StringSerial[16] = '6';
	check_string(2, "Launchpad Pro MIDI 2.0 16");
	check_string(3, "Launchpad Open 16");
	memcpy(USBDesc_StringSerial, saved, sizeof saved);
}

static void test_device_and_config_descriptors(void)
{
	enter_configured();
	CHECK_MEM(DeviceDescriptor, usb_device_desc, 18);
	setup_request(0x80, 0x06, 0x0100, 0, 64);
	CHECK_EQ(drive_data_setup_len(), 18);
	CHECK_MEM(data_buf, usb_device_desc, 18);

	/* the PID patch from device_setuppid reaches the host */
	DeviceDescriptor[10] = 0x53;
	setup_request(0x80, 0x06, 0x0100, 0, 64);
	CHECK_EQ(drive_data_setup_bytes()[10], 0x53);
	DeviceDescriptor[10] = usb_device_desc[10];

	PowerMode = 0;
	setup_request(0x80, 0x06, 0x0200, 0, 9);
	CHECK_EQ(drive_data_setup_len(), 9);
	CHECK_MEM(data_buf, usb_config_desc_bus, 9);
	setup_request(0x80, 0x06, 0x0200, 0, 0xFFFF);
	CHECK_EQ(drive_data_setup_len(), USB_CONFIG_DESC_LEN);
	CHECK_MEM(data_buf, usb_config_desc_bus, USB_CONFIG_DESC_LEN);
	PowerMode = 2;
	CHECK_EQ(drive_data_setup_len(), USB_CONFIG_DESC_LEN);
	CHECK_MEM(data_buf, usb_config_desc_psu, USB_CONFIG_DESC_LEN);
	PowerMode = 1;
	CHECK_EQ(drive_data_setup_len(), USB_CONFIG_DESC_LEN);
	CHECK_MEM(data_buf, usb_config_desc_bus, USB_CONFIG_DESC_LEN);
	PowerMode = 0;
}

static void test_ep1_out_original(void)
{
	static const uint8_t p[8] = {0x09, 0x90, 0x3C, 0x40, 0x19, 0x80, 0x3C, 0x00};
	int i;

	stubs_reset();
	memset(&MIDIToDev, 0, sizeof MIDIToDev);
	stub_rx_packet(p, 8);
	EP1_OUT_Callback();
	CHECK_EQ(MIDIToDev.tail - MIDIToDev.head, 8);
	for (i = 0; i < 8; i++)
		CHECK_EQ(MIDIToDev.buf[i], p[i]);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);
	CHECK_EQ(stub.ep_rx_valid_calls, 0);

	/* fifo not drained yet: the packet is dropped and EP1 stays NAK */
	EP1_OUT_Callback();
	CHECK_EQ(MIDIToDev.tail - MIDIToDev.head, 8);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_NAK);
	memset(&MIDIToDev, 0, sizeof MIDIToDev);
}

static void test_wraps_pass_through(void)
{
	static const uint8_t sx[3] = {0xF0, 0x01, 0xF7};
	enter_configured();
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub.real_parsers_poll_calls, 1);
	__wrap_usbmidi_poll();
	CHECK_EQ(stub.real_usbmidi_poll_calls, 1);
	__wrap_midi_flush_windows_buffer();
	CHECK_EQ(stub.real_flush_calls, 1);
	__wrap_hal_send_midi(1, 0x90, 60, 100);
	CHECK_EQ(stub.real_midi_calls, 1);
	CHECK_EQ(stub.real_midi_last[0], 1);
	CHECK_EQ(stub.real_midi_last[1], 0x90);
	CHECK_EQ(stub.real_midi_last[2], 60);
	CHECK_EQ(stub.real_midi_last[3], 100);
	__wrap_hal_send_sysex(2, sx, 3);
	CHECK_EQ(stub.real_sysex_calls, 1);
	CHECK_EQ(stub.real_sysex_port, 2);
	CHECK(stub.real_sysex_data == sx);
	CHECK_EQ(stub.real_sysex_len, 3);
	CHECK_EQ(stub.fifo_flush_calls, 0);			/* nothing was pending */

	/* the poll wraps run the pending service before the legacy code */
	usb_midi2_set_alt(0);
	__wrap_usbmidi_poll();
	CHECK_EQ(stub.fifo_flush_calls, 1);
	CHECK_EQ(stub.ep_rx_status[1], EP_RX_VALID);
	usb_midi2_set_alt(0);
	__wrap_midi_parsers_poll();
	CHECK_EQ(stub.fifo_flush_calls, 2);
	usb_midi2_set_alt(0);
	__wrap_midi_flush_windows_buffer();
	CHECK_EQ(stub.fifo_flush_calls, 3);
}

void test_usb_prop_all(void)
{
	RUN(test_set_interface_switches_owner);
	RUN(test_set_interface_needs_configuration);
	RUN(test_get_interface_per_interface);
	RUN(test_invalid_alt_rejected);
	RUN(test_gtb_request_in_alt0);
	RUN(test_class_requests_unsupported);
	RUN(test_reset_restores_alt0);
	RUN(test_reset_picks_power_descriptor);
	RUN(test_reset_mid_traffic);
	RUN(test_set_configuration);
	RUN(test_set_device_address);
	RUN(test_init_and_unconnected);
	RUN(test_string_descriptors);
	RUN(test_device_and_config_descriptors);
	RUN(test_ep1_out_original);
	RUN(test_wraps_pass_through);
}
