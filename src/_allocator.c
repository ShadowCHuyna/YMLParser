#include <stdio.h>
#include <stdlib.h>

#include "_allocator.h"

#include "YMLParser.h"

static void* malloc_alloc(size_t len, const char* FILE, int LINE) {
	void* data = malloc(len);
	if(data==NULL) fprintf(stderr, "[ERROR] alloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void* malloc_realloc(void* ptr, size_t new_len, const char* FILE, int LINE) {
	void* data = realloc(ptr, new_len);
	if(data==NULL) fprintf(stderr, "[ERROR] realloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void* malloc_calloc(size_t n, size_t size, const char* FILE, int LINE) {
	void* data = calloc(n, size);
	if(data==NULL) fprintf(stderr, "[ERROR] calloc failed file: %s:%d\n", FILE, LINE);
	return data;
}

static void malloc_dealloc(void* ptr, const char* FILE, int LINE) {
	free(ptr);
}


YML_PRIVATE void* _yml_alloc(size_t len, struct YMLAllocator* ctx, const char* FILE, int LINE){
	if (ctx && ctx->alloc) return ctx->alloc(len, ctx->ctx, FILE, LINE);
	return malloc_alloc(len, FILE, LINE);
}

YML_PRIVATE void* _yml_realloc(void* ptr, size_t new_len, struct YMLAllocator* ctx, const char* FILE, int LINE){
	if (ctx && ctx->realloc) return ctx->realloc(ptr, new_len, ctx->ctx, FILE, LINE);
	return malloc_realloc(ptr, new_len, FILE, LINE);
}

YML_PRIVATE void* _yml_calloc(size_t n, size_t size, struct YMLAllocator* ctx, const char* FILE, int LINE){
	if (ctx && ctx->calloc) return ctx->calloc(n, size, ctx->ctx, FILE, LINE);
	return malloc_calloc(n, size, FILE, LINE);
}

YML_PRIVATE void  _yml_dealloc(void* ptr, struct YMLAllocator* ctx, const char* FILE, int LINE){
	if (ctx && ctx->dealloc) return ctx->dealloc(ptr, ctx->ctx, FILE, LINE);
	return malloc_dealloc(ptr, FILE, LINE);
}