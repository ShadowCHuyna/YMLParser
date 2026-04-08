/*
 * example_threads.c — парсинг YAML.
 *
 * Сборка:
 *   make run-example E=example
 */
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"
#include <stdio.h>

int main(void) {
	YMLValue *root = YMLParse(
		"name: Alice\n"
		"age: 30\n"
		"tags: [dev, yaml]\n"
	);
	if (YMLPrintError() != 0) return 1;

	char *name = YMLMapGetTyped(root->value.object, "name", YML_STRING);
	int age  = YMLMapGetTyped(root->value.object, "age", YML_INT);
	printf("name=%s  age=%lld\n", name, age);

	YMLValue *tags = YMLMapGet(root->value.object, "tags");
	for (size_t i = 0; i < YMLArrayLen(tags->value.array); i++)
		printf("tag: %s\n", tags->value.array[i].value.string);

	YMLMapForech(root->value.object, key, val)
		printf("key: %s\n", key);

	YMLDestroy(root);
	return 0;
}