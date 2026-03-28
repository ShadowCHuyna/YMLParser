# YMLParser

Однофайловый YAML 1.2.2 парсер.
Возвращает дерево `YMLValue*`, которое можно обходить через простой API.

---

## Оглавление

1. [Быстрый старт](#быстрый-старт)
2. [Сборка](#сборка)
   - [Исполняемые файлы и тесты](#исполняемые-файлы-и-тесты)
   - [Статическая библиотека (.a / .lib)](#статическая-библиотека)
   - [Динамическая библиотека (.so / .dll)](#динамическая-библиотека)
3. [Подключение в проект](#подключение-в-проект)
   - [Single-header](#single-header)
4. [API](#api)
   - [Типы](#типы)
   - [YMLParse](#ymlparse)
   - [YMLParseStream](#ymlparsestream)
   - [YMLDestroy / YMLDestroyStream](#ymldestroy--ymldestroystream)
   - [YMLMapGet](#ymlmapget)
   - [YMLMapForech](#ymlmapforech)
   - [ArrayLen](#arraylen)
   - [YMLErrorPrint](#ymlerrorprint)
5. [Обработка ошибок](#обработка-ошибок)
6. [Что поддерживается](#что-поддерживается)
7. [Что не поддерживается](#что-не-поддерживается)

---

## Быстрый старт

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
	for (size_t i = 0; i < ArrayLen(tags->value.array); i++)
		printf("tag: %s\n", tags->value.array[i].value.string);

	YMLMapForech(root->value.object, key, val)
		printf("key: %s\n", key);

	YMLDestroy(root);
	return 0;
}
```
```sh
gcc -std=c23 -o my_app my_app.c -lm
```

---

## Сборка

### Исполняемые файлы и тесты

```sh
make              # собрать все примеры из examples/
make test         # собрать и запустить все тесты

make run-test T=test_scalars      # запустить один тест
make run-example E=example_full   # запустить конкретный пример
```

Компиляция вручную:

```sh
gcc -std=c11 -Isrc -D_POSIX_C_SOURCE=200809L \
    -o my_app my_app.c src/YMLParser.c src/_da.c src/_hm.c src/_lexer.c \
    -lm
```

> `-D_POSIX_C_SOURCE=200809L` — нужен для `strdup`.
> `-lm` — нужен для `HUGE_VAL` / `NAN`.

### Статическая библиотека

```sh
make lib-static                    # Linux  → build/libYMLParser.a
make lib-static PLATFORM=windows   # Windows → build/YMLParser.lib
```

### Динамическая библиотека

```sh
make lib-shared                    # Linux  → build/libYMLParser.so
make lib-shared PLATFORM=windows   # Windows → build/YMLParser.dll
```

Для сборки под Windows из Linux использовать кросс-компилятор:

```sh
make lib-shared PLATFORM=windows CC=x86_64-w64-mingw32-gcc
```

---

## Подключение в проект

**Статическая линковка (Linux):**

```sh
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc \
    my_app.c -Lbuild -lYMLParser -lm
```

**Статическая линковка (Windows / MinGW):**

```sh
gcc -std=c11 -Isrc my_app.c build/YMLParser.lib -lm
```

**Динамическая линковка (Linux):**

```sh
gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc \
    my_app.c -Lbuild -lYMLParser -lm -Wl,-rpath,'$ORIGIN'
```

Единственный публичный заголовок — `src/YMLParser.h`.

### Single-header

Альтернативный вариант — файл `YMLParser.h` в корне репозитория, содержащий и API, и реализацию в одном заголовке. Никаких дополнительных `.c` файлов и флагов компилятора не требуется.

В **одном** файле проекта перед включением определить макрос реализации:

```c
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"
```

Во всех остальных файлах — просто `#include "YMLParser.h"` без макроса.

Компиляция:

```sh
gcc -std=c23 -o my_app my_app.c -lm
```

---

## API

### Типы

```c
typedef enum {
    YML_ANY    = -1,  // только для .type в YMLMapGet (не проверять тип)
    YML_NULL   =  0,
    YML_BOOL,         // value.boolean  (bool)
    YML_INT,          // value.integer  (int64_t) — dec / 0xFF / 0o17
    YML_FLOAT,        // value.number   (double)  — 3.14 / .inf / .nan
    YML_STRING,       // value.string   (const char*, владеет память)
    YML_ARRAY,        // value.array    (YMLValue*, da, см. ArrayLen)
    YML_OBJECT,       // value.object   (void*, доступ через YMLMapGet/YMLMapForech)
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

Разбирает один YAML-документ. При наличии нескольких `---` — парсит только первый.

| Опция | Тип | По умолчанию | Описание |
|---|---|---|---|
| `.ok` | `int*` | `NULL` | Код результата: `0` — успех, `1` — синтакс. ошибка, `2` — OOM |
| `.error` | `char**` | `NULL` | Текст ошибки (указатель на внутренний буфер) |

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

Разбирает YAML-поток из нескольких документов, разделённых `---`.
Возвращает `da<YMLValue*>` — массив корневых узлов.

```c
YMLValue **docs = YMLParseStream("---\nfoo: 1\n---\nbar: 2\n", .ok=&ok);
for (size_t i = 0; i < ArrayLen(docs); i++) { /* docs[i] */ }
YMLDestroyStream(docs);
```

---

### YMLDestroy / YMLDestroyStream

```c
void YMLDestroy(YMLValue *root);
void YMLDestroyStream(YMLValue **stream);
```

Рекурсивно освобождают всю память дерева. Безопасны для `NULL`.
После вызова все `YMLValue*` и `const char*` из этого дерева становятся недействительными.

---

### YMLMapGet

```c
YMLValue *YMLMapGet(void *object, const char *key, ...options...);
```

Возвращает значение по строковому ключу из `YML_OBJECT`.
`object` — это `root->value.object`.

| Опция | Тип | По умолчанию | Описание |
|---|---|---|---|
| `.ok` | `int*` | `NULL` | `0` — найдено, `1` — ключ не найден, `2` — тип не совпадает |
| `.error` | `char**` | `NULL` | Текст ошибки |
| `.type` | `YMLValueType` | `YML_ANY` | Ожидаемый тип; при несовпадении — `ok=2`, возврат `NULL` |

```c
// найти без проверки типа
YMLValue *v = YMLMapGet(root->value.object, "name");

// с проверкой типа и кодом ошибки
YMLValue *n = YMLMapGet(root->value.object, "age", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* ключ не найден или неверный тип */ }
```

Если `.ok` не передан — ошибка попадает в глобальное состояние, доступное через `YMLErrorPrint()`.

---

### YMLMapForech

```c
YMLMapForech(object, key_name, val_name) { ... }
```

Итерация по всем парам ключ–значение `YML_OBJECT`.
`key_name` и `val_name` — имена переменных, которые объявляются внутри макроса
как `const char *key_name` и `YMLValue *val_name`.

```c
YMLMapForech(root->value.object, key, val) {
    printf("%s: type=%d\n", key, val->type);
}
```

Порядок итерации не определён (hash map с открытой адресацией).

---

### ArrayLen

```c
size_t ArrayLen(YMLValue *array);
```

Возвращает количество элементов в `YML_ARRAY`.

```c
YMLValue *arr = YMLMapGet(root->value.object, "items");
for (size_t i = 0; i < ArrayLen(arr->value.array); i++)
    printf("[%zu] = %lld\n", i, (long long)arr->value.array[i].value.integer);
```

---

### YMLErrorPrint

```c
int YMLErrorPrint(void);
```

Если последняя операция завершилась ошибкой (и `.ok` не был передан) — печатает сообщение в `stderr` и возвращает код ошибки. Иначе возвращает `0`.

```c
YMLMapGet(root->value.object, "missing_key");
if (YMLErrorPrint() != 0) { /* ... */ }
```

---

## Обработка ошибок

Все функции API принимают необязательные именованные аргументы `.ok` и `.error` — это реализовано через макрос поверх C99 designated initializers и `__VA_ARGS__`. С точки зрения синтаксиса выглядит как keyword arguments, хотя в C их нет.

```c
YMLValue *v = YMLParse("x: 1\n", .ok=&ok, .error=&err);
```

### Два режима работы

**1. С явной проверкой** — передать `.ok` и проверять после каждого вызова:

```c
int ok = 0;
char *err = NULL;

YMLValue *root = YMLParse(yml, .ok=&ok, .error=&err);
if (ok != 0) { fprintf(stderr, "%d: %s\n", ok, err); return ok; }

YMLValue *port = YMLMapGet(root->value.object, "port", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* ключ не найден или неверный тип */ }
```

**2. Через глобальное состояние** — не передавать `.ok`, проверять через `YMLErrorPrint()`:

```c
YMLValue *root = YMLParse(yml);
YMLValue *host = YMLMapGet(root->value.object, "host");
YMLValue *port = YMLMapGet(root->value.object, "port");

if (YMLErrorPrint() != 0) return 1;  // покажет последнюю ошибку в stderr
```

Каждый вызов сбрасывает глобальное состояние. `YMLErrorPrint()` возвращает код последней ошибки (или `0` если её нет) и при наличии ошибки печатает текст в `stderr`.

### Коды ошибок

| Код | Значение |
|-----|----------|
| `0` | Успех |
| `1` | Ключ не найден / синтаксическая ошибка |
| `2` | Тип значения не совпадает с ожидаемым / OOM |

---

## Что поддерживается

- **Скалярные типы** (Core Schema): `null` / `~`, `bool` (`true`/`false`), `int` (десятичные, `0xFF`, `0o17`), `float` (`.inf`, `-.inf`, `.nan`, научная запись), строки
- **Блочные коллекции**: mapping (`key: value`) и sequence (`- item`) с произвольной вложенностью
- **Потоковые коллекции**: `{key: value}` и `[item, item]`, в том числе вложенные
- **Кавычки**: одинарные `'...'`, двойные `"..."` (с escape-последовательностями), блочные `|` (literal) и `>` (folded) со chomping (`|+`, `|-`, `>+`, `>-`)
- **Якоря и алиасы**: `&anchor` / `*alias`, алиасы являются независимой deep copy
- **Merge key**: `<<: *anchor` — слияние полей из другого объекта (без перезаписи существующих ключей)
- **Встроенные теги**: `!!str`, `!!int`, `!!float`, `!!bool`, `!!null`
- **Многодокументные потоки**: `YMLParseStream` с разделителем `---`
- **Комментарии**: `# ...`

---

## Что не поддерживается

- **Нестроковые ключи** — ключи в mapping всегда строки; `42: value`, `[1,2]: v` не поддерживаются
- **Кастомные теги** — `!!python/object`, `!mytag` и любые теги кроме встроенных игнорируются
- **Директивы** — `%YAML`, `%TAG` не обрабатываются
- **Маркер конца документа** — `...` не поддерживается
- **Специальные типы YAML 1.1** — `!!set`, `!!omap`, `!!pairs`, `!!binary`, `!!timestamp`
- **Табуляция как отступ** — YAML запрещает это; парсер ожидает только пробелы
- **Входные кодировки** — только UTF-8 / ASCII; UTF-16 и UTF-32 не поддерживаются
- **Explicit keys** — блочные ключи `? key\n: value` не поддерживаются
