#ifndef USB_DESC_H
#define USB_DESC_H

#include <stdint.h>

#define USB_CONFIG_DESC_LEN 175

/* the original 133-byte USB MIDI 1.0 configuration (alt 0) sits at the start
 * of the new descriptor; alt 1 (UMP) follows it */
#define USB_ALT0_OFFSET 0
#define USB_ALT0_LEN 133

/* string indexes (verified against the original Device_GetStringDescriptor):
 * 4 = "Standalone Port" (cable 0 / group 0), 5 = "MIDI Port" (cable 1 / group 1) */
#define USB_STR_PORT_STANDALONE 4
#define USB_STR_PORT_MIDI 5

/* device descriptor bytes; usb_prop.c also keeps a writable copy
 * (DeviceDescriptor) because device_setuppid patches idProduct in RAM */
#define USB_DEVICE_DESC_BYTES \
	0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, \
	0x35, 0x12, 0x51, 0x40, 0x00, 0x03, 0x01, 0x02, 0x03, 0x01

extern const uint8_t usb_device_desc[18];
extern const uint8_t usb_config_desc_bus[USB_CONFIG_DESC_LEN];  /* bMaxPower 0xFA */
extern const uint8_t usb_config_desc_psu[USB_CONFIG_DESC_LEN];  /* bMaxPower 0x01 */
extern const uint8_t usb_gtb_desc[31];

#endif
