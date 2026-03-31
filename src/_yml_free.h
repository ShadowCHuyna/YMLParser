#pragma once
/*
 * _yml_free.h — рекурсивное освобождение содержимого YMLValue.
 *
 * Единственный источник истины для логики обхода типов при free.
 * Включается в _hm.c и YMLParser.c; оба файла вызывают yml_value_free_impl.
 *
 * hm_free объявлена в _hm.h и определена в _hm.c; её вызов из inline-функции
 * разрешается на этапе компоновки.
 */

#include "YMLParser.h"
#include "_hm.h"
#include "_da.h"

static inline void yml_value_free_impl(YMLValue *v)
{
	if (!v)
		return;
	switch (v->type)
	{
	case YML_STRING:
		free((char *)v->value.string);
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
