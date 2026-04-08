# YMLParser

A single-header YAML 1.2.2 parser written in C11. Drop in `YMLParser.h`, define `YMLPARSER_IMPLEMENTATION` once, and get a full parse tree back as `YMLValue*`. No dependencies beyond the C standard library and `-lm`.

> [Русская документация](RU_README.md)

---

## Table of contents

1. [Quick start](#quick-start)
2. [API reference](#api-reference)
   - [Types](#types)
   - [YMLParse](#ymlparse)
   - [YMLParseStream](#ymlparsestream)
   - [YMLDestroy / YMLDestroyStream](#ymldestroy--ymldestroystream)
   - [YMLMapGet](#ymlmapget)
   - [YMLMapGetTyped](#ymlmapgettyped)
   - [YMLMapForech](#ymlmapforech)
   - [YMLArrayLen](#YMLArrayLen)
   - [YMLPrintError](#ymlerrorprint)
   - [YMLCreate / YMLCreateArr](#ymlcreate--ymlcreatearr)
   - [YMLMapAdd / YMLMapAddNull / YMLMapAddArr](#ymlmapadd--ymlmapaddnull--ymlmapaddr)
   - [YMLArrPush / YMLArrPushNull / YMLArrPushArr](#ymlarrpush--ymlarrpushnull--ymlarrpusharr)
   - [YMLWriteStream / YMLWriteBuf](#ymlwritestream--ymlwritebuf)
3. [Error handling](#error-handling)
4. [Custom allocator](#custom-allocator)
5. [Build](#build)
6. [What is not supported](#what-is-not-supported)

---

## Quick start

```c
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"
#include <stdio.h>

int main(void) {
	YMLValue *root = YMLParse(
		"name: Alice\n"
		"age: 30\n"
		"tags: [dev, yaml]\n"
	);
	if (YMLPrintError() != 0) return 1;

	char *name = YMLMapGetTyped(root->value.object, "name", YML_STRING);
	int age  = YMLMapGetTyped(root->value.object, "age", YML_INT);
	printf("name=%s  age=%lld\n", name, age);

	YMLValue *tags = YMLMapGet(root->value.object, "tags");
	for (size_t i = 0; i < YMLArrayLen(tags->value.array); i++)
		printf("tag: %s\n", tags->value.array[i].value.string);

	YMLMapForech(root->value.object, key, val)
		printf("key: %s\n", key);

	YMLDestroy(root);
	return 0;
}
```

```sh
gcc -std=c11 -o my_app my_app.c -lm
```

More examples are in the [`examples/`](examples/) directory.

---

## API reference

### Types

```c
typedef enum {
    YML_ANY    = -1,  // only for .type in YMLMapGet (skip type check)
    YML_NULL   =  0,
    YML_BOOL,         // value.boolean  (bool)
    YML_INT,          // value.integer  (int64_t) — dec / 0xFF / 0o17
    YML_FLOAT,        // value.number   (double)  — 3.14 / .inf / .nan
    YML_STRING,       // value.string   (const char*, owned)
    YML_ARRAY,        // value.array    (YMLValue*, da, see YMLArrayLen)
    YML_OBJECT,       // value.object   (void*, access via YMLMapGet/YMLMapForech)
} YMLValueType;

typedef struct YMLValue {
    YMLValueType type;
    union {
        bool            boolean;
        int64_t         integer;
        double          number;
        const char     *string;
        void           *object;
        struct YMLValue *array;
    } value;
} YMLValue;
```

---

### YMLParse

```c
YMLValue *YMLParse(const char *yml_str, ...options...);
```

Parses a single YAML document. If multiple `---` markers are present, only the first document is parsed.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `.ok` | `int*` | `NULL` | Result code: `0` — success, `1` — syntax error, `2` — OOM |
| `.error` | `char**` | `NULL` | Error message (pointer to an internal buffer) |

```c
int ok = 0; char *err = NULL;
YMLValue *root = YMLParse("x: 1\n", .ok=&ok, .error=&err);
if (ok != 0) fprintf(stderr, "error %d: %s\n", ok, err);
```

---

### YMLParseStream

```c
YMLValue **YMLParseStream(const char *yml_str, ...options...);
```

Parses a YAML stream of multiple documents separated by `---`. Returns `da<YMLValue*>` — an array of root nodes. Use `YMLArrayLen` to get the count. Anchors do not survive document boundaries (`---` / `...`).

```c
YMLValue **docs = YMLParseStream("---\nfoo: 1\n---\nbar: 2\n", .ok=&ok);
for (size_t i = 0; i < YMLArrayLen(docs); i++) { /* docs[i] */ }
YMLDestroyStream(docs);
```

---

### YMLDestroy / YMLDestroyStream

```c
void YMLDestroy(YMLValue *root);
void YMLDestroyStream(YMLValue **stream);
```

Recursively free the entire parse tree. Safe to call with `NULL`. After the call, all `YMLValue*` and `const char*` pointers from that tree are invalid.

---

### YMLMapGet

```c
YMLValue *YMLMapGet(void *object, const char *key, ...options...);
```

Returns a value by string key from a `YML_OBJECT`. `object` is `root->value.object`.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `.ok` | `int*` | `NULL` | `0` — found, `1` — key not found, `2` — type mismatch |
| `.error` | `char**` | `NULL` | Error message |
| `.type` | `YMLValueType` | `YML_ANY` | Expected type; mismatch sets `ok=2` and returns `NULL` |
| `.splitter` | `char` | `0` | If non-zero, split `key` by this character and traverse nested objects |

```c
// without type check
YMLValue *v = YMLMapGet(root->value.object, "name");

// with type check and error code
YMLValue *n = YMLMapGet(root->value.object, "age", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* key not found or wrong type */ }

// dot-path traversal — equivalent to two nested YMLMapGet calls
YMLValue *city = YMLMapGet(root->value.object, "address.city", .splitter='.');
```

If `.ok` is not passed, errors go to the global state accessible via `YMLErrorPrint()`.

#### Path traversal with `.splitter`

Setting `.splitter` to a non-zero character splits `key` by that character and traverses nested objects automatically:

```c
// YAML:
// server:
//   host: localhost
//   port: 8080

YMLValue *host = YMLMapGet(root->value.object, "server.host", .splitter='.');
YMLValue *port = YMLMapGet(root->value.object, "server.port", .splitter='.', .type=YML_INT);
```

Any character can be used as a separator:

```c
YMLMapGet(root->value.object, "server/host", .splitter='/');
```

Error codes with `.splitter`:
- `ok=1` — a key in the path was not found, or an intermediate node is not an object
- `ok=2` — type mismatch on the final key

> Keys that literally contain the separator character are not supported when using `.splitter`. Use plain `YMLMapGet` calls for such keys.

---

### YMLMapGetTyped

```c
TYPE YMLMapGetTyped(void *object, const char *key, YMLValueType TYPE, ...options...);
```

Like `YMLMapGet`, but returns the underlying C value directly instead of `YMLValue*`. The return type is resolved at compile time via `_Generic` based on `TYPE`:

| `TYPE` | Return type |
|--------|-------------|
| `YML_BOOL` | `bool` |
| `YML_INT` | `int64_t` |
| `YML_FLOAT` | `double` |
| `YML_STRING` | `const char*` |
| `YML_ARRAY` | `YMLValue*` |
| `YML_OBJECT` | `void*` |

Sets `.type=TYPE` automatically. Optional args (`.ok`, `.error`, `.splitter`) are forwarded to `YMLMapGet`.

> **Warning:** if the key is not found or the type does not match, `YMLMapGet` returns `YMLVallue` `{.type=YML_NULL, .value=NULL}`

```c
int ok;

int64_t     age  = YMLMapGetTyped(root->value.object, "age",   YML_INT,    .ok=&ok);
const char *name = YMLMapGetTyped(root->value.object, "name",  YML_STRING, .ok=&ok);
double      x    = YMLMapGetTyped(root->value.object, "score", YML_FLOAT);

// dot-path traversal works too
const char *city = YMLMapGetTyped(root->value.object, "address.city", YML_STRING, .splitter='.');
```

---

### YMLMapForech

```c
YMLMapForech(object, key_name, val_name) { ... }
```

Iterate over all key–value pairs of a `YML_OBJECT`. `key_name` and `val_name` are variable names declared inside the macro as `const char *key_name` and `YMLValue *val_name`.

```c
YMLMapForech(root->value.object, key, val) {
    printf("%s: type=%d\n", key, val->type);
}
```

Iteration order is unspecified (open-addressing hash map).

---

### YMLArrayLen

```c
size_t YMLArrayLen(YMLValue *array);
```

Returns the number of elements in a `YML_ARRAY`. Returns `0` safely for `NULL`.

```c
YMLValue *arr = YMLMapGet(root->value.object, "items");
for (size_t i = 0; i < YMLArrayLen(arr->value.array); i++)
    printf("[%zu] = %lld\n", i, (long long)arr->value.array[i].value.integer);
```

---

### YMLPrintError

```c
int YMLPrintError(void);
```

If the last operation produced an error (and `.ok` was not passed), prints the message to `stderr` and returns the error code. Otherwise returns `0`. Each call resets the global error state.

```c
YMLMapGet(root->value.object, "missing_key");
if (YMLPrintError() != 0) { /* ... */ }
```

---

### YMLCreate / YMLCreateArr

```c
YMLValue *YMLCreate(void);
YMLValue *YMLCreateArr(void);
```

Create an empty `YML_OBJECT` or `YML_ARRAY` node. Free with `YMLDestroy`.

```c
YMLValue *obj = YMLCreate();     // empty mapping
YMLValue *arr = YMLCreateArr();  // empty sequence
```

---

### YMLMapAdd / YMLMapAddNull / YMLMapAddArr

```c
YMLMapAdd(obj, key, val);
YMLMapAddNull(obj, key);
YMLMapAddArr(obj, key, c_array, len);
```

Add a key–value pair to a `YML_OBJECT`. The type of `val` is inferred via `_Generic`.

| Call | Inferred type |
|------|--------------|
| `YMLMapAdd(obj, "n", (long long)42)` | `YML_INT` |
| `YMLMapAdd(obj, "f", 3.14)` | `YML_FLOAT` |
| `YMLMapAdd(obj, "s", "hello")` | `YML_STRING` (strdup'd) |
| `YMLMapAdd(obj, "b", (bool)true)` | `YML_BOOL` |
| `YMLMapAdd(obj, "sub", node)` | deep-copied `YMLValue*` |
| `YMLMapAddNull(obj, "k")` | `YML_NULL` |

Duplicate keys are overwritten (old value freed). When adding a nested `YMLValue*` the node is deep-copied, so the original can be destroyed immediately after.

`YMLMapAddArr` converts a plain C array into a `YML_ARRAY` child and supports `long long[]`, `double[]`, and `const char*[]`.

```c
YMLValue *obj = YMLCreate();
YMLMapAdd(obj, "name",   "Alice");
YMLMapAdd(obj, "age",    (long long)30);
YMLMapAdd(obj, "score",  98.6);
YMLMapAdd(obj, "active", (bool)true);
YMLMapAddNull(obj, "token");

long long scores[] = {95, 87, 100};
YMLMapAddArr(obj, "scores", scores, 3);

const char *tags[] = {"yaml", "c11"};
YMLMapAddArr(obj, "tags", tags, 2);
```

---

### YMLArrPush / YMLArrPushNull / YMLArrPushArr

```c
YMLArrPush(arr, val);
YMLArrPushNull(arr);
YMLArrPushArr(arr, c_array, len);
```

Append an element to a `YML_ARRAY`. Same type inference rules as `YMLMapAdd`. Nested `YMLValue*` is deep-copied.

```c
YMLValue *arr = YMLCreateArr();
YMLArrPush(arr, (long long)1);
YMLArrPush(arr, 2.5);
YMLArrPush(arr, "three");
YMLArrPush(arr, (bool)false);
YMLArrPushNull(arr);
```

`YMLArrPushArr` appends a C array as a nested `YML_ARRAY` element.

---

### YMLWriteStream / YMLWriteBuf

```c
void YMLWriteStream(YMLValue *obj, FILE *stream, ...options...);
int  YMLWriteBuf   (YMLValue *obj, char *buf, size_t cap, ...options...);
```

Serialize a `YMLValue` tree to YAML text.

`YMLWriteStream` writes to any `FILE*`. `YMLWriteBuf` writes into a caller-supplied buffer and returns the number of bytes written (excluding the NUL terminator), or `-1` on error.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `.indent` | `int` | `2` | Spaces per nesting level |
| `.start` | `int` | `0` | Emit `---` document-start marker |
| `.ok` | `int*` | `NULL` | `0` — success, `1` — buffer too small, `2` — OOM |
| `.error` | `char**` | `NULL` | Error message |

```c
// to stdout
YMLWriteStream(obj, stdout);
YMLWriteStream(obj, stdout, .indent=4, .start=1);

// to buffer
char buf[1024];
int ok = 0;
int n = YMLWriteBuf(obj, buf, sizeof(buf), .ok=&ok);
if (ok != 0) fprintf(stderr, "write error\n");
```

Round-trip example:

```c
YMLValue *obj = YMLCreate();
YMLMapAdd(obj, "x", (long long)1);

char buf[256];
YMLWriteBuf(obj, buf, sizeof(buf));
YMLDestroy(obj);

YMLValue *root = YMLParse(buf);
```

---

## Error handling

All API functions accept optional named arguments `.ok` and `.error` implemented as a macro on top of C99 designated initializers and `__VA_ARGS__`. This gives keyword-argument-like syntax in plain C.

```c
YMLValue *v = YMLParse("x: 1\n", .ok=&ok, .error=&err);
```

### Two modes

**1. Explicit check** — pass `.ok` and check after each call:

```c
int ok = 0;
char *err = NULL;

YMLValue *root = YMLParse(yml, .ok=&ok, .error=&err);
if (ok != 0) { fprintf(stderr, "%d: %s\n", ok, err); return ok; }

YMLValue *port = YMLMapGet(root->value.object, "port", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* key not found or wrong type */ }
```

**2. Global state** — skip `.ok`, check once via `YMLErrorPrint()`:

```c
YMLValue *root = YMLParse(yml);
YMLValue *host = YMLMapGet(root->value.object, "host");
YMLValue *port = YMLMapGet(root->value.object, "port");

if (YMLErrorPrint() != 0) return 1;
```

Error state is `_Thread_local` — each thread sees only its own errors.

### Error codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Key not found / syntax error |
| `2` | Type mismatch / OOM |

---

## Custom allocator

By default all heap operations (`malloc`, `realloc`, `calloc`, `free`) go through the C standard library. You can replace them globally before calling any parser function by assigning to `YMLParserAllocator`:

```c
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

static void *my_alloc  (size_t len,              void *ctx, const char *file, int line);
static void *my_realloc(void *ptr, size_t len,   void *ctx, const char *file, int line);
static void *my_calloc (size_t n,  size_t size,  void *ctx, const char *file, int line);
static void  my_free   (void *ptr,               void *ctx, const char *file, int line);

// call before any YMLParse / YMLCreate
YMLParserAllocator = (struct _YMLParserAllocator){
    .alloc   = my_alloc,
    .realloc = my_realloc,
    .calloc  = my_calloc,
    .dealloc = my_free,
    .ctx     = NULL,   // forwarded to every call; use for arena pointer, etc.
};
```

The struct is defined in `YMLParser.h`:

```c
struct _YMLParserAllocator {
    void* (*alloc)  (size_t len,              void *ctx, const char *file, int line);
    void* (*realloc)(void *ptr, size_t new_len, void *ctx, const char *file, int line);
    void* (*calloc) (size_t n,  size_t size,  void *ctx, const char *file, int line);
    void  (*dealloc)(void *ptr,               void *ctx, const char *file, int line);
    void *ctx;
};

extern struct _YMLParserAllocator YMLParserAllocator;
```

`file` and `line` are the call-site `__FILE__` / `__LINE__` — useful for diagnostics and leak tracking. Ignore them if not needed.

> The allocator is a plain global variable, not thread-local. If multiple threads call the parser concurrently, set the allocator once at startup before spawning threads.

---

## Build

### Single-header (recommended)

Copy `YMLParser.h` from the project root into your project. No other files needed.

```sh
gcc -std=c11 -o my_app my_app.c -lm
```

Regenerate `YMLParser.h` from `src/` after making changes:

```sh
make              # runs tools/amalgamate.py → YMLParser.h
```

### Multi-file (for library development)

```sh
make test                          # build and run all tests
make run-test T=test_scalars       # run a single test
make run-example E=example_nested  # run a specific example

make lib-static                    # Linux  → build/libYMLParser.a
make lib-static PLATFORM=windows   # Windows → build/YMLParser.lib
make lib-shared                    # Linux  → build/libYMLParser.so
make lib-shared PLATFORM=windows   # Windows → build/YMLParser.dll
```

Compile manually against `src/`:

```sh
gcc -std=c11 -Isrc -o my_app my_app.c \
    src/YMLParser.c src/YMLWriter.c src/_da.c src/_hm.c \
    src/_lexer.c src/_yml_utils.c src/_allocator_wraper.c -lm
```

> `-lm` is required for `HUGE_VAL` / `NAN` from `<math.h>`.

---

## What is not supported

- **Non-string keys** — mapping keys are always strings; `42: value` or `[1,2]: v` are not supported
- **Custom tags** — `!!python/object`, `!mytag` and any tag other than the built-in Core Schema tags are ignored
- **Directives** — `%YAML` and `%TAG` are not processed
- **YAML 1.1 special types** — `!!set`, `!!omap`, `!!pairs`, `!!binary`, `!!timestamp`
- **Tab indentation** — YAML forbids it; the parser expects spaces only
- **Explicit keys** — block keys `? key\n: value` are not supported
