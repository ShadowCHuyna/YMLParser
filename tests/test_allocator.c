#include "YMLParser.h"
#include "_da.h"
#include "_hm.h"

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── tracking allocator ────────────────────────────────────────────── */

typedef struct
{
	int allocs;
	int reallocs;
	int callocs;
	int deallocs;
	size_t total_bytes;
} Tracker;

static void *track_alloc(size_t len, void *ctx, const char *file, int line)
{
	(void)file; (void)line;
	Tracker *t = (Tracker *)ctx;
	t->allocs++;
	t->total_bytes += len;
	return malloc(len);
}

static void *track_realloc(void *ptr, size_t new_len, void *ctx, const char *file, int line)
{
	(void)file; (void)line;
	Tracker *t = (Tracker *)ctx;
	t->reallocs++;
	t->total_bytes += new_len;
	return realloc(ptr, new_len);
}

static void *track_calloc(size_t n, size_t size, void *ctx, const char *file, int line)
{
	(void)file; (void)line;
	Tracker *t = (Tracker *)ctx;
	t->callocs++;
	t->total_bytes += n * size;
	return calloc(n, size);
}

static void track_dealloc(void *ptr, void *ctx, const char *file, int line)
{
	(void)file; (void)line;
	Tracker *t = (Tracker *)ctx;
	t->deallocs++;
	free(ptr);
}

static struct YMLAllocator make_tracker(Tracker *ctx)
{
	struct YMLAllocator a = {
		.alloc   = track_alloc,
		.realloc = track_realloc,
		.calloc  = track_calloc,
		.dealloc = track_dealloc,
		.ctx     = ctx,
	};
	return a;
}

/* ── helper: walk tree and verify all nodes share the same allocator ── */

static void check_allocator(YMLValue *node, struct YMLAllocator *expected, int *ok)
{
	if (!node) return;
	if (node->allocator != expected) { *ok = 0; return; }
	if (node->type == YML_ARRAY) {
		size_t n = da_len(node->value.array);
		for (size_t i = 0; i < n; i++)
			check_allocator(&node->value.array[i], expected, ok);
	} else if (node->type == YML_OBJECT) {
		size_t idx = 0;
		const char *key;
		YMLValue *val;
		while (hm_next((_hm *)node->value.object, &idx, &key, &val))
			check_allocator(val, expected, ok);
	}
}

int main(void)
{
	Tracker t1 = {0};
	struct YMLAllocator a1 = make_tracker(&t1);

	Tracker t2 = {0};
	struct YMLAllocator a2 = make_tracker(&t2);

	/* ── YMLParse with custom allocator ─────────────────────────────── */
	SECTION("YMLParse uses custom allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue *root = YMLParse("a: 1\nb: hello\n", .ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse ok");
		CHECK(root != NULL, "root not null");
		CHECK(t.allocs > 0, "at least one alloc");
		YMLDestroy(root);
		CHECK(t.deallocs == t.allocs + t.reallocs + t.callocs, "all freed");
	}

	/* ── allocator inheritance ──────────────────────────────────────── */
	SECTION("allocator inherited by all nodes");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue *root = YMLParse(
			"server:\n"
			"  host: localhost\n"
			"  port: 8080\n"
			"tags:\n"
			"  - api\n"
			"  - v2\n",
			.ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse ok");

		int alloc_ok = 1;
		check_allocator(root, &a, &alloc_ok);
		CHECK(alloc_ok, "all nodes share same allocator");

		YMLDestroy(root);
	}

	/* ── YMLCreate with allocator ───────────────────────────────────── */
	SECTION("YMLCreate inherits allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		YMLValue *obj = YMLCreate(&a);
		CHECK(obj != NULL, "create ok");
		CHECK(obj->type == YML_OBJECT, "is object");
		CHECK(obj->allocator == &a, "allocator set on object");

		YMLMapAdd(obj, "name", "test");
		YMLMapAdd(obj, "num", 42);

		/* check that children also have the allocator */
		int alloc_ok = 1;
		check_allocator(obj, &a, &alloc_ok);
		CHECK(alloc_ok, "children inherit allocator");

		YMLDestroy(obj);
	}

	/* ── YMLCreateArr with allocator ────────────────────────────────── */
	SECTION("YMLCreateArr inherits allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		YMLValue *arr = YMLCreateArr(&a);
		CHECK(arr != NULL, "create ok");
		CHECK(arr->type == YML_ARRAY, "is array");
		CHECK(arr->allocator == &a, "allocator set on array");

		YMLArrPush(arr, 1);
		YMLArrPush(arr, "two");

		int alloc_ok = 1;
		check_allocator(arr, &a, &alloc_ok);
		CHECK(alloc_ok, "children inherit allocator");

		YMLDestroy(arr);
	}

	/* ── deep copy inherits destination allocator ───────────────────── */
	SECTION("YMLMapAdd deep-copies with destination allocator");
	{
		Tracker t1 = {0}, t2 = {0};
		struct YMLAllocator a1 = make_tracker(&t1);
		struct YMLAllocator a2 = make_tracker(&t2);

		/* source node with a1 */
		YMLValue *src = YMLCreate(&a1);
		YMLMapAdd(src, "key", "val");

		/* dest node with a2 */
		YMLValue *dst = YMLCreate(&a2);
		YMLMapAdd(dst, "src", src);  /* deep copy */

		CHECK(dst->allocator == &a2, "dst has a2");

		/* the copied child inside dst should have a2 */
		int ok;
		YMLValue *copied = YMLMapGet(dst->value.object, "src", .ok = &ok);
		CHECK(ok == 0 && copied, "got src");
		CHECK(copied->allocator == &a2, "copied node has dst allocator");

		YMLDestroy(src);
		YMLDestroy(dst);
	}

	/* ── YMLArrPush deep-copies with array allocator ────────────────── */
	SECTION("YMLArrPush deep-copies with array allocator");
	{
		Tracker t1 = {0}, t2 = {0};
		struct YMLAllocator a1 = make_tracker(&t1);
		struct YMLAllocator a2 = make_tracker(&t2);

		YMLValue *src = YMLCreate(&a1);
		YMLMapAdd(src, "x", 10);

		YMLValue *arr = YMLCreateArr(&a2);
		YMLArrPush(arr, src);

		int ok;
		YMLValue *item = &arr->value.array[0];
		CHECK(item->allocator == &a2, "pushed node has array allocator");

		YMLDestroy(src);
		YMLDestroy(arr);
	}

	/* ── two trees, independent allocators ──────────────────────────── */
	SECTION("two trees with different allocators");
	{
		Tracker t1 = {0}, t2 = {0};
		struct YMLAllocator a1 = make_tracker(&t1);
		struct YMLAllocator a2 = make_tracker(&t2);

		int ok;
		YMLValue *root1 = YMLParse("a: 1\n", .ok = &ok, .allocator = &a1);
		YMLValue *root2 = YMLParse("b: 2\n", .ok = &ok, .allocator = &a2);

		CHECK(root1->allocator == &a1, "root1 has a1");
		CHECK(root2->allocator == &a2, "root2 has a2");

		int ok1 = 1, ok2 = 1;
		check_allocator(root1, &a1, &ok1);
		check_allocator(root2, &a2, &ok2);
		CHECK(ok1, "tree1 all a1");
		CHECK(ok2, "tree2 all a2");

		YMLDestroy(root1);
		CHECK(t1.deallocs > 0, "t1 freed");

		YMLDestroy(root2);
		CHECK(t2.deallocs > 0, "t2 freed");

		CHECK(t1.allocs > 0 && t2.allocs > 0, "both had allocs");
	}

	/* ── NULL allocator fallback ────────────────────────────────────── */
	SECTION("NULL allocator works (malloc fallback)");
	{
		int ok;
		YMLValue *root = YMLParse("x: 123\n", .ok = &ok, .allocator = NULL);
		CHECK(ok == 0, "parse ok");
		CHECK(root != NULL, "root not null");
		CHECK(root->allocator == NULL, "allocator is NULL");

		YMLValue *v = YMLMapGet(root->value.object, "x", .ok = &ok);
		CHECK(ok == 0 && v && v->type == YML_INT, "got value");

		YMLDestroy(root);
	}

	/* ── YMLParseStream with allocator ──────────────────────────────── */
	SECTION("YMLParseStream uses allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue **docs = YMLParseStream(
			"---\na: 1\n---\nb: 2\n",
			.ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse stream ok");
		CHECK(YMLArrayLen(docs) == 2, "2 docs");
		CHECK(docs[0]->allocator == &a, "doc1 allocator");
		CHECK(docs[1]->allocator == &a, "doc2 allocator");

		YMLDestroyStream(docs);
		CHECK(t.deallocs > 0, "stream freed");
	}

	/* ── block sequence allocator ───────────────────────────────────── */
	SECTION("block sequence elements inherit allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue *root = YMLParse("- 1\n- 2\n- 3\n", .ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse ok");
		CHECK(YMLArrayLen(root->value.array) == 3, "len 3");

		int alloc_ok = 1;
		check_allocator(root, &a, &alloc_ok);
		CHECK(alloc_ok, "array elements inherit allocator");

		YMLDestroy(root);
	}

	/* ── flow mapping allocator ─────────────────────────────────────── */
	SECTION("flow mapping inherits allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue *root = YMLParse("{k: v, n: 42}\n", .ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse ok");

		int alloc_ok = 1;
		check_allocator(root, &a, &alloc_ok);
		CHECK(alloc_ok, "flow mapping nodes inherit allocator");

		YMLDestroy(root);
	}

	/* ── nested collections allocator ───────────────────────────────── */
	SECTION("deeply nested collections inherit allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		YMLValue *obj = YMLCreate(&a);

		YMLValue *level1 = YMLCreate(&a);
		YMLValue *level2 = YMLCreate(&a);
		YMLMapAdd(level2, "deep", true);
		YMLMapAdd(level1, "level2", level2);
		YMLMapAdd(obj, "level1", level1);

		int alloc_ok = 1;
		check_allocator(obj, &a, &alloc_ok);
		CHECK(alloc_ok, "deeply nested nodes inherit allocator");

		YMLDestroy(obj);
	}

	/* ── anchor/alias allocator ─────────────────────────────────────── */
	SECTION("anchor/alias nodes inherit allocator");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		int ok;
		YMLValue *root = YMLParse(
			"base: &base\n"
			"  x: 1\n"
			"ref: *base\n",
			.ok = &ok, .allocator = &a);
		CHECK(ok == 0, "parse ok");

		int alloc_ok = 1;
		check_allocator(root, &a, &alloc_ok);
		CHECK(alloc_ok, "anchor/alias nodes inherit allocator");

		YMLDestroy(root);
	}

	/* ── writer round-trip allocator ────────────────────────────────── */
	SECTION("write → parse preserves allocator separation");
	{
		Tracker t = {0};
		struct YMLAllocator a = make_tracker(&t);
		YMLValue *obj = YMLCreate(&a);
		YMLMapAdd(obj, "name", "Alice");
		YMLMapAdd(obj, "age", 30);

		char buf[256];
		int n = YMLWriteBuf(obj, buf, sizeof(buf));
		CHECK(n > 0, "write ok");

		int ok;
		YMLValue *parsed = YMLParse(buf, .ok = &ok, .allocator = &a);
		CHECK(ok == 0, "re-parse ok");
		CHECK(parsed->allocator == &a, "parsed allocator");

		YMLDestroy(obj);
		YMLDestroy(parsed);
	}

	TEST_REPORT();
}
