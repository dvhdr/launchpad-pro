#include "test.h"

static void test_harness_basics(void)
{
	CHECK_EQ(1 + 1, 2);
	CHECK_MEM("ab", "ab", 2);
}

void test_harness_all(void)
{
	RUN(test_harness_basics);
}
