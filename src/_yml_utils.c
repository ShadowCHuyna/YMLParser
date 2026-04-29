#include "_yml_utils.h"
#include "_da.h"
#include "_hm.h"

YML_PRIVATE YMLValue *yml_deep_copy(const YMLValue *src, struct YMLAllocator *target_alloc)
{
	if (!src)
		return NULL;
	struct YMLAllocator* alloc = target_alloc ? target_alloc : src->allocator;
	YMLValue *v = YMLALLOC(sizeof(YMLValue), alloc);
	if (!v)
		return NULL;
	v->type = src->type;
	v->allocator = alloc;
	switch (src->type)
	{
	case YML_NULL:
	case YML_BOOL:
	case YML_INT:
	case YML_FLOAT:
		v->value = src->value;
		break;
	case YML_STRING:
		v->value.string = src->value.string ? yml_strdup(src->value.string, alloc) : NULL;
		break;
	case YML_ARRAY:
	{
		size_t n = da_len(src->value.array);
		YMLValue *arr = da_new(YMLValue, n > 0 ? n : 1, alloc);
		for (size_t i = 0; i < n; i++)
		{
			YMLValue *cp = yml_deep_copy(&src->value.array[i], alloc);
			if (cp)
			{
				da_push(arr, *cp);
				YMLDEALLOC(cp, alloc);
			}
		}
		v->value.array = arr;
		break;
	}
	case YML_OBJECT:
	{
		_hm *src_hm = (_hm *)src->value.object;
		_hm *dst_hm = hm_new(src_hm->cap, alloc);
		size_t idx = 0;
		const char *key;
		YMLValue *val;
		while (hm_next(src_hm, &idx, &key, &val))
		{
			YMLValue *cp = yml_deep_copy(val, alloc);
			if (cp)
			{
				hm_set(dst_hm, key, *cp);
				YMLDEALLOC(cp, alloc);
			}
		}
		v->value.object = dst_hm;
		break;
	}
	default:
		break;
	}
	return v;
}
