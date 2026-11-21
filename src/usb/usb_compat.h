#ifndef USB_COMPAT_H
#define USB_COMPAT_H

/*
 * Types and entry points of the ST USB-FS device library v3.x objects kept in
 * the closed archive (usb_core.o, usb_init.o, usb_regs.o, usb_sil.o, ...).
 * Written from the documented v3.x layout and checked against the archive
 * disassembly (offsets in docs/notes/usb_prop_original.md); not copied from
 * ST sources.
 */

#include <stdint.h>

typedef enum { USB_SUCCESS = 0, USB_ERROR, USB_UNSUPPORT, USB_NOT_READY } RESULT;

/* Setup0_Process stores wValue and wIndex byte-swapped: the high byte of the
 * request field (bb1) comes first in memory. */
typedef union {
	uint16_t w;
	struct {
		uint8_t bb1;
		uint8_t bb0;
	} bw;
} uint16_t_uint8_t;

typedef struct {
	uint16_t Usb_wLength;
	uint16_t Usb_wOffset;
	uint16_t PacketSize;
	uint8_t *(*CopyData)(uint16_t Length);
} ENDPOINT_INFO;

typedef struct {
	uint8_t USBbmRequestType;		/* 0 */
	uint8_t USBbRequest;			/* 1 */
	uint16_t_uint8_t USBwValues;		/* 2 */
	uint16_t_uint8_t USBwIndexs;		/* 4 */
	uint16_t_uint8_t USBwLengths;		/* 6 */
	uint8_t ControlState;			/* 8 */
	uint8_t Current_Feature;		/* 9 */
	uint8_t Current_Configuration;		/* 10 */
	uint8_t Current_Interface;		/* 11 */
	uint8_t Current_AlternateSetting;	/* 12 */
	ENDPOINT_INFO Ctrl_Info;		/* 16 */
} DEVICE_INFO;

#define USBwValue0 USBwValues.bw.bb0
#define USBwValue1 USBwValues.bw.bb1
#define USBwIndex0 USBwIndexs.bw.bb0
#define USBwIndex1 USBwIndexs.bw.bb1

typedef struct {
	void (*Init)(void);
	void (*Reset)(void);
	void (*Process_Status_IN)(void);
	void (*Process_Status_OUT)(void);
	RESULT (*Class_Data_Setup)(uint8_t RequestNo);
	RESULT (*Class_NoData_Setup)(uint8_t RequestNo);
	RESULT (*Class_Get_Interface_Setting)(uint8_t Interface, uint8_t AlternateSetting);
	uint8_t *(*GetDeviceDescriptor)(uint16_t Length);
	uint8_t *(*GetConfigDescriptor)(uint16_t Length);
	uint8_t *(*GetStringDescriptor)(uint16_t Length);
	void *RxEP_buffer;
	uint8_t MaxPacketSize;			/* 44 */
} DEVICE_PROP;

typedef struct {
	void (*User_GetConfiguration)(void);
	void (*User_SetConfiguration)(void);
	void (*User_GetInterface)(void);
	void (*User_SetInterface)(void);
	void (*User_GetStatus)(void);
	void (*User_ClearFeature)(void);
	void (*User_SetEndPointFeature)(void);
	void (*User_SetDeviceFeature)(void);
	void (*User_SetDeviceAddress)(void);
} USER_STANDARD_REQUESTS;

typedef struct {
	uint8_t Total_Endpoint;
	uint8_t Total_Configuration;
} DEVICE;

typedef struct {
	uint8_t *Descriptor;
	uint16_t Descriptor_Size;
} ONE_DESCRIPTOR;

#ifndef HOST_TEST
/* layout checks against the disassembly (32-bit target only) */
typedef char usb_compat_device_info_size[sizeof(DEVICE_INFO) == 28 ? 1 : -1];
typedef char usb_compat_ctrl_info_at_16[__builtin_offsetof(DEVICE_INFO, Ctrl_Info) == 16 ? 1 : -1];
typedef char usb_compat_copydata_at_24[__builtin_offsetof(DEVICE_INFO, Ctrl_Info.CopyData) == 24 ? 1 : -1];
typedef char usb_compat_maxpacket_at_44[__builtin_offsetof(DEVICE_PROP, MaxPacketSize) == 44 ? 1 : -1];
typedef char usb_compat_device_prop_size[sizeof(DEVICE_PROP) == 48 ? 1 : -1];
typedef char usb_compat_user_requests_size[sizeof(USER_STANDARD_REQUESTS) == 36 ? 1 : -1];
#endif

/* bDeviceState values (usb_pwr.o) */
#define USB_STATE_UNCONNECTED 0
#define USB_STATE_ATTACHED 1
#define USB_STATE_ADDRESSED 4
#define USB_STATE_CONFIGURED 5

/* endpoint register values */
#define EP_BULK 0x0000
#define EP_CONTROL 0x0200
#define EP_TX_STALL 0x0010
#define EP_TX_NAK 0x0020
#define EP_TX_VALID 0x0030
#define EP_RX_STALL 0x1000
#define EP_RX_NAK 0x2000
#define EP_RX_VALID 0x3000

/* standard requests and descriptor types used here */
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_RECIPIENT_MASK 0x7F		/* type and recipient bits */
#define USB_STD_INTERFACE 0x01

extern DEVICE_INFO *pInformation;
extern DEVICE_INFO Device_Info;
extern volatile uint32_t bDeviceState;

void SetBTABLE(uint16_t addr);
void SetEPType(uint8_t ep, uint16_t type);
void SetEPTxStatus(uint8_t ep, uint16_t state);
void SetEPRxStatus(uint8_t ep, uint16_t state);
void SetEPRxValid(uint8_t ep);
void SetEPTxAddr(uint8_t ep, uint16_t addr);
void SetEPRxAddr(uint8_t ep, uint16_t addr);
void SetEPTxCount(uint8_t ep, uint16_t count);
void SetEPRxCount(uint8_t ep, uint16_t count);
void ClearDTOG_RX(uint8_t ep);
void ClearDTOG_TX(uint8_t ep);
uint16_t GetEPTxStatus(uint8_t ep);
uint16_t GetEPRxStatus(uint8_t ep);
void Clear_Status_Out(uint8_t ep);
uint32_t USB_SIL_Init(void);
uint32_t USB_SIL_Read(uint8_t ep_addr, uint8_t *buf);
uint32_t USB_SIL_Write(uint8_t ep_addr, uint8_t *buf, uint32_t n);
uint8_t *Standard_GetDescriptorData(uint16_t len, ONE_DESCRIPTOR *desc);
void SetDeviceAddress(uint8_t addr);
void NOP_Process(void);
RESULT PowerOn(void);

#endif
