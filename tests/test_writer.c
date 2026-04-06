#include <stdio.h>
#include <string.h>
#include "test.h"
#include "YMLParser.h"

int main(void)
{
	int ok = 0;
	char *err = NULL;

	/* ── YMLCreate / YMLCreateArr ───────────────────────────────────── */
	SECTION("YMLCreate returns YML_OBJECT");
	YMLValue *obj = YMLCreate();
	CHECK(obj && obj->type == YML_OBJECT, "type");
	YMLDestroy(obj);

	SECTION("YMLCreateArr returns YML_ARRAY");
	YMLValue *arr = YMLCreateArr();
	CHECK(arr && arr->type == YML_ARRAY, "type");
	YMLDestroy(arr);

	/* ── YMLMapAdd scalars ──────────────────────────────────────────── */
	SECTION("YMLMapAdd int");
	obj = YMLCreate();
	YMLMapAdd(obj, "n", 42);
	YMLValue *v = YMLMapGet(obj->value.object, "n", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_INT && v->value.integer == 42, "int");
	YMLDestroy(obj);

	SECTION("YMLMapAdd double");
	obj = YMLCreate();
	YMLMapAdd(obj, "f", 3.14);
	v = YMLMapGet(obj->value.object, "f", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_FLOAT, "float type");
	YMLDestroy(obj);

	SECTION("YMLMapAdd string");
	obj = YMLCreate();
	YMLMapAdd(obj, "s", "hello");
	v = YMLMapGet(obj->value.object, "s", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_STRING, "string type");
	CHECK(strcmp(v->value.string, "hello") == 0, "string value");
	YMLDestroy(obj);

	SECTION("YMLMapAdd bool");
	obj = YMLCreate();
	YMLMapAdd(obj, "b", (bool)true);
	v = YMLMapGet(obj->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_BOOL && v->value.boolean == true, "bool");
	YMLDestroy(obj);

	SECTION("YMLMapAdd null");
	obj = YMLCreate();
	YMLMapAddNull(obj, "n");
	v = YMLMapGet(obj->value.object, "n", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "null");
	YMLDestroy(obj);

	/* ── YMLMapAdd nested node ──────────────────────────────────────── */
	SECTION("YMLMapAdd nested object (deep copy)");
	obj = YMLCreate();
	YMLValue *inner = YMLCreate();
	YMLMapAdd(inner, "x", 99);
	YMLMapAdd(obj, "sub", inner);
	YMLDestroy(inner); /* оригинал можно уничтожить — был скопирован */
	v = YMLMapGet(obj->value.object, "sub", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_OBJECT, "nested type");
	YMLValue *x = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && x && x->value.integer == 99, "nested value");
	YMLDestroy(obj);

	/* ── YMLMapAdd duplicate key cleanup ────────────────────────────── */
	SECTION("YMLMapAdd overwrite key");
	obj = YMLCreate();
	YMLMapAdd(obj, "k", "first");
	YMLMapAdd(obj, "k", "second");
	v = YMLMapGet(obj->value.object, "k", .ok = &ok);
	CHECK(ok == 0 && v && strcmp(v->value.string, "second") == 0, "overwritten");
	YMLDestroy(obj);

	/* ── YMLMapAdd C-array ──────────────────────────────────────────── */
	SECTION("YMLMapAdd int[]");
	obj = YMLCreate();
	long long nums[] = {10, 20, 30};
	YMLMapAddArr(obj, "arr", nums, 3);
	v = YMLMapGet(obj->value.object, "arr", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_ARRAY, "array type");
	CHECK(YMLArrayLen(v->value.array) == 3, "len=3");
	CHECK(v->value.array[1].value.integer == 20, "[1]=20");
	YMLDestroy(obj);

	SECTION("YMLMapAdd const char*[]");
	obj = YMLCreate();
	const char *tags[] = {"a", "b", "c"};
	YMLMapAddArr(obj, "tags", tags, 3);
	v = YMLMapGet(obj->value.object, "tags", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_ARRAY, "string array type");
	CHECK(YMLArrayLen(v->value.array) == 3, "len=3");
	CHECK(strcmp(v->value.array[0].value.string, "a") == 0, "[0]=a");
	YMLDestroy(obj);

	/* ── YMLArrPush ─────────────────────────────────────────────────── */
	SECTION("YMLArrPush scalars");
	arr = YMLCreateArr();
	YMLArrPush(arr, 1);
	YMLArrPush(arr, "two");
	YMLArrPush(arr, (bool)true);
	YMLArrPushNull(arr);
	CHECK(YMLArrayLen(arr->value.array) == 4, "len=4");
	CHECK(arr->value.array[0].value.integer == 1, "[0]=1");
	CHECK(arr->value.array[2].value.boolean == true, "[2]=true");
	CHECK(arr->value.array[3].type == YML_NULL, "[3]=null");
	YMLDestroy(arr);

	/* ── YMLWriteBuf basic ──────────────────────────────────────────── */
	SECTION("YMLWriteBuf simple object");
	obj = YMLCreate();
	YMLMapAdd(obj, "x", 1);
	char buf[256];
	int n = YMLWriteBuf(obj, buf, sizeof(buf), .ok = &ok);
	CHECK(ok == 0 && n > 0, "write ok");
	CHECK(strstr(buf, "x:") != NULL, "key present");
	CHECK(strstr(buf, "1") != NULL, "value present");
	YMLDestroy(obj);

	SECTION("YMLWriteBuf with .start=1");
	obj = YMLCreate();
	YMLMapAdd(obj, "k", "v");
	n = YMLWriteBuf(obj, buf, sizeof(buf), .ok = &ok, .start = 1);
	CHECK(ok == 0 && strncmp(buf, "---", 3) == 0, "doc start");
	YMLDestroy(obj);

	SECTION("YMLWriteBuf buffer too small");
	obj = YMLCreate();
	YMLMapAdd(obj, "key", "a very long value that won't fit");
	char tiny[4];
	n = YMLWriteBuf(obj, tiny, sizeof(tiny), .ok = &ok);
	CHECK(ok == 1 && n == -1, "overflow detected");
	YMLDestroy(obj);

	/* ── round-trip ─────────────────────────────────────────────────── */
	SECTION("round-trip write → parse");
	obj = YMLCreate();
	YMLMapAdd(obj, "name", "Alice");
	YMLMapAdd(obj, "age",  (long long)30);
	YMLMapAdd(obj, "active", (bool)true);
	char rt_buf[512];
	n = YMLWriteBuf(obj, rt_buf, sizeof(rt_buf), .ok = &ok);
	CHECK(ok == 0 && n > 0, "write ok");
	YMLDestroy(obj);

	YMLValue *root = YMLParse(rt_buf, .ok = &ok, .error = &err);
	CHECK(ok == 0 && root, "parse ok");
	YMLValue *name = YMLMapGet(root->value.object, "name", .ok = &ok);
	CHECK(ok == 0 && name && strcmp(name->value.string, "Alice") == 0, "name");
	YMLValue *age = YMLMapGet(root->value.object, "age", .ok = &ok);
	CHECK(ok == 0 && age && age->value.integer == 30, "age");
	YMLValue *active = YMLMapGet(root->value.object, "active", .ok = &ok);
	CHECK(ok == 0 && active && active->value.boolean == true, "active");
	YMLDestroy(root);

	TEST_REPORT();
	return _failed != 0;
}
