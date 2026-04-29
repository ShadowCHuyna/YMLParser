#pragma once
/*
 * _yml_utils.h — рекурсивное освобождение содержимого YMLValue + утилиты.
 *
 * Единственный источник истины для логики обхода типов при free.
 * Включается в _hm.c и YMLParser.c.
 *
 * hm_free объявлена в _hm.h и определена в _hm.c; её вызов из inline-функции
 * разрешается на этапе компоновки.
 */

#include "YMLParser.h"
#include "_allocator.h"
#include "_hm.h"
#include "_da.h"
#include <stdlib.h>
#include <string.h>

/*
 * yml_strdup — портативная замена strdup (не требует POSIX / _POSIX_C_SOURCE).
 */
static inline char *yml_strdup(const char *s, struct YMLAllocator* alloc)
{
	size_t n = strlen(s) + 1;
	char *copy = (char *)YMLALLOC(n, alloc);
	if (copy)
		memcpy(copy, s, n);
	return copy;
}

/*
 * Рекурсивная глубокая копия YMLValue.
 * Возвращает heap-аллоцированный YMLValue* или NULL при OOM.
 * Реализация в _yml_utils.c.
 */
YML_PRIVATE YMLValue *yml_deep_copy(const YMLValue *src);

static inline void yml_value_free_impl(YMLValue *v)
{
	if (!v)
		return;
	switch (v->type)
	{
	case YML_STRING:
		YMLDEALLOC((char *)v->value.string, v->allocator);
		break;
	case YML_ARRAY:
	{
		size_t n = da_len(v->value.array);
		for (size_t i = 0; i < n; i++)
			yml_value_free_impl(&v->value.array[i]);
		da_free(v->value.array);
		break;
	}
	case YML_OBJECT:
		hm_free((_hm *)v->value.object);
		break;
	default:
		break;
	}
}
