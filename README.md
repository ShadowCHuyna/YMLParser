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
   - [YMLMapForech](#ymlmapforech)
   - [YMLArrayLen](#YMLArrayLen)
   - [YMLErrorPrint](#ymlerrorprint)
3. [Error handling](#error-handling)
4. [Build](#build)
5. [What is not supported](#what-is-not-supported)

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
    if (YMLErrorPrint() != 0) return 1;

    YMLValue *name = YMLMapGet(root->value.object, "name");
    YMLValue *age  = YMLMapGet(root->value.object, "age");
    printf("name=%s  age=%lld\n", name->value.string, (long long)age->value.integer);

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

```c
// without type check
YMLValue *v = YMLMapGet(root->value.object, "name");

// with type check and error code
YMLValue *n = YMLMapGet(root->value.object, "age", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* key not found or wrong type */ }
```

If `.ok` is not passed, errors go to the global state accessible via `YMLErrorPrint()`.

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

### YMLErrorPrint

```c
int YMLErrorPrint(void);
```

If the last operation produced an error (and `.ok` was not passed), prints the message to `stderr` and returns the error code. Otherwise returns `0`. Each call resets the global error state.

```c
YMLMapGet(root->value.object, "missing_key");
if (YMLErrorPrint() != 0) { /* ... */ }
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
    src/YMLParser.c src/_da.c src/_hm.c src/_lexer.c -lm
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
