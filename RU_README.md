# YMLParser

Однофайловый YAML 1.2.2 парсер на C11. Скопируй `YMLParser.h` в проект, один раз определи `YMLPARSER_IMPLEMENTATION` — и получишь полное дерево разбора `YMLValue*`. Зависимостей нет, кроме стандартной библиотеки C и `-lm`.

---

## Оглавление

1. [Быстрый старт](#быстрый-старт)
2. [API](#api)
   - [Типы](#типы)
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
3. [Обработка ошибок](#обработка-ошибок)
4. [Кастомный аллокатор](#кастомный-аллокатор)
5. [Сборка](#сборка)
6. [Что не поддерживается](#что-не-поддерживается)

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

Больше примеров — в директории [`examples/`](examples/).

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
    YML_ARRAY,        // value.array    (YMLValue*, da, см. YMLArrayLen)
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
|-------|-----|--------------|----------|
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

Разбирает YAML-поток из нескольких документов, разделённых `---`. Возвращает `da<YMLValue*>` — массив корневых узлов. Длину массива получить через `YMLArrayLen`. Якоря не переживают границу документа (`---` / `...`).

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

Рекурсивно освобождают всю память дерева. Безопасны для `NULL`. После вызова все `YMLValue*` и `const char*` из этого дерева становятся недействительными.

---

### YMLMapGet

```c
YMLValue *YMLMapGet(void *object, const char *key, ...options...);
```

Возвращает значение по строковому ключу из `YML_OBJECT`. `object` — это `root->value.object`.

| Опция | Тип | По умолчанию | Описание |
|-------|-----|--------------|----------|
| `.ok` | `int*` | `NULL` | `0` — найдено, `1` — ключ не найден, `2` — тип не совпадает |
| `.error` | `char**` | `NULL` | Текст ошибки |
| `.type` | `YMLValueType` | `YML_ANY` | Ожидаемый тип; при несовпадении — `ok=2`, возврат `NULL` |
| `.splitter` | `char` | `0` | Если не ноль — разбивает `key` по этому символу и обходит вложенные объекты |

```c
// без проверки типа
YMLValue *v = YMLMapGet(root->value.object, "name");

// с проверкой типа и кодом ошибки
YMLValue *n = YMLMapGet(root->value.object, "age", .type=YML_INT, .ok=&ok);
if (ok != 0) { /* ключ не найден или неверный тип */ }

// обход по пути — эквивалентно двум вложенным вызовам YMLMapGet
YMLValue *city = YMLMapGet(root->value.object, "address.city", .splitter='.');
```

Если `.ok` не передан — ошибка попадает в глобальное состояние, доступное через `YMLErrorPrint()`.

#### Обход по пути через `.splitter`

При установленном `.splitter` ключ разбивается по указанному символу и выполняется автоматический обход вложенных объектов:

```c
// YAML:
// server:
//   host: localhost
//   port: 8080

YMLValue *host = YMLMapGet(root->value.object, "server.host", .splitter='.');
YMLValue *port = YMLMapGet(root->value.object, "server.port", .splitter='.', .type=YML_INT);
```

В качестве разделителя можно использовать любой символ:

```c
YMLMapGet(root->value.object, "server/host", .splitter='/');
```

Коды ошибок при использовании `.splitter`:
- `ok=1` — ключ на каком-либо уровне не найден, или промежуточный узел не является объектом
- `ok=2` — тип последнего ключа не совпадает с `.type`

> Ключи, буквально содержащие символ-разделитель, не поддерживаются при использовании `.splitter`. Для таких ключей используйте обычный `YMLMapGet`.

---

### YMLMapGetTyped

```c
TYPE YMLMapGetTyped(void *object, const char *key, YMLValueType TYPE, ...options...);
```

Аналог `YMLMapGet`, но возвращает C-значение напрямую, без обёртки `YMLValue*`. Тип возврата определяется на этапе компиляции через `_Generic` по параметру `TYPE`:

| `TYPE` | Тип возврата |
|--------|-------------|
| `YML_BOOL` | `bool` |
| `YML_INT` | `int64_t` |
| `YML_FLOAT` | `double` |
| `YML_STRING` | `const char*` |
| `YML_ARRAY` | `YMLValue*` |
| `YML_OBJECT` | `void*` |

`.type=TYPE` выставляется автоматически. Опциональные аргументы (`.ok`, `.error`, `.splitter`) передаются в `YMLMapGet` как есть.

> **Внимание:** если ключ не найден или тип не совпадает, `YMLMapGet` вернёт `YMLVallue` `{.type=YML_NULL, .value=NULL}`

```c
int ok;

int64_t     age  = YMLMapGetTyped(root->value.object, "age",   YML_INT,    .ok=&ok);
const char *name = YMLMapGetTyped(root->value.object, "name",  YML_STRING, .ok=&ok);
double      x    = YMLMapGetTyped(root->value.object, "score", YML_FLOAT);

// работает и с обходом по пути
const char *city = YMLMapGetTyped(root->value.object, "address.city", YML_STRING, .splitter='.');
```

---

### YMLMapForech

```c
YMLMapForech(object, key_name, val_name) { ... }
```

Итерация по всем парам ключ–значение `YML_OBJECT`. `key_name` и `val_name` — имена переменных, которые объявляются внутри макроса как `const char *key_name` и `YMLValue *val_name`.

```c
YMLMapForech(root->value.object, key, val) {
    printf("%s: type=%d\n", key, val->type);
}
```

Порядок итерации не определён (hash map с открытой адресацией).

---

### YMLArrayLen

```c
size_t YMLArrayLen(YMLValue *array);
```

Возвращает количество элементов в `YML_ARRAY`. Для `NULL` безопасно возвращает `0`.

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

Если последняя операция завершилась ошибкой (и `.ok` не был передан) — печатает сообщение в `stderr` и возвращает код ошибки. Иначе возвращает `0`. Каждый вызов сбрасывает глобальное состояние ошибки.

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

Создают пустой `YML_OBJECT` или `YML_ARRAY`. Освобождать через `YMLDestroy`.

```c
YMLValue *obj = YMLCreate();     // пустое отображение
YMLValue *arr = YMLCreateArr();  // пустая последовательность
```

---

### YMLMapAdd / YMLMapAddNull / YMLMapAddArr

```c
YMLMapAdd(obj, key, val);
YMLMapAddNull(obj, key);
YMLMapAddArr(obj, key, c_array, len);
```

Добавляют пару ключ–значение в `YML_OBJECT`. Тип `val` выводится через `_Generic`.

| Вызов | Тип |
|-------|-----|
| `YMLMapAdd(obj, "n", (long long)42)` | `YML_INT` |
| `YMLMapAdd(obj, "f", 3.14)` | `YML_FLOAT` |
| `YMLMapAdd(obj, "s", "hello")` | `YML_STRING` (strdup'd) |
| `YMLMapAdd(obj, "b", (bool)true)` | `YML_BOOL` |
| `YMLMapAdd(obj, "sub", node)` | deep-копия `YMLValue*` |
| `YMLMapAddNull(obj, "k")` | `YML_NULL` |

При дублировании ключа старое значение освобождается. Вложенный `YMLValue*` глубоко копируется, поэтому оригинал можно уничтожить сразу после добавления.

`YMLMapAddArr` конвертирует C-массив в дочерний `YML_ARRAY`. Поддерживаются `long long[]`, `double[]`, `const char*[]`.

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

Добавляют элемент в `YML_ARRAY`. Те же правила вывода типа, что у `YMLMapAdd`. Вложенный `YMLValue*` глубоко копируется.

```c
YMLValue *arr = YMLCreateArr();
YMLArrPush(arr, (long long)1);
YMLArrPush(arr, 2.5);
YMLArrPush(arr, "three");
YMLArrPush(arr, (bool)false);
YMLArrPushNull(arr);
```

`YMLArrPushArr` добавляет C-массив как вложенный `YML_ARRAY`-элемент.

---

### YMLWriteStream / YMLWriteBuf

```c
void YMLWriteStream(YMLValue *obj, FILE *stream, ...options...);
int  YMLWriteBuf   (YMLValue *obj, char *buf, size_t cap, ...options...);
```

Сериализуют дерево `YMLValue` в YAML-текст.

`YMLWriteStream` пишет в любой `FILE*`. `YMLWriteBuf` пишет в буфер, переданный вызывающим, и возвращает количество записанных байт (без NUL-терминатора) или `-1` при ошибке.

| Опция | Тип | По умолчанию | Описание |
|-------|-----|--------------|----------|
| `.indent` | `int` | `2` | Пробелов на уровень вложенности |
| `.start` | `int` | `0` | Добавить маркер `---` в начало |
| `.ok` | `int*` | `NULL` | `0` — успех, `1` — буфер мал, `2` — OOM |
| `.error` | `char**` | `NULL` | Текст ошибки |

```c
// в stdout
YMLWriteStream(obj, stdout);
YMLWriteStream(obj, stdout, .indent=4, .start=1);

// в буфер
char buf[1024];
int ok = 0;
int n = YMLWriteBuf(obj, buf, sizeof(buf), .ok=&ok);
if (ok != 0) fprintf(stderr, "write error\n");
```

Пример round-trip:

```c
YMLValue *obj = YMLCreate();
YMLMapAdd(obj, "x", (long long)1);

char buf[256];
YMLWriteBuf(obj, buf, sizeof(buf));
YMLDestroy(obj);

YMLValue *root = YMLParse(buf);
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

if (YMLErrorPrint() != 0) return 1;
```

Состояние ошибки — `_Thread_local`: каждый поток видит только свои ошибки.

### Коды ошибок

| Код | Значение |
|-----|----------|
| `0` | Успех |
| `1` | Ключ не найден / синтаксическая ошибка |
| `2` | Тип значения не совпадает с ожидаемым / OOM |

---

## Кастомный аллокатор

По умолчанию все операции с кучей (`malloc`, `realloc`, `calloc`, `free`) идут через стандартную библиотеку C. Заменить их можно глобально через `YMLParserSetAllocator` до первого вызова функций парсера:

```c
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

static void *my_alloc  (size_t len,              void *ctx, const char *file, int line);
static void *my_realloc(void *ptr, size_t len,   void *ctx, const char *file, int line);
static void *my_calloc (size_t n,  size_t size,  void *ctx, const char *file, int line);
static void  my_free   (void *ptr,               void *ctx, const char *file, int line);

// вызвать до любого YMLParse / YMLCreate
YMLParserSetAllocator((struct _YMLParserAllocator){
    .alloc   = my_alloc,
    .realloc = my_realloc,
    .calloc  = my_calloc,
    .dealloc = my_free,
    .ctx     = NULL,   // передаётся в каждый вызов; подходит для arena-указателя и т.п.
});
```

Структура и функция объявлены в `YMLParser.h`:

```c
struct YMLParserAllocator {
    void* (*alloc)  (size_t len,               void *ctx, const char *file, int line);
    void* (*realloc)(void *ptr, size_t new_len, void *ctx, const char *file, int line);
    void* (*calloc) (size_t n,  size_t size,   void *ctx, const char *file, int line);
    void  (*dealloc)(void *ptr,                void *ctx, const char *file, int line);
    void *ctx;
};

void YMLParserSetAllocator(struct YMLParserAllocator allocator);
```

`file` и `line` — это `__FILE__` / `__LINE__` на месте вызова, полезны для диагностики и отслеживания утечек. Если не нужны — просто игнорировать.

> Аллокатор — обычная глобальная переменная, не `_Thread_local`. При многопоточном использовании — установить один раз при старте, до создания потоков.

---

## Сборка

### Single-header (рекомендуется)

Скопировать `YMLParser.h` из корня репозитория в проект. Никаких дополнительных файлов не нужно.

```sh
gcc -std=c11 -o my_app my_app.c -lm
```

Пересобрать `YMLParser.h` из `src/` после изменений:

```sh
make              # запускает tools/amalgamate.py → YMLParser.h
```

### Multi-file (для разработки библиотеки)

```sh
make test                          # собрать и запустить все тесты
make run-test T=test_scalars       # запустить один тест
make run-example E=example_nested  # запустить конкретный пример

make lib-static                    # Linux  → build/libYMLParser.a
make lib-static PLATFORM=windows   # Windows → build/YMLParser.lib
make lib-shared                    # Linux  → build/libYMLParser.so
make lib-shared PLATFORM=windows   # Windows → build/YMLParser.dll
```

Компиляция вручную через `src/`:

```sh
gcc -std=c11 -Isrc -o my_app my_app.c \
    src/YMLParser.c src/YMLWriter.c src/_da.c src/_hm.c \
    src/_lexer.c src/_yml_utils.c src/_allocator.c -lm
```

> `-lm` обязателен — нужен `HUGE_VAL` / `NAN` из `<math.h>`.

---

## Что не поддерживается

- **Нестроковые ключи** — ключи в mapping всегда строки; `42: value`, `[1,2]: v` не поддерживаются
- **Кастомные теги** — `!!python/object`, `!mytag` и любые теги кроме встроенных игнорируются
- **Директивы** — `%YAML`, `%TAG` не обрабатываются
- **Специальные типы YAML 1.1** — `!!set`, `!!omap`, `!!pairs`, `!!binary`, `!!timestamp`
- **Табуляция как отступ** — YAML запрещает это; парсер ожидает только пробелы
- **Explicit keys** — блочные ключи `? key\n: value` не поддерживаются
