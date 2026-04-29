#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * YMLValueType — type of a stored value.
 *
 *   YML_NULL    no payload
 *   YML_BOOL    bool
 *   YML_INT     int64_t  (integers: 42, 0xFF, 0o17)
 *   YML_FLOAT   double   (floats: 3.14, .inf, .nan)
 *   YML_STRING  const char* (heap-allocated, NUL-terminated, owned by YMLValue)
 *   YML_OBJECT  hm<str, YMLValue>  — access via YMLMapGet / YMLMapForech
 *   YML_ARRAY   da<YMLValue>       — index with value.array[i], length via YMLArrayLen
 *
 * YML_ANY — sentinel only for _YMLOptionals.type (means "skip type check").
 *           Not a valid node type.
 */
typedef enum YMLValueType
{
	YML_ANY = -1,
	YML_NULL = 0,
	YML_BOOL,
	YML_INT,
	YML_FLOAT,
	YML_STRING,
	YML_ARRAY,
	YML_OBJECT,
} YMLValueType;


struct YMLAllocator {
	void* (*alloc)  (size_t len,               void* ctx, const char* FILE, int LINE);
	void* (*realloc)(void* ptr, size_t new_len, void* ctx, const char* FILE, int LINE);
	void* (*calloc) (size_t n, size_t size,     void* ctx, const char* FILE, int LINE);
	void  (*dealloc)(void* ptr,                 void* ctx, const char* FILE, int LINE);
	void* ctx;
};


/*
 * YMLValue — a single node in the parse tree.
 *
 * The type field determines which union member is active:
 *   YML_BOOL   → value.boolean
 *   YML_INT    → value.integer
 *   YML_FLOAT  → value.number
 *   YML_STRING → value.string  (pointer to a NUL-terminated string)
 *   YML_OBJECT → value.object  (hm<str → YMLValue>)
 *   YML_ARRAY  → value.array   (da<YMLValue>, hidden header, see YMLArrayLen)
 *   YML_NULL   → value is unused
 */
typedef struct YMLValue
{
	YMLValueType type;
	union
	{
		bool boolean;
		int64_t integer;
		double number;
		const char *string;
		void *object;			// _hm* — opaque pointer, access via YMLMapGet/YMLMapForech
		struct YMLValue *array; // da<YMLValue> — direct indexing arr[i], length via YMLArrayLen
	} value;
	struct YMLAllocator* allocator; 
} YMLValue;

/*
 * Hidden header of dynamic arrays da<T>.
 * Stored immediately before the first element in memory:
 *   [__da_header][elem0][elem1]...
 * The user holds a pointer to elem0.
 * All da arrays are freed by YMLDestroy / YMLDestroyStream —
 * copy any data you need before destroying the tree.
 */
typedef struct
{
	size_t len;
	size_t cap;
	struct YMLAllocator* alloc;
} __da_header;

/*
 * Returns the number of elements in a da array.
 * Example: for (size_t i = 0; i < YMLArrayLen(arr); i++) { ... arr[i] ... }
 */
#define YMLArrayLen(arr) ((arr) ? ((__da_header *)(arr) - 1)->len : (size_t)0)

struct _YMLOptionals
{
	int *ok;		   // 0 — no error, otherwise error code
	char **error;	   // *error points to a buffer with the error message
	YMLValueType type; // for YMLMapGet: expected type (YML_ANY — skip check)
	char splitter;	   // for YMLMapGet: if non-zero, split key by this char and traverse nested objects
	struct YMLAllocator* allocator;
};

/*
 * Parses a single YAML document and returns the root YMLValue*.
 * If the string contains multiple documents (---), only the first is parsed.
 *
 * Error codes for ok:
 *   1 — syntax error
 *   2 — out of memory
 */
YMLValue *_YMLParse(const char *yml_str, struct _YMLOptionals optionals);

#define YMLParse(yml_str, ...) \
	_YMLParse(yml_str, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Parses a YAML stream with multiple documents (separated by ---).
 * Returns da<YMLValue*> — an array of root nodes, one per document.
 * Get the length via YMLArrayLen.
 *
 * Example:
 *   YMLValue **docs = YMLParseStream(yml_str);
 *   for (size_t i = 0; i < YMLArrayLen(docs); i++) { ... docs[i] ... }
 *   YMLDestroyStream(docs);
 *
 * Error codes for ok: same as YMLParse.
 */
YMLValue **_YMLParseStream(const char *yml_str, struct _YMLOptionals optionals);

#define YMLParseStream(yml_str, ...) \
	_YMLParseStream(yml_str, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Frees the memory of a single-document tree (from YMLParse).
 */
void _YMLDestroy(YMLValue *root, struct _YMLOptionals optionals);

#define YMLDestroy(root, ...) \
	_YMLDestroy(root, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Frees all documents in a stream and the da array itself (from YMLParseStream).
 */
void _YMLDestroyStream(YMLValue **stream, struct _YMLOptionals optionals);

#define YMLDestroyStream(stream, ...) \
	_YMLDestroyStream(stream, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * If the last operation produced an error, prints it to stderr
 * and returns the error code. Otherwise returns 0.
 */
int YMLPrintError(void);

/*
 * Returns a YMLValue* by string key from a YML_OBJECT.
 * If the key is not found — error (ok=1).
 * If .type != YML_ANY and the value type does not match — error (ok=2).
 *
 * Error codes for ok:
 *   1 — key not found
 *   2 — type mismatch
 */
/* hm — YMLValue.value.object pointer (void* = _hm*) */
YMLValue *_YMLMapGet(void *hm, const char *key, struct _YMLOptionals optionals);

#define YMLMapGet(object, key, ...) \
	_YMLMapGet(object, key, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, .splitter = 0, __VA_ARGS__})


#define __YML_CTYPE_YML_BOOL    bool
#define __YML_CTYPE_YML_INT     int64_t
#define __YML_CTYPE_YML_FLOAT   double
#define __YML_CTYPE_YML_STRING  const char*
#define __YML_CTYPE_YML_ARRAY   YMLValue*
#define __YML_CTYPE_YML_OBJECT  void*

#define YMLTYPE_2_CTYPE(type) \
	__YML_CTYPE_##type


/*
 * YMLMapGetValue — like YMLMapGet, but returns the C value directly instead of YMLValue*.
 * TYPE must be one of: YML_BOOL, YML_INT, YML_FLOAT, YML_STRING, YML_ARRAY, YML_OBJECT.
 * The return type is resolved at compile time via _Generic:
 *   YML_BOOL   → bool
 *   YML_INT    → int64_t
 *   YML_FLOAT  → double
 *   YML_STRING → const char*
 *   YML_ARRAY  → YMLValue*
 *   YML_OBJECT → void*
 *
 * Sets .type=TYPE automatically; optional args (.ok, .error, .splitter) are forwarded.
 *
 * WARNING: if the key is not found or the type does not match, YMLMapGet returns YMLVallue {.type=YML_NULL, .value=NULL}
 *
 * Examples:
 *   int64_t     age  = YMLMapGetValue(obj, "age",   YML_INT);
 *   const char *name = YMLMapGetValue(obj, "name",  YML_STRING, .ok=&ok);
 *   double      x    = YMLMapGetValue(obj, "score", YML_FLOAT,  .splitter='.');
 */
#define YMLMapGetValue(obj, key, TYPE, ...) \
	_Generic(((YMLTYPE_2_CTYPE(TYPE))0), \
		double:        YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.number, \
		int64_t:       YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.integer, \
		bool:          YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.boolean, \
		const char*:   YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.string, \
		char*:         YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.string, \
		void*:         YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.object, \
		YMLValue*:     YMLMapGet(obj, key, .type=TYPE, __VA_ARGS__)->value.array \
	)

/*
 * Iterator over key-value pairs of a YML_OBJECT.
 * Used internally by the YMLMapForech macro.
 */
/* _hm — opaque pointer to the internal hm structure, _i — current slot index. */
typedef struct
{
	void *_hm;
	size_t _i;
} _YMLMapIter;
/* hm — YMLValue.value.object pointer */
_YMLMapIter _YMLMapIterBegin(void *hm);
bool _YMLMapIterNext(_YMLMapIter *iter, const char **key, YMLValue **value);

/*
 * Iterate over all key-value pairs of an object.
 * key_name and val_name are variable names declared by the macro inside the loop.
 *
 * Example:
 *   YMLMapForech(root->value.object, key, val) {
 *       printf("%s\n", key);
 *   }
 */
#define YMLMapForech(object, key_name, val_name)                                                         \
	for (_YMLMapIter _yml_it_ = _YMLMapIterBegin(object), *_yml_p_ = &_yml_it_; _yml_p_; _yml_p_ = NULL) \
		for (const char *(key_name) = NULL; _yml_p_; _yml_p_ = NULL)                                     \
			for (YMLValue * (val_name) = NULL; _YMLMapIterNext(&_yml_it_, (const char **)&(key_name), &(val_name));)

/* ════════════════════════ WRITER API ════════════════════════════════ */

#include <stdio.h>

/*
 * Options for YMLWrite / YMLWriteBuf.
 *
 * .indent  — spaces per nesting level (default 2)
 * .start   — emit "---" document-start marker (default 0)
 * .ok      — 0 on success, 1 if buffer too small, 2 on OOM
 * .error   — pointer to static error message buffer
 */
struct _YMLWriteOptions
{
	int   *ok;
	char **error;
	int    indent;
	int    start;
	struct YMLAllocator* allocator;
};

/* ── Create ──────────────────────────────────────────────────────── */

/*
 * Allocate an empty YML_OBJECT node.
 * Free with YMLDestroy.
 */
YMLValue *_YMLCreate(struct YMLAllocator* alloc);

#define YMLCreate(...) _YMLCreate(__VA_ARGS__)
	
YMLValue *_YMLCreateArr(struct YMLAllocator* alloc);

#define YMLCreateArr(...) _YMLCreateArr(__VA_ARGS__)

/* ── YMLMapAdd ───────────────────────────────────────────────────── */

/* Add a null value: YMLMapAddNull(obj, "key"); */
void YMLMapAddNull (YMLValue *obj, const char *key);

void _YMLMapAdd_bool (YMLValue *obj, const char *key, bool       val);
void _YMLMapAdd_int  (YMLValue *obj, const char *key, long long  val);
void _YMLMapAdd_float(YMLValue *obj, const char *key, double     val);
void _YMLMapAdd_str  (YMLValue *obj, const char *key, const char *val);
void _YMLMapAdd_node (YMLValue *obj, const char *key, YMLValue   *val);

void _YMLMapAddArr_int  (YMLValue *obj, const char *key, const long long   *arr, size_t len);
void _YMLMapAddArr_float(YMLValue *obj, const char *key, const double      *arr, size_t len);
void _YMLMapAddArr_str  (YMLValue *obj, const char *key, const char *const *arr, size_t len);

/*
 * Add a scalar or nested node. Type is inferred via _Generic.
 *
 *   YMLMapAdd(obj, "n", 42);           → YML_INT
 *   YMLMapAdd(obj, "f", 3.14);         → YML_FLOAT
 *   YMLMapAdd(obj, "s", "hello");      → YML_STRING (strdup'd)
 *   YMLMapAdd(obj, "b", (bool)true);   → YML_BOOL
 *   YMLMapAdd(obj, "sub", node);       → deep-copied YMLValue*
 *
 * Duplicate keys are overwritten (old value freed).
 */
#define YMLMapAdd(obj, key, val) _Generic((val),        \
    _Bool:        _YMLMapAdd_bool,                      \
    int:          _YMLMapAdd_int,                       \
    long:         _YMLMapAdd_int,                       \
    long long:    _YMLMapAdd_int,                       \
    float:        _YMLMapAdd_float,                     \
    double:       _YMLMapAdd_float,                     \
    char *:       _YMLMapAdd_str,                       \
    const char *: _YMLMapAdd_str,                       \
    YMLValue *:   _YMLMapAdd_node                       \
)(obj, key, val)

/*
 * Add a C array as a YML_ARRAY value.
 *
 *   long long nums[] = {1, 2, 3};
 *   YMLMapAddArr(obj, "nums", nums, 3);
 *
 *   const char *tags[] = {"a", "b"};
 *   YMLMapAddArr(obj, "tags", tags, 2);
 */
#define YMLMapAddArr(obj, key, arr, len) _Generic((arr),  \
    long long *:        _YMLMapAddArr_int,                \
    const long long *:  _YMLMapAddArr_int,                \
    double *:           _YMLMapAddArr_float,              \
    const double *:     _YMLMapAddArr_float,              \
    char **:            _YMLMapAddArr_str,                \
    const char **:      _YMLMapAddArr_str,                \
    const char *const*: _YMLMapAddArr_str                 \
)(obj, key, arr, len)

/* ── YMLArrPush ──────────────────────────────────────────────────── */

/* Push a null element: YMLArrPushNull(arr); */
void YMLArrPushNull (YMLValue *arr);

void _YMLArrPush_bool (YMLValue *arr, bool       val);
void _YMLArrPush_int  (YMLValue *arr, long long  val);
void _YMLArrPush_float(YMLValue *arr, double     val);
void _YMLArrPush_str  (YMLValue *arr, const char *val);
void _YMLArrPush_node (YMLValue *arr, YMLValue   *val);

void _YMLArrPushArr_int  (YMLValue *arr, const long long   *c_arr, size_t len);
void _YMLArrPushArr_float(YMLValue *arr, const double      *c_arr, size_t len);
void _YMLArrPushArr_str  (YMLValue *arr, const char *const *c_arr, size_t len);


/*
 * Push a scalar or nested node.
 *
 *   YMLArrPush(arr, 1);
 *   YMLArrPush(arr, "two");
 *   YMLArrPush(arr, (bool)true);
 *   YMLArrPush(arr, node);
 */
#define YMLArrPush(arr, val) _Generic((val),            \
    _Bool:        _YMLArrPush_bool,                     \
    int:          _YMLArrPush_int,                      \
    long:         _YMLArrPush_int,                      \
    long long:    _YMLArrPush_int,                      \
    float:        _YMLArrPush_float,                    \
    double:       _YMLArrPush_float,                    \
    char *:       _YMLArrPush_str,                      \
    const char *: _YMLArrPush_str,                      \
    YMLValue *:   _YMLArrPush_node                      \
)(arr, val)

/*
 * Push a C array as a nested YML_ARRAY element.
 */
#define YMLArrPushArr(arr, c_arr, len) _Generic((c_arr),  \
    long long *:        _YMLArrPushArr_int,               \
    const long long *:  _YMLArrPushArr_int,               \
    double *:           _YMLArrPushArr_float,             \
    const double *:     _YMLArrPushArr_float,             \
    char **:            _YMLArrPushArr_str,               \
    const char **:      _YMLArrPushArr_str,               \
    const char *const*: _YMLArrPushArr_str                \
)(arr, c_arr, len)

/* ── YMLWrite / YMLWriteBuf ──────────────────────────────────────── */

void _YMLWriteStream(YMLValue *obj, FILE *stream, struct _YMLWriteOptions opts);
int  _YMLWriteBuf   (YMLValue *obj, char *buf, size_t cap, struct _YMLWriteOptions opts);

/*
 * Serialize obj to a FILE* stream.
 *
 *   YMLWrite(obj, stdout);
 *   YMLWrite(obj, fp, .indent=4, .start=1);
 */
#define YMLWriteStream(obj, stream, ...) \
	_YMLWriteStream(obj, stream, \
		(struct _YMLWriteOptions){.indent = 2, .start = 0, ##__VA_ARGS__})

/*
 * Serialize obj into a char buffer. Returns bytes written (without NUL).
 * Returns -1 if the buffer is too small (.ok set to 1).
 *
 *   char buf[1024];
 *   int n = YMLWriteBuf(obj, buf, sizeof(buf));
 *   int n = YMLWriteBuf(obj, buf, sizeof(buf), .start=1, .ok=&ok);
 */
#define YMLWriteBuf(obj, buf, cap, ...) \
	_YMLWriteBuf(obj, buf, cap, \
		(struct _YMLWriteOptions){.indent = 2, .start = 0, ##__VA_ARGS__})