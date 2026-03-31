/*
 * example_errors.c — все паттерны обработки ошибок.
 *
 * Сборка:
 *   make run-example E=example_errors
 */

#include <stdio.h>
#include <string.h>
#include "YMLParser.h"

/* ── helpers ── */
static const char *type_name(YMLValueType t)
{
	switch (t)
	{
	case YML_NULL:
		return "null";
	case YML_BOOL:
		return "bool";
	case YML_INT:
		return "int";
	case YML_FLOAT:
		return "float";
	case YML_STRING:
		return "string";
	case YML_ARRAY:
		return "array";
	case YML_OBJECT:
		return "object";
	default:
		return "?";
	}
}

int main(void)
{
	int ok = 0;
	char *err = NULL;

	/* ═══════════════════════════════════════════════════════════════════
	 * 1. Паттерн А: явная проверка после каждого вызова (.ok + .error)
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("=== 1. Явная проверка (.ok / .error) ===\n");

	const char *good_yaml = "host: localhost\nport: 8080\n";
	YMLValue *root = YMLParse(good_yaml, .ok = &ok, .error = &err);
	if (ok != 0)
	{
		fprintf(stderr, "parse error %d: %s\n", ok, err);
		return 1;
	}

	YMLValue *port = YMLMapGet(root->value.object, "port", .ok = &ok, .error = &err, .type = YML_INT);
	if (ok != 0)
		fprintf(stderr, "  MapGet error %d: %s\n", ok, err);
	else
		printf("  port = %lld\n", (long long)port->value.integer);

	YMLDestroy(root);

	/* ═══════════════════════════════════════════════════════════════════
	 * 2. Паттерн Б: глобальное состояние + YMLErrorPrint в конце блока
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 2. Глобальное состояние (YMLErrorPrint) ===\n");

	root = YMLParse("x: 1\ny: 2\n");
	YMLValue *x = YMLMapGet(root->value.object, "x");
	YMLValue *y = YMLMapGet(root->value.object, "y");
	/* Один вызов в конце: если что-то пошло не так — напечатает и вернёт код. */
	if (YMLErrorPrint() != 0)
	{
		YMLDestroy(root);
		return 1;
	}
	printf("  x=%lld  y=%lld\n", (long long)x->value.integer, (long long)y->value.integer);
	YMLDestroy(root);

	/* ═══════════════════════════════════════════════════════════════════
	 * 3. Синтаксическая ошибка парсера
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 3. Синтаксическая ошибка ===\n");

	root = YMLParse("key: *undefined_anchor\n", .ok = &ok, .error = &err);
	printf("  ok=%d  root=%s\n", ok, root ? "non-null" : "NULL");
	if (ok != 0)
		printf("  error: %s\n", err);

	/* ═══════════════════════════════════════════════════════════════════
	 * 4. Ключ не найден (ok == 1)
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 4. Ключ не найден ===\n");

	root = YMLParse("a: 1\n");
	YMLValue *missing = YMLMapGet(root->value.object, "b", .ok = &ok, .error = &err);
	printf("  ok=%d  value=%s\n", ok, missing ? "non-null" : "NULL");
	if (ok == 1)
		printf("  причина: %s\n", err);
	YMLDestroy(root);

	/* ═══════════════════════════════════════════════════════════════════
	 * 5. Несовпадение типа (.type=YML_INT, а значение — строка)
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 5. Несовпадение типа ===\n");

	root = YMLParse("count: 'not a number'\n");
	YMLValue *count = YMLMapGet(root->value.object, "count", .ok = &ok, .error = &err, .type = YML_INT);
	printf("  ok=%d  value=%s\n", ok, count ? "non-null" : "NULL");
	if (ok == 2)
		printf("  причина: %s\n", err);

	/* Без .type — любой тип принимается. */
	count = YMLMapGet(root->value.object, "count", .ok = &ok);
	if (ok == 0)
		printf("  без .type — тип=%s  значение='%s'\n", type_name(count->type), count->value.string);
	YMLDestroy(root);

	/* ═══════════════════════════════════════════════════════════════════
	 * 6. Типичный "ранний выход" без goto
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 6. Ранний выход без goto ===\n");

	const char *cfg =
		"server:\n"
		"  host: example.com\n"
		"  port: 443\n";

	do
	{
		root = YMLParse(cfg, .ok = &ok);
		if (ok != 0)
			break;

		YMLValue *srv = YMLMapGet(root->value.object, "server", .ok = &ok, .type = YML_OBJECT);
		if (ok != 0)
			break;

		YMLValue *host = YMLMapGet(srv->value.object, "host", .ok = &ok, .type = YML_STRING);
		if (ok != 0)
			break;

		YMLValue *prt = YMLMapGet(srv->value.object, "port", .ok = &ok, .type = YML_INT);
		if (ok != 0)
			break;

		printf("  host=%s  port=%lld\n", host->value.string, (long long)prt->value.integer);
	} while (0);

	if (ok != 0)
	{
		fprintf(stderr, "  ошибка %d: %s\n", ok, err ? err : "");
		YMLDestroy(root);
		return ok;
	}
	YMLDestroy(root);

	/* ═══════════════════════════════════════════════════════════════════
	 * 7. ArrayLen(NULL) безопасен — возвращает 0
	 * ═══════════════════════════════════════════════════════════════════ */
	printf("\n=== 7. ArrayLen(NULL) ===\n");
	printf("  ArrayLen(NULL) = %zu  (безопасно, не UB)\n", ArrayLen(NULL));

	root = YMLParse("items: []\n");
	YMLValue *items = YMLMapGet(root->value.object, "items");
	printf("  ArrayLen([]) = %zu\n", ArrayLen(items->value.array));
	YMLDestroy(root);

	return 0;
}
