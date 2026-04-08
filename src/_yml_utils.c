#include "_yml_utils.h"
#include "_da.h"
#include "_hm.h"

YML_PRIVATE YMLValue *yml_deep_copy(const YMLValue *src)
{
	if (!src)
		return NULL;
	YMLValue *v = YMLALLOC(sizeof(YMLValue));
	if (!v)
		return NULL;
	v->type = src->type;
	switch (src->type)
	{
	case YML_NULL:
	case YML_BOOL:
	case YML_INT:
	case YML_FLOAT:
		v->value = src->value;
		break;
	case YML_STRING:
		v->value.string = src->value.string ? yml_strdup(src->value.string) : NULL;
		break;
	case YML_ARRAY:
	{
		size_t n = da_len(src->value.array);
		YMLValue *arr = da_new(YMLValue, n > 0 ? n : 1);
		for (size_t i = 0; i < n; i++)
		{
			YMLValue *cp = yml_deep_copy(&src->value.array[i]);
			if (cp)
			{
				da_push(arr, *cp);
				YMLDEALLOC(cp);
			}
		}
		v->value.array = arr;
		break;
	}
	case YML_OBJECT:
	{
		_hm *src_hm = (_hm *)src->value.object;
		_hm *dst_hm = hm_new(src_hm->cap);
		size_t idx = 0;
		const char *key;
		YMLValue *val;
		while (hm_next(src_hm, &idx, &key, &val))
		{
			YMLValue *cp = yml_deep_copy(val);
			if (cp)
			{
				hm_set(dst_hm, key, *cp);
				YMLDEALLOC(cp);
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
