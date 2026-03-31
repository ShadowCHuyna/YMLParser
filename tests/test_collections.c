#include <stdio.h>
#include "test.h"
#include "YMLParser.h"

int main(void)
{
	int ok = 0;
	YMLValue *root, *v;

	/* ── block mapping ────────────────────────────────────────────── */
	SECTION("block mapping simple");
	root = YMLParse("name: Alice\nage: 30\n", .ok = &ok);
	CHECK(ok == 0 && root && root->type == YML_OBJECT, "parse");
	v = YMLMapGet(root->value.object, "name", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "Alice", "string value");
	v = YMLMapGet(root->value.object, "age", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 30, "int value");
	YMLDestroy(root);

	SECTION("block mapping nested");
	root = YMLParse("outer:\n  inner: 42\n  text: hi\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "outer", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_OBJECT, "outer is object");
	YMLValue *inner = YMLMapGet(v->value.object, "inner", .ok = &ok);
	CHECK(ok == 0 && inner && inner->value.integer == 42, "inner value");
	YMLValue *text = YMLMapGet(v->value.object, "text", .ok = &ok);
	CHECK_CSTR(text ? text->value.string : NULL, "hi", "text value");
	YMLDestroy(root);

	SECTION("block mapping null value");
	root = YMLParse("a: ~\nb:\nc: 1\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "empty value = null");
	YMLDestroy(root);

	/* ── block sequence ───────────────────────────────────────────── */
	SECTION("block sequence at root");
	root = YMLParse("- 1\n- 2\n- 3\n", .ok = &ok);
	CHECK(ok == 0 && root && root->type == YML_ARRAY, "parse");
	CHECK(YMLArrayLen(root->value.array) == 3, "length 3");
	CHECK(root->value.array[0].value.integer == 1, "[0]");
	CHECK(root->value.array[1].value.integer == 2, "[1]");
	CHECK(root->value.array[2].value.integer == 3, "[2]");
	YMLDestroy(root);

	SECTION("block sequence as mapping value");
	root = YMLParse("nums:\n  - 10\n  - 20\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "nums", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_ARRAY, "array type");
	CHECK(YMLArrayLen(v->value.array) == 2, "length 2");
	CHECK(v->value.array[0].value.integer == 10, "[0]=10");
	CHECK(v->value.array[1].value.integer == 20, "[1]=20");
	YMLDestroy(root);

	SECTION("block sequence of mappings");
	root = YMLParse("- name: Alice\n  age: 30\n- name: Bob\n  age: 25\n", .ok = &ok);
	CHECK(ok == 0 && root && root->type == YML_ARRAY, "parse");
	CHECK(YMLArrayLen(root->value.array) == 2, "2 items");
	YMLValue *alice = YMLMapGet(root->value.array[0].value.object, "name", .ok = &ok);
	CHECK_CSTR(alice ? alice->value.string : NULL, "Alice", "Alice");
	YMLValue *bob_age = YMLMapGet(root->value.array[1].value.object, "age", .ok = &ok);
	CHECK(ok == 0 && bob_age && bob_age->value.integer == 25, "Bob age");
	YMLDestroy(root);

	SECTION("block sequence mixed types");
	root = YMLParse("- 1\n- hello\n- true\n- ~\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	CHECK(root->value.array[0].type == YML_INT, "[0] int");
	CHECK(root->value.array[1].type == YML_STRING, "[1] string");
	CHECK(root->value.array[2].type == YML_BOOL, "[2] bool");
	CHECK(root->value.array[3].type == YML_NULL, "[3] null");
	YMLDestroy(root);

	/* ── flow mapping ─────────────────────────────────────────────── */
	SECTION("flow mapping");
	root = YMLParse("a: {x: 1, y: 2}\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_OBJECT, "type");
	YMLValue *x = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && x && x->value.integer == 1, "x=1");
	YMLValue *y = YMLMapGet(v->value.object, "y", .ok = &ok);
	CHECK(ok == 0 && y && y->value.integer == 2, "y=2");
	YMLDestroy(root);

	SECTION("flow mapping at root");
	root = YMLParse("{foo: bar, n: 42}\n", .ok = &ok);
	CHECK(ok == 0 && root && root->type == YML_OBJECT, "parse");
	v = YMLMapGet(root->value.object, "foo", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "bar", "foo=bar");
	v = YMLMapGet(root->value.object, "n", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 42, "n=42");
	YMLDestroy(root);

	/* ── flow sequence ────────────────────────────────────────────── */
	SECTION("flow sequence");
	root = YMLParse("a: [1, 2, 3]\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_ARRAY, "type");
	CHECK(YMLArrayLen(v->value.array) == 3, "length 3");
	CHECK(v->value.array[0].value.integer == 1, "[0]");
	CHECK(v->value.array[2].value.integer == 3, "[2]");
	YMLDestroy(root);

	SECTION("flow sequence at root");
	root = YMLParse("[true, 42, hello]\n", .ok = &ok);
	CHECK(ok == 0 && root && root->type == YML_ARRAY, "parse");
	CHECK(YMLArrayLen(root->value.array) == 3, "length 3");
	CHECK(root->value.array[0].type == YML_BOOL, "[0] bool");
	CHECK(root->value.array[1].type == YML_INT, "[1] int");
	CHECK(root->value.array[2].type == YML_STRING, "[2] string");
	YMLDestroy(root);

	SECTION("nested flow");
	root = YMLParse("a: {x: [1, 2], y: {z: 3}}\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	x = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && x && x->type == YML_ARRAY && YMLArrayLen(x->value.array) == 2, "nested array");
	y = YMLMapGet(v->value.object, "y", .ok = &ok);
	CHECK(ok == 0 && y && y->type == YML_OBJECT, "nested object");
	YMLValue *z = YMLMapGet(y->value.object, "z", .ok = &ok);
	CHECK(ok == 0 && z && z->value.integer == 3, "z=3");
	YMLDestroy(root);

	TEST_REPORT();
	return _failed != 0;
}
