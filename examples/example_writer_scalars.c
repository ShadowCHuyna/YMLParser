/*
 * example_writer_scalars.c — построение объекта из скалярных значений
 *                            и сериализация в YAML.
 *
 * Сборка:
 *   make run-example E=example_writer_scalars
 */

#include <stdio.h>
#include <string.h>
#include "YMLParser.h"

int main(void)
{
	/* ── построить объект ──────────────────────────────────────────── */
	YMLValue *obj = YMLCreate();

	YMLMapAdd(obj, "name",    "Alice");
	YMLMapAdd(obj, "age",     (long long)30);
	YMLMapAdd(obj, "score",   98.6);
	YMLMapAdd(obj, "active",  (bool)true);
	YMLMapAddNull(obj, "token");

	/* вложенный объект — deep-copied, оригинал можно уничтожить */
	YMLValue *addr = YMLCreate();
	YMLMapAdd(addr, "city",   "Moscow");
	YMLMapAdd(addr, "zip",    "101000");
	YMLMapAdd(obj, "address", addr);
	YMLDestroy(addr);

	/* ── вывод в stdout ────────────────────────────────────────────── */
	printf("=== YMLWrite → stdout ===\n");
	YMLWriteStream(obj, stdout);

	/* ── вывод в буфер ─────────────────────────────────────────────── */
	char buf[512];
	int ok = 0;
	int n  = YMLWriteBuf(obj, buf, sizeof(buf), .ok = &ok);
	printf("\n=== YMLWriteBuf (%d bytes) ===\n%s\n", n, buf);

	YMLDestroy(obj);

	/* ── round-trip: YMLWriteBuf → YMLParse ───────────────────────── */
	printf("=== round-trip parse ===\n");
	YMLValue *root = YMLParse(buf, .ok = &ok);
	if (ok != 0) { fprintf(stderr, "parse error\n"); return 1; }

	YMLValue *name   = YMLMapGet(root->value.object, "name",   .ok = &ok);
	YMLValue *age    = YMLMapGet(root->value.object, "age",    .ok = &ok);
	YMLValue *active = YMLMapGet(root->value.object, "active", .ok = &ok);
	YMLValue *token  = YMLMapGet(root->value.object, "token",  .ok = &ok);

	printf("name   = %s\n",  name->value.string);
	printf("age    = %lld\n", (long long)age->value.integer);
	printf("active = %s\n",  active->value.boolean ? "true" : "false");
	printf("token  = %s\n",  token->type == YML_NULL ? "null" : "?");

	YMLDestroy(root);
	return 0;
}
