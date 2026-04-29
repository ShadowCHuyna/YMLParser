#include "YMLParser.h"
#include "_da.h"
#include "_hm.h"
#include "_yml_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── internal value constructors ───────────────────────────────────── */

static YMLValue _yml_mk_str(const char *v, struct YMLAllocator* alloc)
{
	YMLValue r = {.type = YML_STRING, .allocator = alloc};
	r.value.string = v ? yml_strdup(v, alloc) : NULL;
	return r;
}

static YMLValue _yml_mk_node(YMLValue *v, struct YMLAllocator *target_alloc)
{
	if (!v)
		return (YMLValue){.type = YML_NULL};
	YMLValue *cp = yml_deep_copy(v, target_alloc);
	if (!cp)
		return (YMLValue){.type = YML_NULL};
	YMLValue r = *cp;
	YMLDEALLOC(cp, r.allocator);
	return r;
}

/* ── YMLCreate / YMLCreateArr ───────────────────────────────────────── */

YMLValue *_YMLCreate(struct YMLAllocator* alloc)
{
	YMLValue *v = YMLALLOC(sizeof(YMLValue), alloc);
	if (!v)
		return NULL;
	_hm *hm = hm_new(8, alloc);
	if (!hm)
	{
		YMLDEALLOC(v, alloc);
		return NULL;
	}
	v->type = YML_OBJECT;
	v->value.object = hm;
	v->allocator = alloc;
	return v;
}

YMLValue *_YMLCreateArr(struct YMLAllocator* alloc)
{
	YMLValue *v = YMLALLOC(sizeof(YMLValue), alloc);
	if (!v)
		return NULL;
	YMLValue *arr = da_new(YMLValue, 4, alloc);
	if (!arr)
	{
		YMLDEALLOC(v, alloc);
		return NULL;
	}
	v->type = YML_ARRAY;
	v->value.array = arr;
	v->allocator = alloc;
	return v;
}

/* ── helpers: вставка с cleanup дубликата ───────────────────────────── */

static void map_insert(YMLValue *obj, const char *key, YMLValue val)
{
	_hm *hm = (_hm *)obj->value.object;
	YMLValue *existing = hm_get(hm, key);
	if (existing)
		yml_value_free_impl(existing); /* чистим содержимое старого значения */
	hm_set(hm, key, val);
}

/* ── YMLMapAdd ──────────────────────────────────────────────────────── */

void YMLMapAddNull(YMLValue *obj, const char *key)
{
	map_insert(obj, key, (YMLValue){.type = YML_NULL, .allocator = obj->allocator});
}

void _YMLMapAdd_bool(YMLValue *obj, const char *key, bool val)
{
	map_insert(obj, key, (YMLValue){.type = YML_BOOL, .value.boolean = val, .allocator = obj->allocator});
}

void _YMLMapAdd_int(YMLValue *obj, const char *key, long long val)
{
	map_insert(obj, key, (YMLValue){.type = YML_INT, .value.integer = val, .allocator = obj->allocator});
}

void _YMLMapAdd_float(YMLValue *obj, const char *key, double val)
{
	map_insert(obj, key, (YMLValue){.type = YML_FLOAT, .value.number = val, .allocator = obj->allocator});
}

void _YMLMapAdd_str(YMLValue *obj, const char *key, const char *val)
{
	map_insert(obj, key, _yml_mk_str(val, obj->allocator));
}

void _YMLMapAdd_node(YMLValue *obj, const char *key, YMLValue *val)
{
	map_insert(obj, key, _yml_mk_node(val, obj->allocator));
}

static YMLValue *_yml_build_int_arr(const long long *arr, size_t len, struct YMLAllocator* alloc)
{
	YMLValue *da = da_new(YMLValue, len > 0 ? len : 1, alloc);
	for (size_t i = 0; i < len; i++)
	{
		YMLValue e = {.type = YML_INT, .value.integer = arr[i], .allocator = alloc};
		da_push(da, e);
	}
	return da;
}

static YMLValue *_yml_build_float_arr(const double *arr, size_t len, struct YMLAllocator* alloc)
{
	YMLValue *da = da_new(YMLValue, len > 0 ? len : 1, alloc);
	for (size_t i = 0; i < len; i++)
	{
		YMLValue e = {.type = YML_FLOAT, .value.number = arr[i], .allocator = alloc};
		da_push(da, e);
	}
	return da;
}

static YMLValue *_yml_build_str_arr(const char *const *arr, size_t len, struct YMLAllocator* alloc)
{
	YMLValue *da = da_new(YMLValue, len > 0 ? len : 1, alloc);
	for (size_t i = 0; i < len; i++)
	{
		YMLValue e = {.type = YML_STRING,
		              .value.string = arr[i] ? yml_strdup(arr[i], alloc) : NULL,
		              .allocator = alloc};
		da_push(da, e);
	}
	return da;
}

void _YMLMapAddArr_int(YMLValue *obj, const char *key,
                       const long long *arr, size_t len)
{
	YMLValue v = {.type = YML_ARRAY, .value.array = _yml_build_int_arr(arr, len, obj->allocator), .allocator = obj->allocator};
	map_insert(obj, key, v);
}

void _YMLMapAddArr_float(YMLValue *obj, const char *key,
                         const double *arr, size_t len)
{
	YMLValue v = {.type = YML_ARRAY, .value.array = _yml_build_float_arr(arr, len, obj->allocator), .allocator = obj->allocator};
	map_insert(obj, key, v);
}

void _YMLMapAddArr_str(YMLValue *obj, const char *key,
                       const char *const *arr, size_t len)
{
	YMLValue v = {.type = YML_ARRAY, .value.array = _yml_build_str_arr(arr, len, obj->allocator), .allocator = obj->allocator};
	map_insert(obj, key, v);
}

/* ── YMLArrPush ─────────────────────────────────────────────────────── */

void YMLArrPushNull(YMLValue *arr)
{
	YMLValue e = {.type = YML_NULL, .allocator = arr->allocator};
	da_push(arr->value.array, e);
}

void _YMLArrPush_bool(YMLValue *arr, bool val)
{
	YMLValue e = {.type = YML_BOOL, .value.boolean = val, .allocator = arr->allocator};
	da_push(arr->value.array, e);
}

void _YMLArrPush_int(YMLValue *arr, long long val)
{
	YMLValue e = {.type = YML_INT, .value.integer = val, .allocator = arr->allocator};
	da_push(arr->value.array, e);
}

void _YMLArrPush_float(YMLValue *arr, double val)
{
	YMLValue e = {.type = YML_FLOAT, .value.number = val, .allocator = arr->allocator};
	da_push(arr->value.array, e);
}

void _YMLArrPush_str(YMLValue *arr, const char *val)
{
	da_push(arr->value.array, _yml_mk_str(val, arr->allocator));
}

void _YMLArrPush_node(YMLValue *arr, YMLValue *val)
{
	da_push(arr->value.array, _yml_mk_node(val, arr->allocator));
}

void _YMLArrPushArr_int(YMLValue *arr, const long long *c_arr, size_t len)
{
	YMLValue nested = {.type = YML_ARRAY,
	                   .value.array = _yml_build_int_arr(c_arr, len, arr->allocator),
	                   .allocator = arr->allocator};
	da_push(arr->value.array, nested);
}

void _YMLArrPushArr_float(YMLValue *arr, const double *c_arr, size_t len)
{
	YMLValue nested = {.type = YML_ARRAY,
	                   .value.array = _yml_build_float_arr(c_arr, len, arr->allocator),
	                   .allocator = arr->allocator};
	da_push(arr->value.array, nested);
}

void _YMLArrPushArr_str(YMLValue *arr, const char *const *c_arr, size_t len)
{
	YMLValue nested = {.type = YML_ARRAY,
	                   .value.array = _yml_build_str_arr(c_arr, len, arr->allocator),
	                   .allocator = arr->allocator};
	da_push(arr->value.array, nested);
}

/* ══════════════════════════ SERIALIZER ══════════════════════════════ */

typedef struct
{
	FILE   *stream; /* != NULL → поток */
	char   *buf;    /* != NULL → буфер */
	size_t  cap;    /* ёмкость буфера */
	size_t  pos;    /* байт записано */
	int     indent; /* пробелов на уровень */
	int     emit_start; /* 1 → emit "---" */
	int     ok;
	char    error[256];
} _YMLWriteCtx;

/* Записать len байт; при переполнении выставить ok=1 */
static void _yml_emit(_YMLWriteCtx *ctx, const char *s, size_t len)
{
	if (ctx->ok)
		return;
	if (ctx->stream)
	{
		fwrite(s, 1, len, ctx->stream);
	}
	else
	{
		if (ctx->pos + len + 1 > ctx->cap)
		{
			ctx->ok = 1;
			snprintf(ctx->error, sizeof(ctx->error), "YMLWrite: buffer too small");
			return;
		}
		memcpy(ctx->buf + ctx->pos, s, len);
		ctx->pos += len;
	}
}

static void _yml_emit_c(_YMLWriteCtx *ctx, char c)
{
	_yml_emit(ctx, &c, 1);
}

static void _yml_emit_str(_YMLWriteCtx *ctx, const char *s)
{
	if (s)
		_yml_emit(ctx, s, strlen(s));
}

static void _yml_emit_indent(_YMLWriteCtx *ctx, int depth)
{
	for (int i = 0; i < depth * ctx->indent; i++)
		_yml_emit_c(ctx, ' ');
}

/* Нужны ли кавычки для plain-скаляра? */
static int _yml_needs_quotes(const char *s)
{
	if (!s || *s == '\0')
		return 1;
	/* ведущий пробел */
	if (*s == ' ' || *s == '\t')
		return 1;
	/* спецсимволы в начале */
	switch (*s)
	{
	case '-': case ':': case '?': case '#': case '&': case '*':
	case '!': case '|': case '>': case '\'': case '"': case '%':
	case '@': case '`': case '[': case '{': case ',':
		return 1;
	default:
		break;
	}
	/* управляющие символы или спецпоследовательности внутри */
	for (const char *p = s; *p; p++)
	{
		if ((unsigned char)*p < 0x20)
			return 1; /* \n, \r, \t и т.д. */
		if (*p == ':' && (*(p + 1) == ' ' || *(p + 1) == '\0'))
			return 1;
		if (*p == ' ' && *(p + 1) == '#')
			return 1;
	}
	/* строки совпадающие с YAML-скалярами */
	if (strcmp(s, "null") == 0 || strcmp(s, "~") == 0 ||
	    strcmp(s, "true") == 0 || strcmp(s, "false") == 0 ||
	    strcmp(s, ".inf") == 0 || strcmp(s, "-.inf") == 0 ||
	    strcmp(s, ".nan") == 0 || strcmp(s, ".Inf") == 0 ||
	    strcmp(s, ".NaN") == 0 || strcmp(s, ".INF") == 0)
		return 1;
	return 0;
}

/* Записать строку с кавычками + escape если нужно, иначе plain */
static void _yml_write_string(_YMLWriteCtx *ctx, const char *s)
{
	if (!s)
	{
		_yml_emit_str(ctx, "null");
		return;
	}
	if (!_yml_needs_quotes(s))
	{
		_yml_emit_str(ctx, s);
		return;
	}
	_yml_emit_c(ctx, '"');
	for (const char *p = s; *p; p++)
	{
		switch (*p)
		{
		case '"':  _yml_emit(ctx, "\\\"", 2); break;
		case '\\': _yml_emit(ctx, "\\\\", 2); break;
		case '\n': _yml_emit(ctx, "\\n",  2); break;
		case '\r': _yml_emit(ctx, "\\r",  2); break;
		case '\t': _yml_emit(ctx, "\\t",  2); break;
		default:   _yml_emit_c(ctx, *p);      break;
		}
	}
	_yml_emit_c(ctx, '"');
}

static void _yml_write_node(_YMLWriteCtx *ctx, const YMLValue *node, int depth);

static void _yml_write_node(_YMLWriteCtx *ctx, const YMLValue *node, int depth)
{
	if (ctx->ok)
		return;
	if (!node)
	{
		_yml_emit_str(ctx, "null");
		return;
	}
	switch (node->type)
	{
	case YML_NULL:
		_yml_emit_str(ctx, "null");
		break;
	case YML_BOOL:
		_yml_emit_str(ctx, node->value.boolean ? "true" : "false");
		break;
	case YML_INT:
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%lld", (long long)node->value.integer);
		_yml_emit_str(ctx, buf);
		break;
	}
	case YML_FLOAT:
	{
		double d = node->value.number;
		char buf[32];
		if (isinf(d))
			snprintf(buf, sizeof(buf), "%s", d > 0 ? ".inf" : "-.inf");
		else if (isnan(d))
			snprintf(buf, sizeof(buf), ".nan");
		else
			snprintf(buf, sizeof(buf), "%.17g", d);
		_yml_emit_str(ctx, buf);
		break;
	}
	case YML_STRING:
		_yml_write_string(ctx, node->value.string);
		break;
	case YML_ARRAY:
	{
		size_t n = da_len(node->value.array);
		if (n == 0)
		{
			_yml_emit_str(ctx, "[]");
			break;
		}
		for (size_t i = 0; i < n; i++)
		{
			const YMLValue *elem = &node->value.array[i];
			_yml_emit_c(ctx, '\n');
			_yml_emit_indent(ctx, depth);
			_yml_emit_str(ctx, "- ");
			int is_collection = (elem->type == YML_ARRAY || elem->type == YML_OBJECT);
			if (is_collection)
				_yml_write_node(ctx, elem, depth + 1);
			else
				_yml_write_node(ctx, elem, depth);
		}
		break;
	}
	case YML_OBJECT:
	{
		_hm *hm = (_hm *)node->value.object;
		if (hm->len == 0)
		{
			_yml_emit_str(ctx, "{}");
			break;
		}
		size_t idx = 0;
		const char *key;
		YMLValue *val;
		while (hm_next(hm, &idx, &key, &val))
		{
			_yml_emit_c(ctx, '\n');
			_yml_emit_indent(ctx, depth);
			_yml_write_string(ctx, key);
			_yml_emit_c(ctx, ':');
			int is_collection = (val->type == YML_ARRAY || val->type == YML_OBJECT);
			if (is_collection)
				_yml_write_node(ctx, val, depth + 1);
			else
			{
				_yml_emit_c(ctx, ' ');
				_yml_write_node(ctx, val, depth);
			}
		}
		break;
	}
	default:
		_yml_emit_str(ctx, "null");
		break;
	}
}

static void _yml_write(YMLValue *obj, _YMLWriteCtx *ctx)
{
	if (ctx->emit_start)
		_yml_emit_str(ctx, "---");

	/* корневой объект/массив начинается без ведущего \n */
	if (obj && (obj->type == YML_OBJECT || obj->type == YML_ARRAY))
	{
		_hm *hm = NULL;
		size_t n = 0;
		if (obj->type == YML_OBJECT)
		{
			hm = (_hm *)obj->value.object;
			n = hm->len;
		}
		else
			n = da_len(obj->value.array);

		if (n == 0)
		{
			if (ctx->emit_start)
				_yml_emit_c(ctx, '\n');
			_yml_emit_str(ctx, obj->type == YML_OBJECT ? "{}\n" : "[]\n");
			return;
		}

		/* записываем дочерние элементы: каждый начинается с \n+indent */
		if (obj->type == YML_OBJECT)
		{
			size_t idx = 0;
			const char *key;
			YMLValue *val;
			while (hm_next(hm, &idx, &key, &val))
			{
				_yml_emit_c(ctx, '\n');
				_yml_write_string(ctx, key);
				_yml_emit_c(ctx, ':');
				int is_col = (val->type == YML_ARRAY || val->type == YML_OBJECT);
				if (is_col)
					_yml_write_node(ctx, val, 1);
				else
				{
					_yml_emit_c(ctx, ' ');
					_yml_write_node(ctx, val, 0);
				}
			}
		}
		else
		{
			for (size_t i = 0; i < n; i++)
			{
				const YMLValue *elem = &obj->value.array[i];
				_yml_emit_c(ctx, '\n');
				_yml_emit_str(ctx, "- ");
				int is_col = (elem->type == YML_ARRAY || elem->type == YML_OBJECT);
				if (is_col)
					_yml_write_node(ctx, elem, 1);
				else
					_yml_write_node(ctx, elem, 0);
			}
		}
		_yml_emit_c(ctx, '\n');
	}
	else
	{
		if (ctx->emit_start)
			_yml_emit_c(ctx, '\n');
		_yml_write_node(ctx, obj, 0);
		_yml_emit_c(ctx, '\n');
	}
}

/* ── public ─────────────────────────────────────────────────────────── */

void _YMLWriteStream(YMLValue *obj, FILE *stream, struct _YMLWriteOptions opts)
{
	_YMLWriteCtx ctx = {
		.stream = stream,
		.buf    = NULL,
		.cap    = 0,
		.pos    = 0,
		.indent = opts.indent > 0 ? opts.indent : 2,
		.emit_start = opts.start,
		.ok     = 0,
	};
	_yml_write(obj, &ctx);
	if (opts.ok)
		*opts.ok = ctx.ok;
	if (opts.error && ctx.ok)
		*opts.error = ctx.error;
}

int _YMLWriteBuf(YMLValue *obj, char *buf, size_t cap, struct _YMLWriteOptions opts)
{
	_YMLWriteCtx ctx = {
		.stream = NULL,
		.buf    = buf,
		.cap    = cap,
		.pos    = 0,
		.indent = opts.indent > 0 ? opts.indent : 2,
		.emit_start = opts.start,
		.ok     = 0,
	};
	_yml_write(obj, &ctx);
	if (ctx.ok == 0 && cap > 0)
		buf[ctx.pos] = '\0';
	if (opts.ok)
		*opts.ok = ctx.ok;
	if (opts.error && ctx.ok)
		*opts.error = ctx.error;
	return ctx.ok ? -1 : (int)ctx.pos;
}
