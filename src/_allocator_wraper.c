#include <stdio.h>
#include <stdlib.h>

#include "_allocator_wraper.h"

#include "YMLParser.h"

static void* malloc_alloc(size_t len, void* ctx, const char* FILE, int LINE) {
	(void)ctx;
	void* data = malloc(len);
	if(data==NULL) fprintf(stderr, "[ERROR] alloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void* malloc_realloc(void* ptr, size_t new_len, void* ctx, const char* FILE, int LINE) {
	(void)ctx;
	void* data = realloc(ptr, new_len);
	if(data==NULL) fprintf(stderr, "[ERROR] realloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void* malloc_calloc(size_t n, size_t size, void* ctx, const char* FILE, int LINE) {
	(void)ctx;
	void* data = calloc(n, size);
	if(data==NULL) fprintf(stderr, "[ERROR] calloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void malloc_dealloc(void* ptr, void* ctx, const char* FILE, int LINE) {
	(void)ctx; (void)FILE; (void)LINE;
	free(ptr);
}

struct _YMLParserAllocator YMLParserAllocator = {
	.alloc   = malloc_alloc,
	.realloc = malloc_realloc,
	.calloc  = malloc_calloc,
	.dealloc = malloc_dealloc,
	.ctx     = NULL
};