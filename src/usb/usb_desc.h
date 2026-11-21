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

extern const uint8_t usb_device_desc[18];
extern const uint8_t usb_config_desc_bus[USB_CONFIG_DESC_LEN];  /* bMaxPower 0xFA */
extern const uint8_t usb_config_desc_psu[USB_CONFIG_DESC_LEN];  /* bMaxPower 0x01 */
extern const uint8_t usb_gtb_desc[31];

#endif
