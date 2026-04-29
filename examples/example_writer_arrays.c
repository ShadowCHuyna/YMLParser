/*
 * example_writer_arrays.c — YML_ARRAY: YMLCreateArr, YMLArrPush,
 *                           YMLMapAddArr, вложенные массивы.
 *
 * Сборка:
 *   make run-example E=example_writer_arrays
 */

#include <stdio.h>
#include "YMLParser.h"

int main(void)
{
	/* ── плоский массив смешанных типов ────────────────────────────── */
	YMLValue *mixed = _YMLCreateArr();
	YMLArrPush(mixed, (long long)1);
	YMLArrPush(mixed, 2.5);
	YMLArrPush(mixed, "three");
	YMLArrPush(mixed, (bool)true);
	YMLArrPushNull(mixed);

	printf("=== mixed array ===\n");
	YMLWriteStream(mixed, stdout);
	YMLDestroy(mixed);

	/* ── объект с C-массивами ──────────────────────────────────────── */
	YMLValue *obj = _YMLCreate();

	long long scores[] = {95, 87, 100, 73};
	YMLMapAddArr(obj, "scores", scores, 4);

	const char *tags[] = {"yaml", "parser", "c11"};
	YMLMapAddArr(obj, "tags", tags, 3);

	/* вложенный массив как значение ключа */
	YMLValue *matrix_row = _YMLCreateArr();
	YMLArrPush(matrix_row, (long long)1);
	YMLArrPush(matrix_row, (long long)2);
	YMLArrPush(matrix_row, (long long)3);
	YMLMapAdd(obj, "row", matrix_row);
	YMLDestroy(matrix_row);

	printf("=== object with arrays ===\n");
	YMLWriteStream(obj, stdout);
	YMLDestroy(obj);

	/* ── массив объектов ───────────────────────────────────────────── */
	YMLValue *users = _YMLCreateArr();
	const char *names[] = {"Alice", "Bob", "Carol"};
	const long long ages[] = {30, 25, 28};

	for (int i = 0; i < 3; i++)
	{
		YMLValue *user = _YMLCreate();
		YMLMapAdd(user, "name", names[i]);
		YMLMapAdd(user, "age",  ages[i]);
		YMLArrPush(users, user);
		YMLDestroy(user);
	}

	printf("=== array of objects ===\n");
	YMLWriteStream(users, stdout);
	YMLDestroy(users);

	return 0;
}
