/*
 * USB device properties: the control-path callbacks the ST USB-FS core calls
 * (Device_Property, User_Standard_Requests) and the descriptor tables.
 *
 * Replaces usb_prop.o / usb_desc.o of the closed archive. Every exported
 * symbol of those objects that another archive member uses is kept, with the
 * original behaviour (docs/notes/usb_prop_original.md), plus:
 * - interface 1 alt 1 (USB MIDI 2.0) and the EP1 owner switch on SET_INTERFACE,
 * - GET_INTERFACE answered per interface,
 * - the Group Terminal Block descriptor request,
 * - product string "Launchpad Pro MIDI 2.0".
 */
#include "usb/usb_compat.h"
#include "usb/usb_desc.h"
#include "usb/usb_midi2.h"
#include "platform/lib_private.h"
#include <string.h>

void Device_Init(void);
void Device_Reset(void);
void Device_Status_In(void);
void Device_Status_Out(void);
RESULT Device_Data_Setup(uint8_t RequestNo);
RESULT Device_NoData_Setup(uint8_t RequestNo);
RESULT Device_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);
uint8_t *Device_GetDeviceDescriptor(uint16_t Length);
uint8_t *Device_GetConfigDescriptor(uint16_t Length);
uint8_t *Device_GetStringDescriptor(uint16_t Length);
void Device_SetConfiguration(void);
void Device_SetDeviceAddress(void);
void Device_GetInterface(void);
void Device_SetInterface(void);
void usb_unconnected(void);

#define USB_STRING_PRODUCT "Launchpad Pro MIDI 2.0"
#define USB_SERIAL_SUFFIX_AT 14		/* strlen("Launchpad Open"), where device_setuppid writes */

/* Writable: device_setuppid (device_init.o) patches idProduct (byte 10). */
uint8_t DeviceDescriptor[18] = {USB_DEVICE_DESC_BYTES};

/* Writable: device_setuppid appends " N" (device number) at byte 14. */
uint8_t USBDesc_StringSerial[20] = "Launchpad Open";

static const uint8_t USBDesc_LangID[2] = {0x09, 0x04};
static const uint8_t USBDesc_StringVendor[] = "Focusrite A.E. Ltd";
static const uint8_t USBDesc_StringProduct[] = USB_STRING_PRODUCT;
static const uint8_t USBDesc_StringPort1[] = "Standalone Port";
static const uint8_t USBDesc_StringPort2[] = "MIDI Port";

/* index → ASCII string; 0 is the LangID pair */
const uint8_t *const USBDescriptors[6] = {
	USBDesc_LangID,
	USBDesc_StringVendor,
	USBDesc_StringProduct,		/* + the suffix in USBDesc_StringSerial */
	USBDesc_StringSerial,
	USBDesc_StringPort1,		/* USB_STR_PORT_STANDALONE */
	USBDesc_StringPort2,		/* USB_STR_PORT_MIDI */
};

ONE_DESCRIPTOR Device_Descriptor = {DeviceDescriptor, sizeof DeviceDescriptor};
ONE_DESCRIPTOR Config_Descriptor = {(uint8_t *)usb_config_desc_bus, USB_CONFIG_DESC_LEN};
ONE_DESCRIPTOR Config_Descriptor_psu = {(uint8_t *)usb_config_desc_psu, USB_CONFIG_DESC_LEN};
static ONE_DESCRIPTOR Gtb_Descriptor = {(uint8_t *)usb_gtb_desc, sizeof usb_gtb_desc};

static uint8_t descriptor_buffer[64];

DEVICE Device_Table = {2, 1};

DEVICE_PROP Device_Property = {
	Device_Init,
	Device_Reset,
	Device_Status_In,
	Device_Status_Out,
	Device_Data_Setup,
	Device_NoData_Setup,
	Device_Get_Interface_Setting,
	Device_GetDeviceDescriptor,
	Device_GetConfigDescriptor,
	Device_GetStringDescriptor,
	0,
	8,
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
	NOP_Process,			/* GetConfiguration */
	Device_SetConfiguration,
	Device_GetInterface,
	Device_SetInterface,
	NOP_Process,			/* GetStatus */
	NOP_Process,			/* ClearFeature */
	NOP_Process,			/* SetEndPointFeature */
	NOP_Process,			/* SetDeviceFeature */
	Device_SetDeviceAddress,
};

void Device_Init(void)
{
	PowerOn();
	USB_SIL_Init();
}

void Device_Reset(void)
{
	const uint8_t *cfg;

	if (bDeviceState == USB_STATE_CONFIGURED)
		USBUnconfigured();

	cfg = PowerMode == 1 ? usb_config_desc_bus : usb_config_desc_psu;
	pInformation->Current_Feature = cfg[7];
	pInformation->Current_Configuration = 0;
	pInformation->Current_Interface = 0;
	pInformation->Current_AlternateSetting = 0;

	SetBTABLE(0);

	/* EP0: control, 8-byte packets */
	SetEPType(0, EP_CONTROL);
	SetEPTxStatus(0, EP_TX_STALL);
	SetEPRxAddr(0, 0x40);
	SetEPTxAddr(0, 0x48);
	Clear_Status_Out(0);
	SetEPRxCount(0, Device_Property.MaxPacketSize);
	SetEPRxValid(0);

	/* EP1: bulk IN/OUT, 64 bytes */
	SetEPType(1, EP_BULK);
	SetEPTxAddr(1, 0x90);
	SetEPTxCount(1, 64);
	SetEPTxStatus(1, EP_TX_NAK);
	SetEPRxAddr(1, 0x50);
	SetEPRxCount(1, 64);
	SetEPRxStatus(1, EP_RX_VALID);

	SetEPRxValid(0);
	SetDeviceAddress(0);
	bDeviceState = USB_STATE_ATTACHED;

	/* alt 0; EP1 RX NAK until the main loop has flushed the legacy state */
	usb_midi2_usb_reset();
}

void Device_Status_In(void)
{
}

void Device_Status_Out(void)
{
}

void Device_SetConfiguration(void)
{
	/* SET_CONFIGURATION resets the interface alt settings and data toggles */
	usb_midi2_usb_reset();
	if (pInformation->Current_Configuration != 0) {
		bDeviceState = USB_STATE_CONFIGURED;
		USBConfigured();
	}
}

void Device_SetDeviceAddress(void)
{
	if (bDeviceState == USB_STATE_CONFIGURED)
		USBUnconfigured();
	bDeviceState = USB_STATE_ADDRESSED;
}

/* Called by Standard_SetInterface after Device_Get_Interface_Setting accepted
 * the request; the core stores Current_Interface/AlternateSetting after. */
void Device_SetInterface(void)
{
	if (pInformation->USBwIndex0 == 1)
		usb_midi2_set_alt(pInformation->USBwValue0);
}

/* Called by Standard_GetInterface, which then returns &Current_AlternateSetting:
 * that byte is shared by both interfaces, so set it for the one asked. */
void Device_GetInterface(void)
{
	pInformation->Current_AlternateSetting = usb_midi2_iface_alt(pInformation->USBwIndex0);
}

static uint8_t *Device_GetGtbDescriptor(uint16_t Length)
{
	return Standard_GetDescriptorData(Length, &Gtb_Descriptor);
}

/* Class_Data_Setup. usb_core.o sends here every GET_DESCRIPTOR that is not
 * standard device-recipient type 1/2/3, and every class request with a data
 * stage. Only the Group Terminal Block request is supported: GET_DESCRIPTOR,
 * standard/interface, wValue = 0x26 << 8 | alt 1, wIndex = interface 1. The
 * core truncates to wLength. */
RESULT Device_Data_Setup(uint8_t RequestNo)
{
	DEVICE_INFO *p = pInformation;

	if (RequestNo == USB_REQ_GET_DESCRIPTOR &&
	    (p->USBbmRequestType & USB_RECIPIENT_MASK) == USB_STD_INTERFACE &&
	    p->USBwValue1 == 0x26 && p->USBwValue0 == 1 &&
	    p->USBwIndex1 == 0 && p->USBwIndex0 == 1) {
		p->Ctrl_Info.CopyData = Device_GetGtbDescriptor;
		p->Ctrl_Info.Usb_wOffset = 0;
		Device_GetGtbDescriptor(0);
		return USB_SUCCESS;
	}
	return USB_UNSUPPORT;
}

RESULT Device_NoData_Setup(uint8_t RequestNo)
{
	(void)RequestNo;
	return USB_UNSUPPORT;
}

/* Interface 0 (AudioControl): alt 0 only. Interface 1 (MIDIStreaming): alt 0
 * (USB MIDI 1.0) or alt 1 (USB MIDI 2.0). */
RESULT Device_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
	if (Interface == 0 && AlternateSetting == 0)
		return USB_SUCCESS;
	if (Interface == 1 && AlternateSetting <= 1)
		return USB_SUCCESS;
	return USB_UNSUPPORT;
}

uint8_t *Device_GetDeviceDescriptor(uint16_t Length)
{
	return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

uint8_t *Device_GetConfigDescriptor(uint16_t Length)
{
	return Standard_GetDescriptorData(Length,
	                                  PowerMode == 2 ? &Config_Descriptor_psu : &Config_Descriptor);
}

/* Appends ASCII s as UTF-16LE at byte len of descriptor_buffer; at most 31
 * characters in all (64-byte buffer), as the original. */
static uint8_t put_ascii(uint8_t len, const uint8_t *s)
{
	while (*s && len < sizeof descriptor_buffer) {
		descriptor_buffer[len] = *s++;
		descriptor_buffer[len + 1] = 0;
		len += 2;
	}
	return len;
}

uint8_t *Device_GetStringDescriptor(uint16_t Length)
{
	uint8_t idx = pInformation->USBwValue0;
	uint8_t len;
	ONE_DESCRIPTOR d;

	if (idx >= sizeof USBDescriptors / sizeof USBDescriptors[0])
		return 0;
	if (idx == 0) {
		descriptor_buffer[2] = USBDesc_LangID[0];
		descriptor_buffer[3] = USBDesc_LangID[1];
		len = 4;
	} else {
		len = put_ascii(2, USBDescriptors[idx]);
		if (idx == 2)	/* keep the device number suffix on the product */
			len = put_ascii(len, USBDesc_StringSerial + USB_SERIAL_SUFFIX_AT);
	}
	descriptor_buffer[0] = len;
	descriptor_buffer[1] = 0x03;
	d.Descriptor = descriptor_buffer;
	d.Descriptor_Size = len;
	return Standard_GetDescriptorData(Length, &d);
}

void usb_unconnected(void)
{
	bDeviceState = USB_STATE_UNCONNECTED;
}
