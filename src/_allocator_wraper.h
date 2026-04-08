#pragma once

#include "YMLParser.h"

#define YMLALLOC(len)           (YMLParserAllocator.alloc((len), YMLParserAllocator.ctx, __FILE__, __LINE__))
#define YMLREALLOC(ptr, new_len)(YMLParserAllocator.realloc((ptr), (new_len), YMLParserAllocator.ctx, __FILE__, __LINE__))
#define YMLCALLOC(n, size)      (YMLParserAllocator.calloc((n), (size), YMLParserAllocator.ctx, __FILE__, __LINE__))
#define YMLDEALLOC(ptr)         (YMLParserAllocator.dealloc((ptr), YMLParserAllocator.ctx, __FILE__, __LINE__))