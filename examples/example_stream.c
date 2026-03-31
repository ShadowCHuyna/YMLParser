/*
 * example_stream.c — парсинг нескольких YAML-документов из одной строки.
 *
 * Сборка:
 *   make run-example E=example_stream
 */

#include <stdio.h>
#include <string.h>
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

/* ─────────────────────────────────────────────────────────────────────
 * Три документа в одном потоке.
 *
 * Разделитель --- начинает новый документ.
 * Маркер ...  завершает текущий (якоря после него уже недоступны).
 * ───────────────────────────────────────────────────────────────────── */
static const char *stream =
	"---\n"
	"type: user\n"
	"name: Alice\n"
	"age:  30\n"
	"...\n"
	"---\n"
	"type: user\n"
	"name: Bob\n"
	"age:  25\n"
	"...\n"
	"---\n"
	"type: summary\n"
	"count: 2\n";

/* ─────────────────────────────────────────────────────────────────────
 * Якоря НЕ переживают границу документа.
 *
 * Во втором документе *ref должен давать ошибку "unknown alias".
 * ───────────────────────────────────────────────────────────────────── */
static const char *anchor_scope_demo =
	"---\n"
	"value: &ref 42\n"
	"...\n"
	"---\n"
	"# *ref здесь недоступен — документ закончился на ...\n"
	"value: *ref\n";

static void print_user(YMLValue *doc)
{
	int ok = 0;
	YMLValue *name = YMLMapGet(doc->value.object, "name", .ok = &ok);
	YMLValue *age = YMLMapGet(doc->value.object, "age", .ok = &ok);
	if (ok != 0)
	{
		printf("  (не user-документ)\n");
		return;
	}
	printf("  name=%-8s age=%lld\n",
		   name->value.string, (long long)age->value.integer);
}

int main(void)
{
	int ok = 0;

	/* ── базовый поток ── */
	printf("=== Базовый поток (%d документа) ===\n", 3);

	YMLValue **docs = YMLParseStream(stream, .ok = &ok);
	if (ok != 0)
	{
		YMLErrorPrint();
		return 1;
	}

	printf("Получено документов: %zu\n", YMLArrayLen(docs));
	for (size_t i = 0; i < YMLArrayLen(docs); i++)
	{
		printf("Документ %zu:\n", i + 1);
		YMLValue *type = YMLMapGet(docs[i]->value.object, "type", .ok = &ok);
		if (ok == 0 && type->type == YML_STRING &&
			strcmp(type->value.string, "user") == 0)
		{
			print_user(docs[i]);
		}
		else
		{
			YMLValue *count = YMLMapGet(docs[i]->value.object, "count", .ok = &ok);
			if (ok == 0)
				printf("  count=%lld\n", (long long)count->value.integer);
		}
	}
	YMLDestroyStream(docs);

	/* ── область видимости якорей ── */
	printf("\n=== Якоря и границы документов ===\n");
	docs = YMLParseStream(anchor_scope_demo, .ok = &ok);
	if (ok != 0)
	{
		/* Ожидаем ошибку "unknown alias" — это правильное поведение. */
		fflush(stdout);
		fprintf(stderr, "Ожидаемая ошибка: ");
		YMLErrorPrint();
	}
	else
	{
		printf("Документов: %zu (якоря не утекли между документами)\n",
			   YMLArrayLen(docs));
	}
	/* docs может содержать частично разобранные документы — освобождаем всегда. */
	if (docs)
		YMLDestroyStream(docs);

	/* ── поток без маркеров ── */
	printf("\n=== Поток без явных --- / ... ===\n");
	const char *implicit =
		"key1: value1\n"
		"---\n"
		"key2: value2\n"
		"---\n"
		"key3: value3\n";

	docs = YMLParseStream(implicit, .ok = &ok);
	if (ok != 0)
	{
		YMLErrorPrint();
		return 1;
	}
	printf("Получено документов: %zu\n", YMLArrayLen(docs));
	for (size_t i = 0; i < YMLArrayLen(docs); i++)
	{
		YMLMapForech(docs[i]->value.object, key, val)
			printf("  [%zu] %s = %s\n", i + 1, key, val->value.string);
	}
	YMLDestroyStream(docs);

	return 0;
}
