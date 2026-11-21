#include "test.h"
#include "platform/pads.h"

/* c_surface.o .rodata + 0x50: library pad slot -> grid pad index. This is the
 * table surface_pad_velocitycallback uses to turn the slot cx_pad.o's
 * pad_calculate passes (and indexes ADC_Max with) into the app's pad index. */
static const u8 lib_slot_to_index[64] = {
	81, 82, 83, 84, 85, 86, 87, 88,
	71, 72, 73, 74, 75, 76, 77, 78,
	61, 62, 63, 64, 65, 66, 67, 68,
	51, 52, 53, 54, 55, 56, 57, 58,
	41, 42, 43, 44, 45, 46, 47, 48,
	31, 32, 33, 34, 35, 36, 37, 38,
	21, 22, 23, 24, 25, 26, 27, 28,
	11, 12, 13, 14, 15, 16, 17, 18,
};

static void test_slot_matches_library_table(void)
{
	int s;
	for (s = 0; s < 64; s++)
		CHECK_EQ(pad_slot(lib_slot_to_index[s]), s);
}

static void test_slot_rejects_non_grid(void)
{
	static const u8 bad[] = {0, 1, 8, 9, 10, 19, 20, 29, 80, 89, 90, 91, 98, 99, 100, 255};
	unsigned i;
	for (i = 0; i < sizeof(bad); i++)
		CHECK_EQ(pad_slot(bad[i]), PAD_NO_SLOT);
}

static void test_peak_to_velocity(void)
{
	/* ADC_Max holds (12-bit ADC + 2) >> 2: 0..1024 */
	CHECK_EQ(pad_peak_to_velocity(0), 0x0000);
	CHECK_EQ(pad_peak_to_velocity(1), 0x0040);
	CHECK_EQ(pad_peak_to_velocity(512), 0x8000);
	CHECK_EQ(pad_peak_to_velocity(1023), 0xFFFF);
	CHECK_EQ(pad_peak_to_velocity(1024), 0xFFFF);
	CHECK_EQ(pad_peak_to_velocity(0xFFFF), 0xFFFF);
}

void test_pads_all(void)
{
	RUN(test_slot_matches_library_table);
	RUN(test_slot_rejects_non_grid);
	RUN(test_peak_to_velocity);
}
