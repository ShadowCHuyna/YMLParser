#include <stdio.h>
#include <math.h>
#include "test.h"
#include "YMLParser.h"

int main(void)
{
	int ok = 0;
	YMLValue *root, *v;

	/* ── NULL ─────────────────────────────────────────────────────── */
	SECTION("null");
	root = YMLParse("a: ~\nb: null\nc: Null\nd: NULL\ne:\n", .ok = &ok);
	CHECK(ok == 0 && root, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "~");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "null");
	v = YMLMapGet(root->value.object, "c", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "Null");
	v = YMLMapGet(root->value.object, "d", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "NULL");
	v = YMLMapGet(root->value.object, "e", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_NULL, "empty value");
	YMLDestroy(root);

	/* ── BOOL ─────────────────────────────────────────────────────── */
	SECTION("bool");
	root = YMLParse("a: true\nb: True\nc: TRUE\nd: false\ne: False\nf: FALSE\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 1, "true");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 1, "True");
	v = YMLMapGet(root->value.object, "c", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 1, "TRUE");
	v = YMLMapGet(root->value.object, "d", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 0, "false");
	v = YMLMapGet(root->value.object, "e", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 0, "False");
	v = YMLMapGet(root->value.object, "f", .ok = &ok);
	CHECK(ok == 0 && v && v->value.boolean == 0, "FALSE");
	YMLDestroy(root);

	/* ── INT ──────────────────────────────────────────────────────── */
	SECTION("int");
	root = YMLParse("dec: 42\nneg: -5\npos: +100\nhex: 0xFF\noct: 0o17\nzero: 0\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "dec", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_INT && v->value.integer == 42, "42");
	v = YMLMapGet(root->value.object, "neg", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == -5, "-5");
	v = YMLMapGet(root->value.object, "pos", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 100, "+100");
	v = YMLMapGet(root->value.object, "hex", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 255, "0xFF");
	v = YMLMapGet(root->value.object, "oct", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 15, "0o17");
	v = YMLMapGet(root->value.object, "zero", .ok = &ok);
	CHECK(ok == 0 && v && v->value.integer == 0, "0");
	YMLDestroy(root);

	/* ── FLOAT ────────────────────────────────────────────────────── */
	SECTION("float");
	root = YMLParse("f: 3.14\nneg: -0.5\nsci: 1.5e2\n"
					"inf: .inf\nninf: -.inf\npinf: +.inf\n"
					"nan: .nan\nnan2: .NaN\nnan3: .NAN\n",
					.ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "f", .ok = &ok);
	CHECK(ok == 0 && v && v->value.number == 3.14, "3.14");
	v = YMLMapGet(root->value.object, "neg", .ok = &ok);
	CHECK(ok == 0 && v && v->value.number == -0.5, "-0.5");
	v = YMLMapGet(root->value.object, "sci", .ok = &ok);
	CHECK(ok == 0 && v && v->value.number == 150.0, "1.5e2");
	v = YMLMapGet(root->value.object, "inf", .ok = &ok);
	CHECK(ok == 0 && v && isinf(v->value.number) && v->value.number > 0, ".inf");
	v = YMLMapGet(root->value.object, "ninf", .ok = &ok);
	CHECK(ok == 0 && v && isinf(v->value.number) && v->value.number < 0, "-.inf");
	v = YMLMapGet(root->value.object, "pinf", .ok = &ok);
	CHECK(ok == 0 && v && isinf(v->value.number) && v->value.number > 0, "+.inf");
	v = YMLMapGet(root->value.object, "nan", .ok = &ok);
	CHECK(ok == 0 && v && isnan(v->value.number), ".nan");
	v = YMLMapGet(root->value.object, "nan2", .ok = &ok);
	CHECK(ok == 0 && v && isnan(v->value.number), ".NaN");
	v = YMLMapGet(root->value.object, "nan3", .ok = &ok);
	CHECK(ok == 0 && v && isnan(v->value.number), ".NAN");
	YMLDestroy(root);

	/* ── STRING plain ─────────────────────────────────────────────── */
	SECTION("string plain");
	root = YMLParse("a: hello\nb: foo bar\nc: 42abc\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "hello", "simple");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "foo bar", "with space");
	v = YMLMapGet(root->value.object, "c", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "42abc", "starts with digit");
	YMLDestroy(root);

	/* ── STRING single-quoted ─────────────────────────────────────── */
	SECTION("string single-quoted");
	root = YMLParse("a: 'hello world'\nb: 'it''s fine'\nc: '42'\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "hello world", "plain");
	v = YMLMapGet(root->value.object, "b", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "it's fine", "'' escape");
	v = YMLMapGet(root->value.object, "c", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_STRING, "force string type");
	CHECK_CSTR(v ? v->value.string : NULL, "42", "'42' stays string");
	YMLDestroy(root);

	/* ── STRING double-quoted escapes ─────────────────────────────── */
	SECTION("string double-quoted");
	root = YMLParse("nl: \"line1\\nline2\"\n"
					"tab: \"a\\tb\"\n"
					"bs: \"back\\\\slash\"\n"
					"qt: \"say \\\"hi\\\"\"\n",
					.ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "nl", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "line1\nline2", "\\n");
	v = YMLMapGet(root->value.object, "tab", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "a\tb", "\\t");
	v = YMLMapGet(root->value.object, "bs", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "back\\slash", "\\\\");
	v = YMLMapGet(root->value.object, "qt", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "say \"hi\"", "\\\"");
	YMLDestroy(root);

	/* ── block literal | ──────────────────────────────────────────── */
	SECTION("block literal");
	/* "line1\nline2\n" — clip: один \n в конце */
	root = YMLParse("a: |\n  line1\n  line2\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_STRING, "type");
	CHECK_CSTR(v ? v->value.string : NULL, "line1\nline2\n", "clip");
	YMLDestroy(root);

	/* strip |- */
	root = YMLParse("a: |-\n  line1\n  line2\n", .ok = &ok);
	CHECK(ok == 0, "parse |-");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "line1\nline2", "strip");
	YMLDestroy(root);

	/* ── block folded > ───────────────────────────────────────────── */
	SECTION("block folded");
	/* одиночный \n между строками → пробел */
	root = YMLParse("a: >\n  line1\n  line2\n", .ok = &ok);
	CHECK(ok == 0, "parse");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK(ok == 0 && v && v->type == YML_STRING, "type");
	CHECK_CSTR(v ? v->value.string : NULL, "line1 line2\n", "fold");
	YMLDestroy(root);

	/* strip >- */
	root = YMLParse("a: >-\n  line1\n  line2\n", .ok = &ok);
	CHECK(ok == 0, "parse >-");
	v = YMLMapGet(root->value.object, "a", .ok = &ok);
	CHECK_CSTR(v ? v->value.string : NULL, "line1 line2", "fold strip");
	YMLDestroy(root);

	TEST_REPORT();
	return _failed != 0;
}
