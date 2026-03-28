#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static int _passed = 0, _failed = 0;

#define SECTION(name) printf("\n[%s]\n", (name))

#define CHECK(cond, label)                        \
	do                                            \
	{                                             \
		if (cond)                                 \
		{                                         \
			++_passed;                            \
		}                                         \
		else                                      \
		{                                         \
			++_failed;                            \
			fprintf(stderr, "  FAIL %s:%d  %s\n", \
					__FILE__, __LINE__, (label)); \
		}                                         \
	} while (0)

/* C-строка — strcmp */
#define CHECK_CSTR(got, expected, label)                     \
	do                                                       \
	{                                                        \
		bool _ok = (got) && strcmp((got), (expected)) == 0;  \
		CHECK(_ok, label);                                   \
		if (!_ok)                                            \
			fprintf(stderr, "         got '%s' want '%s'\n", \
					(got) ? (got) : "(null)", (expected));   \
	} while (0)

/* Итоговая строка + код возврата */
#define TEST_REPORT()                                          \
	do                                                         \
	{                                                          \
		printf("\n────────────────────────────────\n");        \
		printf("passed: %d   failed: %d\n", _passed, _failed); \
	} while (0)

#endif /* TEST_H */
