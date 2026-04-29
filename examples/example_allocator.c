/*
 * example_allocator.c — custom allocator with call-site logging.
 *
 * Each YMLValue node carries a reference to its allocator.  By passing
 * a custom allocator to YMLParse / YMLCreate every allocation (strings,
 * hash-map entries, dynamic arrays, the nodes themselves) goes through
 * your callbacks with full __FILE__:__LINE__ context from the library.
 *
 * Build:
 *   make run-example E=example_allocator
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/YMLParser.h"

/* ── logging allocator ──────────────────────────────────────────────── */

static int alloc_count;
static int free_count;

static void *log_alloc(size_t len, void *ctx, const char *file, int line)
{
	(void)ctx;
	alloc_count++;
	void *ptr = malloc(len);
	fprintf(stderr, "  alloc   %6zu bytes  → %p  (%s:%d)\n", len, ptr, file, line);
	return ptr;
}

static void *log_realloc(void *old, size_t new_len, void *ctx, const char *file, int line)
{
	(void)ctx;
	alloc_count++;
	void *ptr = realloc(old, new_len);
	fprintf(stderr, "  realloc %6zu bytes  %p → %p  (%s:%d)\n", new_len, old, ptr, file, line);
	return ptr;
}

static void *log_calloc(size_t n, size_t size, void *ctx, const char *file, int line)
{
	(void)ctx;
	alloc_count++;
	void *ptr = calloc(n, size);
	fprintf(stderr, "  calloc  %6zu bytes  → %p  (%s:%d)\n", n * size, ptr, file, line);
	return ptr;
}

static void log_free(void *ptr, void *ctx, const char *file, int line)
{
	(void)ctx;
	(void)file; (void)line;
	free_count++;
	fprintf(stderr, "  free               ← %p\n", ptr);
	free(ptr);
}

static struct YMLAllocator log_allocator = {
	.alloc   = log_alloc,
	.realloc = log_realloc,
	.calloc  = log_calloc,
	.dealloc = log_free,
	.ctx     = NULL,
};

int main(void)
{
	const char *yml =
		"server:\n"
		"  host: localhost\n"
		"  port: 8080\n"
		"tags: [api, v2]\n";

	fprintf(stderr, "\n── YMLParse with custom allocator ──────────────────\n");
	int ok;
	YMLValue *root = YMLParse(yml, .ok = &ok, .allocator = &log_allocator);
	if (ok != 0) { YMLPrintError(); return 1; }

	fprintf(stderr, "\n── read values (no allocations) ────────────────────\n");
	const char *host = YMLMapGetValue(root->value.object, "server.host", YML_STRING, .splitter = '.');
	long long   port = YMLMapGetValue(root->value.object, "server.port", YML_INT, .splitter = '.');
	printf("host=%s  port=%lld\n", host, port);

	fprintf(stderr, "\n── YMLCreate with same allocator ───────────────────\n");
	YMLValue *obj = YMLCreate(&log_allocator);
	YMLMapAdd(obj, "name", "test");
	YMLMapAdd(obj, "version", 42);
	YMLMapAdd(obj, "enabled", (bool)true);
	YMLWriteStream(obj, stdout);

	fprintf(stderr, "\n── YMLDestroy (frees everything via custom alloc) ──\n");
	YMLDestroy(root);
	YMLDestroy(obj);

	fprintf(stderr, "\n── summary: %d allocs, %d frees ──────────────────\n", alloc_count, free_count);

	return 0;
}
