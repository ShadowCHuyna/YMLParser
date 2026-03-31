#include "YMLParser.h"
#include <stdio.h>

int main(void) {
	YMLValue *root = YMLParse(
		"name: Alice\n"
		"age: 30\n"
		"tags: [dev, yaml]\n"
	);
	if (YMLErrorPrint() != 0) return 1;

	YMLValue *name = YMLMapGet(root->value.object, "name");
	YMLValue *age  = YMLMapGet(root->value.object, "age");
	printf("name=%s  age=%lld\n", name->value.string, (long long)age->value.integer);

	YMLValue *tags = YMLMapGet(root->value.object, "tags");
	for (size_t i = 0; i < ArrayLen(tags->value.array); i++)
		printf("tag: %s\n", tags->value.array[i].value.string);

	YMLMapForech(root->value.object, key, val)
		printf("key: %s\n", key);

	YMLDestroy(root);
	return 0;
}