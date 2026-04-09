/*
 * example_allocator.c — custom allocator with call-site logging.
 *
 * Replaces the default malloc/realloc/calloc/free with wrappers that
 * print every allocation to stderr: operation, size, pointer, and the
 * source location (__FILE__:__LINE__) inside the library where the
 * call originated.
 *
 * Build:
 *   make run-example E=example_allocator
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── logging allocator ──────────────────────────────────────────────── */

static void *log_alloc(size_t len, void *ctx, const char *file, int line)
{
	(void)ctx;
	void *ptr = malloc(len);
	fprintf(stderr, "  alloc   %6zu bytes  → %p  (%s:%d)\n", len, ptr, file, line);
	return ptr;
}

static void *log_realloc(void *old, size_t new_len, void *ctx, const char *file, int line)
{
	(void)ctx;
	void *ptr = realloc(old, new_len);
	fprintf(stderr, "  realloc %6zu bytes  %p → %p  (%s:%d)\n", new_len, old, ptr, file, line);
	return ptr;
}

static void *log_calloc(size_t n, size_t size, void *ctx, const char *file, int line)
{
	(void)ctx;
	void *ptr = calloc(n, size);
	fprintf(stderr, "  calloc  %6zu bytes  → %p  (%s:%d)\n", n * size, ptr, file, line);
	return ptr;
}

static void log_free(void *ptr, void *ctx, const char *file, int line)
{
	(void)ctx;
	fprintf(stderr, "  free               ← %p  (%s:%d)\n", ptr, file, line);
	free(ptr);
}

/* ── set allocator before YMLPARSER_IMPLEMENTATION ─────────────────── */

#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

int main(void)
{
	YMLParserSetAllocator((struct YMLParserAllocator){
		.alloc   = log_alloc,
		.realloc = log_realloc,
		.calloc  = log_calloc,
		.dealloc = log_free,
		.ctx     = NULL,
	});

	const char *yml =
		"server:\n"
		"  host: localhost\n"
		"  port: 8080\n"
		"tags: [api, v2]\n";

	fprintf(stderr, "\n── YMLParse ────────────────────────────────────────\n");
	YMLValue *root = YMLParse(yml);
	if (YMLPrintError() != 0) return 1;

	fprintf(stderr, "\n── read values ─────────────────────────────────────\n");
	const char *host = YMLMapGetValue(root->value.object, "server.host", YML_STRING, .splitter = '.');
	long long   port = YMLMapGetValue(root->value.object, "server.port", YML_INT, .splitter = '.');
	printf("host=%s  port=%lld\n", host, port);

	fprintf(stderr, "\n── YMLDestroy ──────────────────────────────────────\n");
	YMLDestroy(root);

	return 0;
}
