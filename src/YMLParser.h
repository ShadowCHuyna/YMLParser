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
} __da_header; /* must match _da_hdr in _da.h */

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
int YMLErrorPrint(void);

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
	_YMLMapGet(object, key, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

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
