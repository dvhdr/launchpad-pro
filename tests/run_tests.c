#include "test.h"

int g_test_failures = 0;

int test_failures(void)
{
	return g_test_failures;
}

void test_harness_all(void);
void test_ump_codec_all(void);
void test_ump_tx_all(void);

int main(void)
{
	test_harness_all();
	test_ump_codec_all();
	test_ump_tx_all();

	printf("%d failures\n", test_failures());
	return test_failures() ? 1 : 0;
}
