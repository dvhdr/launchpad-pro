#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

extern int g_test_failures;

int test_failures(void);

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			g_test_failures++; \
		} \
	} while (0)

#define CHECK_EQ(a, b) \
	do { \
		unsigned long long check_a_ = (unsigned long long)(a); \
		unsigned long long check_b_ = (unsigned long long)(b); \
		if (check_a_ != check_b_) { \
			printf("FAIL %s:%d: %s == %s (0x%llx vs 0x%llx)\n", \
				__FILE__, __LINE__, #a, #b, check_a_, check_b_); \
			g_test_failures++; \
		} \
	} while (0)

#define CHECK_MEM(a, b, n) \
	do { \
		if (memcmp((a), (b), (n)) != 0) { \
			printf("FAIL %s:%d: memcmp(%s, %s, %s) != 0\n", \
				__FILE__, __LINE__, #a, #b, #n); \
			g_test_failures++; \
		} \
	} while (0)

#define RUN(test_fn) \
	do { \
		printf("run %s\n", #test_fn); \
		test_fn(); \
	} while (0)

#endif
