#include "test.h"

int g_test_failures = 0;

int test_failures(void)
{
	return g_test_failures;
}

void test_harness_all(void);

int main(void)
{
	test_harness_all();

	printf("%d failures\n", test_failures());
	return test_failures() ? 1 : 0;
}
