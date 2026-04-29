#pragma once

#include "YMLParser.h"

#ifndef YML_PRIVATE
#	define YML_PRIVATE
#endif


YML_PRIVATE void* _yml_alloc(size_t len, struct YMLAllocator* ctx, const char* FILE, int LINE);
YML_PRIVATE void* _yml_realloc(void* ptr, size_t new_len, struct YMLAllocator* ctx, const char* FILE, int LINE);
YML_PRIVATE void* _yml_calloc(size_t n, size_t size, struct YMLAllocator* ctx, const char* FILE, int LINE);
YML_PRIVATE void  _yml_dealloc(void* ptr, struct YMLAllocator* ctx, const char* FILE, int LINE);

#define YMLALLOC(len, alloc)            (_yml_alloc((len), (alloc), __FILE__, __LINE__))
#define YMLREALLOC(ptr, new_len, alloc) (_yml_realloc((ptr), (new_len), (alloc), __FILE__, __LINE__))
#define YMLCALLOC(n, size, alloc)       (_yml_calloc((n), (size), (alloc), __FILE__, __LINE__))
#define YMLDEALLOC(ptr, alloc)          (_yml_dealloc((ptr), (alloc), __FILE__, __LINE__))