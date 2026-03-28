#include <stdio.h>
#include "test.h"
#include "YMLParser.h"

int main(void)
{
	int ok = 0;
	YMLValue *root, *v;

	/* ── anchors / aliases ────────────────────────────────────────── */
	SECTION("anchor scalar");
	root = YMLParse("a: &ref 42\nb: *ref\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_INT && v->value.integer == 42, "alias = anchor value");
	YMLDestroy(root);

	SECTION("anchor object");
	root = YMLParse("base: &b\n  x: 1\n  y: 2\ncopy: *b\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "copy", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_OBJECT, "copy is object");
	YMLValue *cx = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && cx && cx->value.integer == 1, "x=1");
	YMLValue *cy = YMLMapGet(v->value.object, "y", .ok = &ok);
	CHECK(ok == 0 && cy && cy->value.integer == 2, "y=2");
	YMLDestroy(root);

	SECTION("alias is independent copy");
	root = YMLParse("a: &r\n  n: 1\nb: *r\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	YMLValue *a = YMLMapGet(root->value.object, "a", .ok = &ok);
	YMLValue *b = YMLMapGet(root->value.object, "b", .ok = &ok);
	/* два разных объекта (deep copy) */
	CHECK(a && b && a->value.object != b->value.object, "different pointers");
	YMLDestroy(root);

	/* ── merge key << ─────────────────────────────────────────────── */
	SECTION("merge key");
	root = YMLParse("defaults: &d\n  x: 1\n  y: 2\nitem:\n  <<: *d\n  z: 3\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "item", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_OBJECT, "item is object");
	YMLValue *ix = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && ix && ix->value.integer == 1, "merged x");
	YMLValue *iy = YMLMapGet(v->value.object, "y", .ok = &ok);
	CHECK(ok == 0 && iy && iy->value.integer == 2, "merged y");
	YMLValue *iz = YMLMapGet(v->value.object, "z", .ok = &ok);
	CHECK(ok == 0 && iz && iz->value.integer == 3, "own z");
	YMLDestroy(root);

	SECTION("merge does not override existing keys");
	root = YMLParse("base: &b\n  x: 1\nchild:\n  x: 99\n  <<: *b\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "child", .ok = &ok);
	YMLValue *cx2 = YMLMapGet(v->value.object, "x", .ok = &ok);
	CHECK(ok == 0 && cx2 && cx2->value.integer == 99, "child x not overridden");
	YMLDestroy(root);

	/* ── tags ─────────────────────────────────────────────────────── */
	SECTION("!!str");
	root = YMLParse("a: !!str 42\nb: !!str true\nc: !!str ~\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_STRING, "!!str 42 type");
	CHECK_CSTR(v ? v->value.string : NULL, "42", "!!str 42 value");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "true", "!!str true");
	v = YMLMapGet(root->value.object, "c", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "null", "!!str ~");
	YMLDestroy(root);

	SECTION("!!int");
	root = YMLParse("a: !!int '7'\nb: !!int 3.9\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_INT && v->value.integer == 7, "!!int '7'");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_INT && v->value.integer == 3, "!!int 3.9");
	YMLDestroy(root);

	SECTION("!!float");
	root = YMLParse("a: !!float 3\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_FLOAT, "!!float 3 type");
	YMLDestroy(root);

	SECTION("!!bool");
	root = YMLParse("a: !!bool 1\nb: !!bool 0\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_BOOL && v->value.boolean, "!!bool 1");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_BOOL && !v->value.boolean, "!!bool 0");
	YMLDestroy(root);

	SECTION("!!null");
	root = YMLParse("a: !!null whatever\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "!!null");
	YMLDestroy(root);

	/* ── multi-document stream ────────────────────────────────────── */
	SECTION("stream 2 docs");
	YMLValue **docs = YMLParseStream("---\nfoo: 1\n---\nbar: 2\n", .ok = &ok);
	CHECK(ok == 0 && docs, "parse");
	CHECK(ArrayLen(docs) == 2, "2 documents");
	v = YMLMapGet(docs[0]->value.object, "foo", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 1, "doc[0].foo");
	v = YMLMapGet(docs[1]->value.object, "bar", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 2, "doc[1].bar");
	YMLDestroyStream(docs);

	SECTION("stream without ---");
	docs = YMLParseStream("x: 1\n", .ok = &ok);
	CHECK(ok == 0 && docs && ArrayLen(docs) == 1, "single doc without ---");
	YMLDestroyStream(docs);

	TEST_REPORT();
	return _failed != 0;
}
