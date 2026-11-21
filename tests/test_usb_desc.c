#include "test.h"
#include "usb/usb_desc.h"
#include <stdint.h>
#include <stdio.h>

static void test_alt0_byte_identical(void)
{
	uint8_t g[266];
	FILE *f = fopen(GOLDEN_DESC, "rb");
	CHECK(f != NULL);
	CHECK(f && fread(g, 1, 266, f) == 266);
	if (f)
		fclose(f);
	CHECK_MEM(usb_config_desc_psu + 4, g + 4, 129);	/* skip wTotalLength */
	CHECK_MEM(usb_config_desc_bus + 4, g + 0x85 + 4, 129);
	CHECK_EQ(usb_config_desc_psu[8], 0x01);
	CHECK_EQ(usb_config_desc_bus[8], 0xFA);
	CHECK_EQ(USB_ALT0_LEN, 133);
}

static void walk_config(const uint8_t *d)
{
	unsigned pos = 0, alts1 = 0, cs_eps = 0, in_if1 = 0;
	CHECK_EQ(d[2] | d[3] << 8, USB_CONFIG_DESC_LEN);
	while (pos < USB_CONFIG_DESC_LEN) {
		uint8_t len = d[pos], type = d[pos + 1];
		CHECK(len >= 2);
		if (len < 2)
			return;
		CHECK(pos + len <= USB_CONFIG_DESC_LEN);
		if (type == 0x04) {
			in_if1 = d[pos + 2] == 1;
			if (in_if1 && d[pos + 3] == 1)
				alts1++;
		}
		if (pos >= 133 && type == 0x24 && d[pos + 2] == 0x01) {
			CHECK_EQ(d[pos + 3] | d[pos + 4] << 8, 0x0200);	/* bcdMSC */
			CHECK_EQ(d[pos + 5] | d[pos + 6] << 8, 7);	/* wTotalLength */
		}
		if (pos >= 133 && type == 0x25) {
			CHECK_EQ(d[pos + 2], 0x02);	/* MS_GENERAL_2_0 */
			CHECK_EQ(d[pos + 3], 2);	/* two GTBs */
			CHECK_EQ(d[pos + 4], 1);
			CHECK_EQ(d[pos + 5], 2);
			cs_eps++;
		}
		pos += len;
	}
	CHECK_EQ(pos, USB_CONFIG_DESC_LEN);
	CHECK_EQ(alts1, 1);	/* exactly one alt 1 of interface 1 (plus alt 0) */
	CHECK_EQ(cs_eps, 2);
	CHECK_EQ(in_if1, 1);
}

static void test_lengths_walk(void)
{
	unsigned alt0_if1 = 0, pos;
	walk_config(usb_config_desc_bus);
	walk_config(usb_config_desc_psu);
	for (pos = 0; pos < USB_CONFIG_DESC_LEN; pos += usb_config_desc_bus[pos])
		if (usb_config_desc_bus[pos + 1] == 0x04 && usb_config_desc_bus[pos + 2] == 1)
			alt0_if1++;
	CHECK_EQ(alt0_if1, 2);	/* interface 1: alt 0 and alt 1 */
}

static void test_gtb(void)
{
	CHECK_EQ(usb_gtb_desc[3] | usb_gtb_desc[4] << 8, 31);
	CHECK_EQ(usb_gtb_desc[5 + 8], 0x11);
	CHECK_EQ(usb_gtb_desc[18 + 8], 0x03);
	CHECK_EQ(usb_gtb_desc[18 + 5], 0x01);	/* block 2 -> group index 1 */
	CHECK_EQ(usb_gtb_desc[5 + 7], USB_STR_PORT_STANDALONE);
	CHECK_EQ(usb_gtb_desc[18 + 7], USB_STR_PORT_MIDI);
}

static void test_device_desc(void)
{
	CHECK_EQ(usb_device_desc[12], 0x00);
	CHECK_EQ(usb_device_desc[13], 0x03);
	CHECK_EQ(usb_device_desc[8] | usb_device_desc[9] << 8, 0x1235);
	CHECK_EQ(usb_device_desc[10] | usb_device_desc[11] << 8, 0x4051);
}

void test_usb_desc_all(void)
{
	RUN(test_alt0_byte_identical);
	RUN(test_lengths_walk);
	RUN(test_gtb);
	RUN(test_device_desc);
}
