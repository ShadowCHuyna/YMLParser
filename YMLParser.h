#ifndef YMLParser_H
#define YMLParser_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * YMLValueType — тип хранимого значения.
 *
 *   YML_NULL    нет полезной нагрузки
 *   YML_BOOL    bool
 *   YML_INT     int64_t  (целые: 42, 0xFF, 0o17)
 *   YML_FLOAT   double   (вещественные: 3.14, .inf, .nan)
 *   YML_STRING  const char* (heap, NUL-terminated, владеет YMLValue)
 *   YML_OBJECT  hm<hm_str, YMLValue>  — доступ через YMLMapGet / YMLMapForech
 *   YML_ARRAY   da<YMLValue>          — индексация value.array[i], длина через ArrayLen
 *
 * YML_ANY — сентинел только для _YMLOptionals.type (значит «не проверять тип»).
 *           Не является корректным типом узла.
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
 * YMLValue — одно значение дерева разбора.
 *
 * Поле type определяет, какой элемент union value активен:
 *   YML_BOOL   → value.boolean
 *   YML_INT    → value.integer
 *   YML_FLOAT  → value.number
 *   YML_STRING → value.string  (указатель на строку, NUL-terminated)
 *   YML_OBJECT → value.object  (hm<str → YMLValue>)
 *   YML_ARRAY  → value.array   (da<YMLValue>, скрытый заголовок, см. ArrayLen)
 *   YML_NULL   → value не используется
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
		void *object;			// _hm* — непрозрачный указатель, доступ через YMLMapGet/YMLMapForech
		struct YMLValue *array; // da<YMLValue> — прямая индексация arr[i], длина через ArrayLen
	} value;
} YMLValue;

/*
 * Скрытый заголовок динамических массивов da<T>.
 * Расположен непосредственно перед первым элементом в памяти:
 *   [_da_header][elem0][elem1]...
 * Пользователь держит указатель на elem0.
 * Все da освобождаются при YMLDestroy / YMLDestroyStream —
 * если нужны данные после уничтожения дерева, скопируй их заранее.
 */
typedef struct
{
	size_t len;
	size_t cap;
} _da_header; /* должен совпадать с _da_hdr из _da.h */

/*
 * Возвращает количество элементов в da-массиве.
 * Пример: for (size_t i = 0; i < ArrayLen(arr); i++) { ... arr[i] ... }
 */
#define ArrayLen(arr) (((_da_header *)(arr) - 1)->len)

struct _YMLOptionals
{
	int *ok;		   // 0 — без ошибок, иначе код ошибки
	char **error;	   // *error указывает на буфер с текстом ошибки
	YMLValueType type; // для YMLMapGet: ожидаемый тип (YML_ANY — не проверять)
};

/*
 * Разбирает один YAML-документ, возвращает корневой YMLValue*.
 * Если в строке несколько документов (---) — парсит только первый.
 *
 * Коды ошибок ok:
 *   1 — синтаксическая ошибка
 *   2 — недостаточно памяти
 */
YMLValue *_YMLParse(const char *yml_str, struct _YMLOptionals optionals);

#define YMLParse(yml_str, ...) \
	_YMLParse(yml_str, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Разбирает YAML-поток с несколькими документами (разделитель ---).
 * Возвращает da<YMLValue*> — массив корневых узлов каждого документа.
 * Длину массива получить через ArrayLen.
 *
 * Пример:
 *   YMLValue **docs = YMLParseStream(yml_str);
 *   for (size_t i = 0; i < ArrayLen(docs); i++) { ... docs[i] ... }
 *   YMLDestroyStream(docs);
 *
 * Коды ошибок ok: те же что у YMLParse.
 */
YMLValue **_YMLParseStream(const char *yml_str, struct _YMLOptionals optionals);

#define YMLParseStream(yml_str, ...) \
	_YMLParseStream(yml_str, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Освобождает память дерева одного документа (YMLParse).
 */
void _YMLDestroy(YMLValue *root, struct _YMLOptionals optionals);

#define YMLDestroy(root, ...) \
	_YMLDestroy(root, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Освобождает все документы потока и сам da (YMLParseStream).
 */
void _YMLDestroyStream(YMLValue **stream, struct _YMLOptionals optionals);

#define YMLDestroyStream(stream, ...) \
	_YMLDestroyStream(stream, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Если после последней операции была ошибка — печатает её в stderr
 * и возвращает код ошибки. Иначе возвращает 0.
 */
int YMLErrorPrint(void);

/*
 * Возвращает YMLValue* по строковому ключу из YML_OBJECT.
 * Если ключ не найден — ошибка (ok=1).
 * Если .type != YML_ANY и тип значения не совпадает — ошибка (ok=2).
 *
 * Коды ошибок ok:
 *   1 — ключ не найден
 *   2 — тип не совпадает с ожидаемым
 */
/* hm — указатель YMLValue.value.object (void* = _hm*) */
YMLValue *_YMLMapGet(void *hm, const char *key, struct _YMLOptionals optionals);

#define YMLMapGet(object, key, ...) \
	_YMLMapGet(object, key, (struct _YMLOptionals){.ok = NULL, .error = NULL, .type = YML_ANY, __VA_ARGS__})

/*
 * Итератор по парам ключ-значение YML_OBJECT.
 * Используется внутри макроса YMLMapForech.
 */
/* _hm — непрозрачный указатель на внутреннюю структуру hm, _i — текущий слот. */
typedef struct
{
	void *_hm;
	size_t _i;
} _YMLMapIter;
/* hm — указатель YMLValue.value.object */
_YMLMapIter _YMLMapIterBegin(void *hm);
bool _YMLMapIterNext(_YMLMapIter *iter, const char **key, YMLValue **value);

/*
 * Итерация по всем парам ключ-значение объекта.
 * key_name и val_name — имена переменных, объявляются макросом внутри цикла.
 *
 * Пример:
 *   YMLMapForech(root->value.object, key, val) {
 *       printf("%s\n", key);
 *   }
 */
#define YMLMapForech(object, key_name, val_name)                                                         \
	for (_YMLMapIter _yml_it_ = _YMLMapIterBegin(object), *_yml_p_ = &_yml_it_; _yml_p_; _yml_p_ = NULL) \
		for (const char *(key_name) = NULL; _yml_p_; _yml_p_ = NULL)                                     \
			for (YMLValue * (val_name) = NULL; _YMLMapIterNext(&_yml_it_, (const char **)&(key_name), &(val_name));)


#ifdef YMLPARSER_IMPLEMENTATION

/*
 * _da.h — dynamic array со скрытым заголовком.
 *
 * Компоновка памяти:
 *   [_da_hdr | elem0 | elem1 | ...]
 *                ^
 *                пользователю возвращается указатель сюда
 *
 * _da_hdr содержит len и cap — пользователь видит только len через ArrayLen().
 * Массив нельзя изменять снаружи — только читать через [i] и ArrayLen.
 * Освобождение: _da_free(da).
 */

#include <stddef.h>

typedef struct
{
	size_t len;
	size_t cap;
} _da_hdr;

/* Выделить новый da для элементов размером elem_size. */
void *_da_new(size_t elem_size, size_t cap);

/*
 * Добавить элемент (elem_size байт) в конец da.
 * Возвращает новый указатель на начало массива (может переаллоцироваться).
 * Вызывающий обязан обновить свой указатель.
 */
void *_da_push(void *da, const void *elem, size_t elem_size);

/* Освободить da вместе со скрытым заголовком. */
void _da_free(void *da);

/*
 * Типизированные макросы.
 *
 * da_new(T, cap)      — выделить новый da<T> с ёмкостью cap
 * da_push(da, val)    — добавить val в конец, обновить da
 * da_len(da)          — длина (то же что ArrayLen)
 * da_free(da)         — освободить
 *
 * Пример:
 *   int *arr = da_new(int, 8);
 *   da_push(arr, 42);
 *   for (size_t i = 0; i < da_len(arr); i++) printf("%d\n", arr[i]);
 *   da_free(arr);
 */
#define da_new(T, cap) ((T *)_da_new(sizeof(T), (cap)))
#define da_len(da) (((_da_hdr *)(da) - 1)->len)
#define da_push(da, val)                        \
	do                                          \
	{                                           \
		__typeof__(val) _v = (val);             \
		(da) = _da_push((da), &_v, sizeof(_v)); \
	} while (0)
#define da_free(da) _da_free(da)

#include <stdlib.h>
#include <string.h>

void *_da_new(size_t elem_size, size_t cap)
{
	if (cap == 0)
		cap = 4;
	_da_hdr *hdr = malloc(sizeof(_da_hdr) + elem_size * cap);
	if (!hdr)
		return NULL;
	hdr->len = 0;
	hdr->cap = cap;
	return hdr + 1;
}

void *_da_push(void *da, const void *elem, size_t elem_size)
{
	_da_hdr *hdr = (_da_hdr *)da - 1;
	if (hdr->len == hdr->cap)
	{
		hdr->cap *= 2;
		hdr = realloc(hdr, sizeof(_da_hdr) + elem_size * hdr->cap);
		if (!hdr)
			return da; /* TODO: propagate OOM */
	}
	memcpy((char *)(hdr + 1) + elem_size * hdr->len, elem, elem_size);
	hdr->len++;
	return hdr + 1;
}

void _da_free(void *da)
{
	if (!da)
		return;
	free((_da_hdr *)da - 1);
}

/*
 * _hm.h — hash map: строковый ключ → YMLValue.
 *
 * Реализация: открытая адресация с линейным зондированием.
 * Ключи копируются через strdup и освобождаются при hm_free.
 * Значения хранятся по значению (YMLValue).
 *
 * Используется как тип поля YMLValue.value.object (через void*).
 */

#include "YMLParser.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
	char *key; /* strdup, NULL = пустой слот */
	YMLValue value;
} _hm_entry;

typedef struct
{
	_hm_entry *entries; /* malloc-массив слотов (не da — управляется вручную) */
	size_t cap;			/* количество слотов (степень двойки) */
	size_t len;			/* количество занятых слотов */
} _hm;

/* Выделить новую hm. cap — начальное количество слотов (округляется до степени 2). */
_hm *hm_new(size_t cap);

/*
 * Вставить или обновить пару key → value.
 * key копируется через strdup.
 * Возвращает false при ошибке памяти.
 */
bool hm_set(_hm *hm, const char *key, YMLValue value);

/*
 * Найти значение по ключу.
 * Возвращает указатель на YMLValue внутри hm (валиден до следующего hm_set/hm_free).
 * Возвращает NULL если ключ не найден.
 */
YMLValue *hm_get(const _hm *hm, const char *key);

/*
 * Итерация: idx — текущий индекс (начинать с 0).
 * Возвращает true и записывает key/value если нашёл следующий занятый слот.
 * При возврате false idx указывает за конец — итерация завершена.
 *
 * Пример:
 *   size_t idx = 0;
 *   const char *key; YMLValue *val;
 *   while (hm_next(hm, &idx, &key, &val)) { ... }
 */
bool hm_next(const _hm *hm, size_t *idx, const char **key, YMLValue **value);

/*
 * Рекурсивно освобождает все YMLValue внутри hm (строки, вложенные hm и da),
 * затем освобождает ключи и саму hm.
 */
void hm_free(_hm *hm);

#include <stdlib.h>
#include <string.h>

/* djb2 hash */
static size_t hash_str(const char *s)
{
	size_t h = 5381;
	while (*s)
		h = h * 33 ^ (unsigned char)*s++;
	return h;
}

/* Округлить cap до ближайшей степени двойки >= cap */
static size_t next_pow2(size_t n)
{
	if (n == 0)
		return 4;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	return n + 1;
}

_hm *hm_new(size_t cap)
{
	_hm *hm = malloc(sizeof(_hm));
	if (!hm)
		return NULL;
	cap = next_pow2(cap < 4 ? 4 : cap);
	hm->entries = calloc(cap, sizeof(_hm_entry));
	if (!hm->entries)
	{
		free(hm);
		return NULL;
	}
	hm->cap = cap;
	hm->len = 0;
	return hm;
}

/* Перехеширование при load factor > 0.7 */
static bool hm_grow(_hm *hm)
{
	size_t new_cap = hm->cap * 2;
	_hm_entry *new_entries = calloc(new_cap, sizeof(_hm_entry));
	if (!new_entries)
		return false;
	for (size_t i = 0; i < hm->cap; i++)
	{
		if (!hm->entries[i].key)
			continue;
		size_t idx = hash_str(hm->entries[i].key) & (new_cap - 1);
		while (new_entries[idx].key)
			idx = (idx + 1) & (new_cap - 1);
		new_entries[idx] = hm->entries[i];
	}
	free(hm->entries);
	hm->entries = new_entries;
	hm->cap = new_cap;
	return true;
}

bool hm_set(_hm *hm, const char *key, YMLValue value)
{
	if (hm->len * 10 >= hm->cap * 7) /* load > 0.7 */
		if (!hm_grow(hm))
			return false;

	size_t idx = hash_str(key) & (hm->cap - 1);
	while (hm->entries[idx].key)
	{
		if (strcmp(hm->entries[idx].key, key) == 0)
		{
			hm->entries[idx].value = value; /* обновить */
			return true;
		}
		idx = (idx + 1) & (hm->cap - 1);
	}
	hm->entries[idx].key = strdup(key);
	if (!hm->entries[idx].key)
		return false;
	hm->entries[idx].value = value;
	hm->len++;
	return true;
}

YMLValue *hm_get(const _hm *hm, const char *key)
{
	size_t idx = hash_str(key) & (hm->cap - 1);
	while (hm->entries[idx].key)
	{
		if (strcmp(hm->entries[idx].key, key) == 0)
			return &hm->entries[idx].value;
		idx = (idx + 1) & (hm->cap - 1);
	}
	return NULL;
}

bool hm_next(const _hm *hm, size_t *idx, const char **key, YMLValue **value)
{
	while (*idx < hm->cap)
	{
		_hm_entry *e = &hm->entries[(*idx)++];
		if (e->key)
		{
			*key = e->key;
			*value = &e->value;
			return true;
		}
	}
	return false;
}

/* Рекурсивное освобождение значения (без free самого указателя) */
static void free_value(YMLValue *v)
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
			free_value(&v->value.array[i]);
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

void hm_free(_hm *hm)
{
	if (!hm)
		return;
	for (size_t i = 0; i < hm->cap; i++)
	{
		if (!hm->entries[i].key)
			continue;
		free(hm->entries[i].key);
		free_value(&hm->entries[i].value);
	}
	free(hm->entries);
	free(hm);
}

/*
 * _lexer.h — лексический анализатор YAML 1.2.2.
 *
 * Вход:  const char* (NUL-terminated YAML-строка)
 * Выход: da<Token> — массив токенов
 *
 * Лексер делает один проход по входу и производит плоский массив токенов.
 * Парсер затем обходит этот массив рекурсивным спуском.
 *
 * Особенности:
 *  - отслеживает flow_depth (0 = блочный контекст, >0 = внутри [] или {})
 *  - ':' является TK_MAP_VAL только если за ним пробел/перевод/EOF (блочный)
 *    или в flow-контексте (любой ':')
 *  - '-' является TK_SEQ_ENTRY только если за ним пробел/перевод (блочный)
 *  - якоря (&name) и алиасы (*name) разворачиваются в парсере, не в лексере
 *  - теги (!!str, !tag, !<uri>) — сохраняется строка тега
 *  - комментарии (#) и незначимые пробелы пропускаются
 *  - строки TK_SCALAR для блочных (| и >) обрабатываются полностью в лексере
 */

#include <stddef.h>

typedef enum
{
	TK_DOC_START, /* ---                         */
	TK_DOC_END,	  /* ...                         */

	TK_SEQ_ENTRY, /* -<пробел> в блочном контексте */
	TK_MAP_KEY,	  /* ?<пробел> явный ключ         */
	TK_MAP_VAL,	  /* :<пробел|перевод|EOF>        */

	TK_FLOW_SEQ_START, /* [  */
	TK_FLOW_SEQ_END,   /* ]  */
	TK_FLOW_MAP_START, /* {  */
	TK_FLOW_MAP_END,   /* }  */
	TK_FLOW_ENTRY,	   /* ,  */

	TK_SCALAR, /* любой скаляр (plain, '...', "...", |, >) */
	TK_ANCHOR, /* &name  */
	TK_ALIAS,  /* *name  */
	TK_TAG,	   /* !!str / !tag / !<uri> */

	TK_EOF,
} TK_Type;

typedef enum
{
	SCALAR_PLAIN,		  /* foo, 42, true, null         */
	SCALAR_SINGLE_QUOTED, /* 'foo bar'                   */
	SCALAR_DOUBLE_QUOTED, /* "foo\nbar"                  */
	SCALAR_LITERAL,		  /* |  (сохраняет переводы строк) */
	SCALAR_FOLDED,		  /* >  (сворачивает переводы строк) */
} ScalarStyle;

typedef struct
{
	TK_Type type;
	int line; /* 1-based номер строки начала токена */
	int col;  /* 0-based столбец начала токена (= отступ для блочных) */

	/* Заполняется для TK_SCALAR, TK_ANCHOR, TK_ALIAS, TK_TAG.
	 * Указывает в исходную строку — НЕ NUL-terminated!
	 * Длина в value_len. Парсер копирует при необходимости. */
	const char *value;
	size_t value_len;

	ScalarStyle style; /* только для TK_SCALAR */
} Token;

/*
 * Лексировать src и вернуть da<Token>.
 * При синтаксической ошибке:
 *   если error != NULL — записывает сообщение в *error (static буфер)
 *   возвращает NULL
 *
 * Освобождение: da_free() — токены не владеют строками (value указывает в src).
 */
Token *lex(const char *src, const char **error);

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── состояние лексера ─────────────────────────────────────────────── */

typedef struct
{
	const char *src;
	const char *p;	   /* текущая позиция */
	int line;		   /* 1-based */
	int col;		   /* 0-based */
	int flow_depth;	   /* 0 = блочный контекст */
	const char *error; /* static string при ошибке */
} Lexer;

/* ── утилиты ───────────────────────────────────────────────────────── */

static char lc(Lexer *l) { return *l->p; }

static void ladvance(Lexer *l)
{
	if (*l->p == '\n')
	{
		l->line++;
		l->col = 0;
	}
	else
		l->col++;
	l->p++;
}

static void skip_spaces(Lexer *l)
{
	while (*l->p == ' ' || *l->p == '\t')
		ladvance(l);
}

static void skip_line(Lexer *l)
{
	while (*l->p && *l->p != '\n')
		ladvance(l);
	if (*l->p == '\n')
		ladvance(l);
}

/* Пропустить пустые строки и комментарии. */
static void skip_ws(Lexer *l)
{
	for (;;)
	{
		skip_spaces(l);
		if (*l->p == '#')
		{
			skip_line(l);
			continue;
		}
		if (*l->p == '\n')
		{
			ladvance(l);
			continue;
		}
		if (*l->p == '\r' && *(l->p + 1) == '\n')
		{
			ladvance(l);
			ladvance(l);
			continue;
		}
		break;
	}
}

static Token make_tok(Lexer *l, TK_Type type)
{
	return (Token){.type = type, .line = l->line, .col = l->col, .value = NULL, .value_len = 0, .style = SCALAR_PLAIN};
}

static bool is_flow(Lexer *l) { return l->flow_depth > 0; }

/* ':' является MAP_VAL если за ним пробел/\n/\r/EOF или мы в flow */
static bool is_map_val_colon(Lexer *l)
{
	char next = *(l->p + 1);
	return is_flow(l) || next == ' ' || next == '\t' ||
		   next == '\n' || next == '\r' || next == '\0';
}

/* '-' является SEQ_ENTRY если за ним пробел/\n или начало строки */
static bool is_seq_entry_dash(Lexer *l)
{
	char next = *(l->p + 1);
	return next == ' ' || next == '\t' || next == '\n' ||
		   next == '\r' || next == '\0';
}

/* ── скаляры ───────────────────────────────────────────────────────── */

/* Одиночные кавычки: '' = экранированная ' */
static Token lex_single_quoted(Lexer *l)
{
	Token t = make_tok(l, TK_SCALAR);
	t.style = SCALAR_SINGLE_QUOTED;
	ladvance(l); /* skip ' */
	const char *start = l->p;
	while (*l->p)
	{
		if (*l->p == '\'')
		{
			if (*(l->p + 1) == '\'')
			{
				ladvance(l);
				ladvance(l);
				continue;
			}
			break;
		}
		ladvance(l);
	}
	t.value = start;
	t.value_len = (size_t)(l->p - start);
	if (*l->p == '\'')
		ladvance(l);
	return t;
}

/* Двойные кавычки: стандартные escape-последовательности YAML */
static Token lex_double_quoted(Lexer *l)
{
	Token t = make_tok(l, TK_SCALAR);
	t.style = SCALAR_DOUBLE_QUOTED;
	ladvance(l); /* skip " */
	const char *start = l->p;
	while (*l->p && *l->p != '"')
	{
		if (*l->p == '\\')
			ladvance(l); /* skip escape */
		ladvance(l);
	}
	t.value = start;
	t.value_len = (size_t)(l->p - start);
	if (*l->p == '"')
		ladvance(l);
	return t;
}

/*
 * Блочный скаляр: | (literal) или > (folded).
 * Заголовок: [chomping indicator] [indent indicator] — необязательные.
 * chomping: '-' = strip, '+' = keep, по умолчанию = clip (последний \n).
 * indent: цифра — явный отступ, иначе определяется по первой строке.
 * Возвращает токен с value/value_len указывающим на scoped копию текста
 * без leading indent (raw bytes, парсер займётся chomping при построении узла).
 *
 * NB: value здесь НЕ указывает в src, а в отдельный буфер. Освобождение —
 * ответственность парсера после построения YMLValue.
 */
static Token lex_block_scalar(Lexer *l, ScalarStyle style)
{
	Token t = make_tok(l, TK_SCALAR);
	t.style = style;
	ladvance(l); /* skip | or > */

	/* разбор заголовка */
	char chomping = 'c'; /* c=clip, '-'=strip, '+'=keep */
	int explicit_indent = 0;
	while (*l->p == '-' || *l->p == '+' || (*l->p >= '1' && *l->p <= '9'))
	{
		if (*l->p == '-' || *l->p == '+')
			chomping = *l->p;
		else
			explicit_indent = *l->p - '0';
		ladvance(l);
	}
	skip_spaces(l);
	if (*l->p == '#')
		skip_line(l);
	if (*l->p == '\n')
		ladvance(l);

	/* определить базовый отступ */
	int base_indent = explicit_indent;
	if (base_indent == 0)
	{
		const char *pp = l->p;
		while (*pp == ' ')
			pp++;
		base_indent = (int)(pp - l->p);
	}

	/* собрать строки тела */
	size_t buf_cap = 256;
	char *buf = malloc(buf_cap);
	if (!buf)
	{
		l->error = "OOM";
		t.value = "";
		t.value_len = 0;
		return t;
	}
	size_t buf_len = 0;

	while (*l->p)
	{
		/* пустая строка или строка полностью из пробелов */
		const char *line_start = l->p;
		int spaces = 0;
		while (*l->p == ' ')
		{
			ladvance(l);
			spaces++;
		}
		if (*l->p == '\n' || *l->p == '\r' || *l->p == '\0')
		{
			/* пустая строка — добавить \n */
			if (buf_len + 1 >= buf_cap)
			{
				buf_cap *= 2;
				buf = realloc(buf, buf_cap);
				if (!buf)
				{
					l->error = "OOM";
					t.value = "";
					t.value_len = 0;
					return t;
				}
			}
			buf[buf_len++] = '\n';
			if (*l->p == '\r')
				ladvance(l);
			if (*l->p == '\n')
				ladvance(l);
			continue;
		}
		/* строка с меньшим отступом — конец блока */
		if (spaces < base_indent)
		{
			l->p = line_start;
			l->col -= spaces;
			break;
		}
		/* пропустить base_indent пробелов */
		int extra = spaces - base_indent;
		/* добавить extra leading spaces (для literal) */
		while (buf_len + (size_t)extra + 1 >= buf_cap)
		{
			buf_cap *= 2;
			buf = realloc(buf, buf_cap);
			if (!buf)
			{
				l->error = "OOM";
				t.value = "";
				t.value_len = 0;
				return t;
			}
		}
		for (int i = 0; i < extra; i++)
			buf[buf_len++] = ' ';

		/* содержимое строки */
		while (*l->p && *l->p != '\n' && *l->p != '\r')
		{
			if (buf_len + 1 >= buf_cap)
			{
				buf_cap *= 2;
				buf = realloc(buf, buf_cap);
				if (!buf)
				{
					l->error = "OOM";
					t.value = "";
					t.value_len = 0;
					return t;
				}
			}
			buf[buf_len++] = *l->p;
			ladvance(l);
		}
		if (buf_len + 1 >= buf_cap)
		{
			buf_cap *= 2;
			buf = realloc(buf, buf_cap);
			if (!buf)
			{
				l->error = "OOM";
				t.value = "";
				t.value_len = 0;
				return t;
			}
		}
		buf[buf_len++] = '\n';
		if (*l->p == '\r')
			ladvance(l);
		if (*l->p == '\n')
			ladvance(l);
	}

	/* chomping: удалить trailing newlines */
	if (chomping == '-')
	{
		while (buf_len > 0 && buf[buf_len - 1] == '\n')
			buf_len--;
	}
	else if (chomping == 'c')
	{
		/* оставить ровно один \n в конце */
		while (buf_len > 1 && buf[buf_len - 1] == '\n' && buf[buf_len - 2] == '\n')
			buf_len--;
	}
	/* '+' — оставить как есть */

	buf[buf_len] = '\0';
	t.value = buf; /* парсер освобождает этот буфер после strdup в YMLValue */
	t.value_len = buf_len;
	return t;
}

/*
 * Plain scalar: читаем до конца "безопасного" контента.
 * В блочном контексте: стоп на ': ', ' #', ',', '[', ']', '{', '}', '\n'
 * В flow-контексте: стоп также на ',' / ':' / ']' / '}'
 */
static Token lex_plain(Lexer *l)
{
	Token t = make_tok(l, TK_SCALAR);
	t.style = SCALAR_PLAIN;
	const char *start = l->p;
	while (*l->p)
	{
		char c = *l->p;
		char n = *(l->p + 1);
		if (c == '\n' || c == '\r')
			break;
		if (c == '#' && (l->p > start) && *(l->p - 1) == ' ')
			break;
		if (is_flow(l) && (c == ',' || c == ']' || c == '}'))
			break;
		if (c == ':' && (n == ' ' || n == '\t' || n == '\n' || n == '\r' || n == '\0'))
			break;
		if (c == ':' && is_flow(l))
			break;
		ladvance(l);
	}
	/* trim trailing spaces */
	const char *end = l->p;
	while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t'))
		end--;
	t.value = start;
	t.value_len = (size_t)(end - start);
	return t;
}

/* &name или *name */
static Token lex_anchor_alias(Lexer *l, TK_Type type)
{
	Token t = make_tok(l, type);
	ladvance(l); /* skip & or * */
	const char *start = l->p;
	while (*l->p && *l->p != ' ' && *l->p != '\t' &&
		   *l->p != '\n' && *l->p != '\r' &&
		   *l->p != ',' && *l->p != '[' && *l->p != ']' &&
		   *l->p != '{' && *l->p != '}')
		ladvance(l);
	t.value = start;
	t.value_len = (size_t)(l->p - start);
	return t;
}

/* !tag / !!tag / !<uri> */
static Token lex_tag(Lexer *l)
{
	Token t = make_tok(l, TK_TAG);
	const char *start = l->p;
	ladvance(l); /* skip first ! */
	if (*l->p == '!')
		ladvance(l); /* !!tag */
	else if (*l->p == '<')
	{ /* !<uri> */
		while (*l->p && *l->p != '>')
			ladvance(l);
		if (*l->p == '>')
			ladvance(l);
	}
	while (*l->p && *l->p != ' ' && *l->p != '\t' &&
		   *l->p != '\n' && *l->p != '\r')
		ladvance(l);
	t.value = start;
	t.value_len = (size_t)(l->p - start);
	return t;
}

/* ── главный цикл лексера ──────────────────────────────────────────── */

Token *lex(const char *src, const char **error_out)
{
	Lexer l = {.src = src, .p = src, .line = 1, .col = 0, .flow_depth = 0, .error = NULL};
	Token *tokens = da_new(Token, 64);
	if (!tokens)
	{
		if (error_out)
			*error_out = "OOM";
		return NULL;
	}

	for (;;)
	{
		skip_ws(&l);
		int tok_line = l.line, tok_col = l.col;

		if (*l.p == '\0')
		{
			da_push(tokens, make_tok(&l, TK_EOF));
			break;
		}

		/* --- / ... в начале строки (col==0 или только пробелы до) */
		if (l.col == 0 && l.p[0] == '-' && l.p[1] == '-' && l.p[2] == '-' &&
			(l.p[3] == ' ' || l.p[3] == '\n' || l.p[3] == '\r' || l.p[3] == '\0'))
		{
			ladvance(&l);
			ladvance(&l);
			ladvance(&l);
			Token t = {.type = TK_DOC_START, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}
		if (l.col == 0 && l.p[0] == '.' && l.p[1] == '.' && l.p[2] == '.' &&
			(l.p[3] == ' ' || l.p[3] == '\n' || l.p[3] == '\r' || l.p[3] == '\0'))
		{
			ladvance(&l);
			ladvance(&l);
			ladvance(&l);
			Token t = {.type = TK_DOC_END, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}

		char c = lc(&l);

		/* flow indicators */
		if (c == '[')
		{
			ladvance(&l);
			l.flow_depth++;
			Token t = {.type = TK_FLOW_SEQ_START, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}
		if (c == ']')
		{
			ladvance(&l);
			if (l.flow_depth > 0)
				l.flow_depth--;
			Token t = {.type = TK_FLOW_SEQ_END, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}
		if (c == '{')
		{
			ladvance(&l);
			l.flow_depth++;
			Token t = {.type = TK_FLOW_MAP_START, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}
		if (c == '}')
		{
			ladvance(&l);
			if (l.flow_depth > 0)
				l.flow_depth--;
			Token t = {.type = TK_FLOW_MAP_END, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}
		if (c == ',' && is_flow(&l))
		{
			ladvance(&l);
			Token t = {.type = TK_FLOW_ENTRY, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}

		/* block seq entry */
		if (c == '-' && !is_flow(&l) && is_seq_entry_dash(&l))
		{
			ladvance(&l);
			Token t = {.type = TK_SEQ_ENTRY, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}

		/* explicit map key */
		if (c == '?' && (*(l.p + 1) == ' ' || *(l.p + 1) == '\n'))
		{
			ladvance(&l);
			Token t = {.type = TK_MAP_KEY, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}

		/* map value */
		if (c == ':' && is_map_val_colon(&l))
		{
			ladvance(&l);
			Token t = {.type = TK_MAP_VAL, .line = tok_line, .col = tok_col};
			da_push(tokens, t);
			continue;
		}

		/* anchor / alias */
		if (c == '&')
		{
			Token t = lex_anchor_alias(&l, TK_ANCHOR);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}
		if (c == '*')
		{
			Token t = lex_anchor_alias(&l, TK_ALIAS);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}

		/* tag */
		if (c == '!')
		{
			Token t = lex_tag(&l);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}

		/* block scalars */
		if (c == '|')
		{
			Token t = lex_block_scalar(&l, SCALAR_LITERAL);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}
		if (c == '>')
		{
			Token t = lex_block_scalar(&l, SCALAR_FOLDED);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}

		/* quoted scalars */
		if (c == '\'')
		{
			Token t = lex_single_quoted(&l);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}
		if (c == '"')
		{
			Token t = lex_double_quoted(&l);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
			continue;
		}

		/* plain scalar */
		{
			Token t = lex_plain(&l);
			t.line = tok_line;
			t.col = tok_col;
			da_push(tokens, t);
		}
	}

	if (l.error)
	{
		if (error_out)
			*error_out = l.error;
		da_free(tokens);
		return NULL;
	}
	if (error_out)
		*error_out = NULL;
	return tokens;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <inttypes.h>

/* ── глобальное состояние последней ошибки ─────────────────────────── */

static int g_ok;
static char g_error[256];

static void set_error(int code, const char *msg)
{
	g_ok = code;
	snprintf(g_error, sizeof(g_error), "%s", msg);
}

int YMLErrorPrint(void)
{
	if (g_ok == 0)
		return 0;
	fprintf(stderr, "YMLParser error %d: %s\n", g_ok, g_error);
	return g_ok;
}

/* ── состояние парсера ─────────────────────────────────────────────── */

/*
 * Таблица якорей: имя → указатель на уже построенный YMLValue.
 * Алиас (*name) — просто возвращаем тот же указатель; дерево —
 * не граф, поэтому при YMLDestroy нельзя освобождать узел дважды.
 * Якоря хранятся в da<_anchor_entry>, живут на время одного YMLParse.
 */
typedef struct
{
	char *name;
	YMLValue *node;
} _anchor_entry;

typedef struct
{
	Token *tokens; /* da<Token> от лексера */
	size_t pos;	   /* индекс текущего токена */

	_anchor_entry *anchors; /* da<_anchor_entry> */

	int ok;
	char error[256];
} Parser;

/* ── утилиты парсера ───────────────────────────────────────────────── */

static Token *cur(Parser *p) { return &p->tokens[p->pos]; }
static Token *peek(Parser *p, size_t offset) { return &p->tokens[p->pos + offset]; }
static void advance(Parser *p)
{
	if (cur(p)->type != TK_EOF)
		p->pos++;
}

static void parse_error(Parser *p, const char *msg)
{
	Token *t = cur(p);
	snprintf(p->error, sizeof(p->error),
			 "line %d col %d: %s", t->line, t->col, msg);
	p->ok = 1;
}

/* Скопировать value/value_len токена в heap (NUL-terminated). */
static char *token_strdup(const Token *t)
{
	char *s = malloc(t->value_len + 1);
	if (!s)
		return NULL;
	memcpy(s, t->value, t->value_len);
	s[t->value_len] = '\0';
	return s;
}

/* ── инференс типа для plain-скаляра (YAML Core Schema) ───────────── */

/*
 * Порядок проверки:
 *   1. null:  ~  null  Null  NULL  (пустая строка тоже null)
 *   2. bool:  true True TRUE  /  false False FALSE
 *   3. int:   [-+]?[0-9]+  |  0x[0-9a-fA-F]+  |  0o[0-7]+
 *   4. float: [-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?
 *             .inf  -.inf  +.inf  .nan
 *   5. string: всё остальное
 */
static YMLValue infer_scalar(const char *s, size_t len);

/* ── рекурсивные функции разбора ───────────────────────────────────── */

/*
 * Разобрать один узел начиная с текущего токена.
 * min_indent — минимальный отступ, при котором блочная коллекция
 * считается вложенной (0 для корня).
 */
static YMLValue *parse_node(Parser *p, int min_indent);

/* Разобрать блочный маппинг. Все ключи-значения с отступом == indent. */
static YMLValue *parse_block_mapping(Parser *p, int indent);

/* Разобрать блочную последовательность. Все элементы '-' с отступом == indent. */
static YMLValue *parse_block_sequence(Parser *p, int indent);

/* Разобрать flow-маппинг { k: v, ... } */
static YMLValue *parse_flow_mapping(Parser *p);

/* Разобрать flow-последовательность [ v, ... ] */
static YMLValue *parse_flow_sequence(Parser *p);

/* Разобрать скалярный токен и применить infer_scalar (или тег). */
static YMLValue *parse_scalar(Parser *p);

/* Глубокая копия YMLValue (для разворачивания алиасов). */
static YMLValue *yml_deep_copy(const YMLValue *src);

/* ── инференс типа plain-скаляра (YAML Core Schema) ───────────────── */

static bool is_decimal(const char *s, size_t len)
{
	if (!len)
		return false;
	size_t i = 0;
	if (s[i] == '+' || s[i] == '-')
		i++;
	if (i == len)
		return false;
	for (; i < len; i++)
		if (!isdigit((unsigned char)s[i]))
			return false;
	return true;
}

static bool is_hex(const char *s, size_t len)
{
	if (len < 3 || s[0] != '0' || s[1] != 'x')
		return false;
	for (size_t i = 2; i < len; i++)
		if (!isxdigit((unsigned char)s[i]))
			return false;
	return len > 2;
}

static bool is_octal(const char *s, size_t len)
{
	if (len < 3 || s[0] != '0' || s[1] != 'o')
		return false;
	for (size_t i = 2; i < len; i++)
		if (s[i] < '0' || s[i] > '7')
			return false;
	return len > 2;
}

static YMLValue infer_scalar(const char *s, size_t len)
{
	YMLValue v = {0};

	/* null */
	if (len == 0 || (len == 1 && s[0] == '~') ||
		(len == 4 && (memcmp(s, "null", 4) == 0 || memcmp(s, "Null", 4) == 0 || memcmp(s, "NULL", 4) == 0)))
	{
		v.type = YML_NULL;
		return v;
	}

	/* bool */
	if ((len == 4 && (memcmp(s, "true", 4) == 0 || memcmp(s, "True", 4) == 0 || memcmp(s, "TRUE", 4) == 0)))
	{
		v.type = YML_BOOL;
		v.value.boolean = true;
		return v;
	}
	if ((len == 5 && (memcmp(s, "false", 5) == 0 || memcmp(s, "False", 5) == 0 || memcmp(s, "FALSE", 5) == 0)))
	{
		v.type = YML_BOOL;
		v.value.boolean = false;
		return v;
	}

	/* special floats: .inf / +.inf / -.inf / .nan */
	if ((len == 4 && memcmp(s, ".inf", 4) == 0) || (len == 5 && memcmp(s, "+.inf", 5) == 0))
	{
		v.type = YML_FLOAT;
		v.value.number = HUGE_VAL;
		return v;
	}
	if (len == 5 && memcmp(s, "-.inf", 5) == 0)
	{
		v.type = YML_FLOAT;
		v.value.number = -HUGE_VAL;
		return v;
	}
	if ((len == 4 && memcmp(s, ".nan", 4) == 0) || (len == 4 && memcmp(s, ".NaN", 4) == 0) ||
		(len == 4 && memcmp(s, ".NAN", 4) == 0))
	{
		v.type = YML_FLOAT;
		v.value.number = NAN;
		return v;
	}

	/* int: decimal / 0x / 0o */
	if (is_decimal(s, len))
	{
		char tmp[64];
		size_t n = len < 63 ? len : 63;
		memcpy(tmp, s, n);
		tmp[n] = '\0';
		v.type = YML_INT;
		v.value.integer = (int64_t)strtoll(tmp, NULL, 10);
		return v;
	}
	if (is_hex(s, len))
	{
		char tmp[64];
		size_t n = len < 63 ? len : 63;
		memcpy(tmp, s, n);
		tmp[n] = '\0';
		v.type = YML_INT;
		v.value.integer = (int64_t)strtoll(tmp + 2, NULL, 16);
		return v;
	}
	if (is_octal(s, len))
	{
		char tmp[64];
		size_t n = len < 63 ? len : 63;
		memcpy(tmp, s, n);
		tmp[n] = '\0';
		v.type = YML_INT;
		v.value.integer = (int64_t)strtoll(tmp + 2, NULL, 8);
		return v;
	}

	/* float */
	{
		/* быстрая проверка: должны быть цифры, '.', 'e', 'E', '+', '-' */
		bool has_dot_or_e = false;
		const char *p2 = s;
		size_t rem = len;
		if (rem && (*p2 == '+' || *p2 == '-'))
		{
			p2++;
			rem--;
		}
		for (size_t i = 0; i < rem; i++)
		{
			if (p2[i] == '.' || p2[i] == 'e' || p2[i] == 'E')
			{
				has_dot_or_e = true;
				break;
			}
		}
		if (has_dot_or_e)
		{
			char tmp[64];
			size_t n = len < 63 ? len : 63;
			memcpy(tmp, s, n);
			tmp[n] = '\0';
			char *end;
			double d = strtod(tmp, &end);
			if (end == tmp + n)
			{
				v.type = YML_FLOAT;
				v.value.number = d;
				return v;
			}
		}
	}

	/* string */
	char *copy = malloc(len + 1);
	if (copy)
	{
		memcpy(copy, s, len);
		copy[len] = '\0';
	}
	v.type = YML_STRING;
	v.value.string = copy;
	return v;
}

/* ── обработка строк ───────────────────────────────────────────────── */

/* Декодировать \uXXXX (4 hex цифры) в UTF-8 байты, вернуть число записанных байт. */
static int encode_utf8(uint32_t cp, char *out)
{
	if (cp < 0x80)
	{
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800)
	{
		out[0] = (char)(0xC0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if (cp < 0x10000)
	{
		out[0] = (char)(0xE0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	}
	out[0] = (char)(0xF0 | (cp >> 18));
	out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
	out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
	out[3] = (char)(0x80 | (cp & 0x3F));
	return 4;
}

static uint32_t parse_hex_n(const char *s, int n)
{
	uint32_t v = 0;
	for (int i = 0; i < n; i++)
	{
		v <<= 4;
		char c = s[i];
		if (c >= '0' && c <= '9')
			v |= (uint32_t)(c - '0');
		else if (c >= 'a' && c <= 'f')
			v |= (uint32_t)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v |= (uint32_t)(c - 'A' + 10);
	}
	return v;
}

/*
 * Обработать double-quoted строку: escape-последовательности → байты.
 * src/len — содержимое между кавычками (без самих кавычек).
 * Возвращает heap-строку (caller освобождает).
 */
static char *unescape_double_quoted(const char *src, size_t len)
{
	char *buf = malloc(len + 1);
	if (!buf)
		return NULL;
	size_t w = 0;
	for (size_t i = 0; i < len;)
	{
		if (src[i] != '\\')
		{
			buf[w++] = src[i++];
			continue;
		}
		i++; /* skip \ */
		if (i >= len)
			break;
		char esc = src[i++];
		switch (esc)
		{
		case '0':
			buf[w++] = '\0';
			break;
		case 'a':
			buf[w++] = '\a';
			break;
		case 'b':
			buf[w++] = '\b';
			break;
		case 't':
		case '\t':
			buf[w++] = '\t';
			break;
		case 'n':
			buf[w++] = '\n';
			break;
		case 'v':
			buf[w++] = '\v';
			break;
		case 'f':
			buf[w++] = '\f';
			break;
		case 'r':
			buf[w++] = '\r';
			break;
		case 'e':
			buf[w++] = 0x1B;
			break;
		case '"':
			buf[w++] = '"';
			break;
		case '/':
			buf[w++] = '/';
			break;
		case '\\':
			buf[w++] = '\\';
			break;
		case 'x': /* \xXX */
			if (i + 2 <= len)
			{
				buf[w++] = (char)parse_hex_n(src + i, 2);
				i += 2;
			}
			break;
		case 'u': /* \uXXXX */
			if (i + 4 <= len)
			{
				w += (size_t)encode_utf8(parse_hex_n(src + i, 4), buf + w);
				i += 4;
			}
			break;
		case 'U': /* \UXXXXXXXX */
			if (i + 8 <= len)
			{
				w += (size_t)encode_utf8(parse_hex_n(src + i, 8), buf + w);
				i += 8;
			}
			break;
		case '\n': /* line continuation: skip leading whitespace on next line */
			while (i < len && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r'))
				i++;
			break;
		default:
			buf[w++] = esc;
			break;
		}
	}
	buf[w] = '\0';
	return buf;
}

/*
 * Обработать single-quoted строку: '' → '.
 */
static char *unescape_single_quoted(const char *src, size_t len)
{
	char *buf = malloc(len + 1);
	if (!buf)
		return NULL;
	size_t w = 0;
	for (size_t i = 0; i < len;)
	{
		if (src[i] == '\'' && i + 1 < len && src[i + 1] == '\'')
		{
			buf[w++] = '\'';
			i += 2;
		}
		else
			buf[w++] = src[i++];
	}
	buf[w] = '\0';
	return buf;
}

/*
 * Свернуть содержимое folded-блочного скаляра.
 * Правило: одиночный \n → ' ', но \n\n → \n.
 */
static char *fold_scalar(const char *src, size_t len)
{
	char *buf = malloc(len + 1);
	if (!buf)
		return NULL;
	size_t w = 0;
	for (size_t i = 0; i < len;)
	{
		if (src[i] != '\n')
		{
			buf[w++] = src[i++];
			continue;
		}
		if (i + 1 >= len)
		{
			/* финальный \n: сохранить (clip chomping) */
			buf[w++] = '\n';
			i++;
		}
		else if (src[i + 1] == '\n')
		{
			/* пустые строки: текущий \n сворачивается,
			   каждая пустая строка → один \n в выводе */
			i++;
			while (i < len && src[i] == '\n')
			{
				buf[w++] = '\n';
				i++;
			}
		}
		else
		{
			/* одиночный \n между строками → пробел */
			buf[w++] = ' ';
			i++;
		}
	}
	buf[w] = '\0';
	return buf;
}

/* ── якоря / алиасы ────────────────────────────────────────────────── */

static void anchor_add(Parser *p, const char *name, size_t name_len, YMLValue *node)
{
	char *n = malloc(name_len + 1);
	if (!n)
		return;
	memcpy(n, name, name_len);
	n[name_len] = '\0';
	/* хранить deep copy: оригинальный node может быть скопирован в hm и освобождён */
	_anchor_entry e = {.name = n, .node = yml_deep_copy(node)};
	da_push(p->anchors, e);
}

static YMLValue *anchor_find(Parser *p, const char *name, size_t name_len)
{
	size_t count = da_len(p->anchors);
	for (size_t i = 0; i < count; i++)
	{
		_anchor_entry *e = &p->anchors[i];
		if (strlen(e->name) == name_len && memcmp(e->name, name, name_len) == 0)
			return e->node;
	}
	return NULL;
}

/* ── глубокая копия YMLValue ───────────────────────────────────────── */

static YMLValue *yml_deep_copy(const YMLValue *src)
{
	if (!src)
		return NULL;
	YMLValue *v = malloc(sizeof(YMLValue));
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
		v->value.string = src->value.string ? strdup(src->value.string) : NULL;
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
				free(cp);
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
				free(cp);
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

/* ── применить тег к значению ──────────────────────────────────────── */

/*
 * Встроенные теги YAML (Core Schema):
 *   !!null !!bool !!int !!float !!str !!seq !!map
 * Игнорируем кастомные (!mytype и т.п.).
 * tag_val / tag_len — содержимое тега включая ! (например "!!str", len=5).
 */
/*
 * Применить тег к уже построенному значению v.
 * При необходимости конвертирует значение + тип.
 * Работает только с plain-скалярами — вызывается из parse_node.
 */
static void apply_tag(YMLValue *v, const char *tag, size_t tag_len)
{
	if (tag_len < 2)
		return;
	/* нормализовать: !!str → "str", !str → "str" */
	const char *t = tag;
	size_t tl = tag_len;
	if (tl >= 2 && t[0] == '!' && t[1] == '!')
	{
		t += 2;
		tl -= 2;
	}
	else if (tl >= 1 && t[0] == '!')
	{
		t++;
		tl--;
	}

	if (tl == 4 && memcmp(t, "null", 4) == 0)
	{
		v->type = YML_NULL;
		return;
	}
	if (tl == 5 && memcmp(t, "float", 5) == 0)
	{
		v->type = YML_FLOAT;
		return;
	}
	/* !!seq, !!map — структуры, не конвертируем */

	if (tl == 3 && memcmp(t, "str", 3) == 0)
	{
		if (v->type == YML_STRING)
			return;
		char buf[64] = "";
		switch (v->type)
		{
		case YML_INT:
			snprintf(buf, sizeof(buf), "%" PRId64, v->value.integer);
			break;
		case YML_FLOAT:
			snprintf(buf, sizeof(buf), "%g", v->value.number);
			break;
		case YML_BOOL:
			snprintf(buf, sizeof(buf), "%s", v->value.boolean ? "true" : "false");
			break;
		case YML_NULL:
			snprintf(buf, sizeof(buf), "null");
			break;
		default:
			break;
		}
		v->type = YML_STRING;
		v->value.string = strdup(buf);
		return;
	}

	if (tl == 3 && memcmp(t, "int", 3) == 0)
	{
		if (v->type == YML_INT)
			return;
		if (v->type == YML_STRING)
		{
			char *end;
			int64_t n = strtoll(v->value.string, &end, 0);
			free((char *)v->value.string);
			v->type = YML_INT;
			v->value.integer = n;
		}
		else if (v->type == YML_FLOAT)
		{
			v->type = YML_INT;
			v->value.integer = (int64_t)v->value.number;
		}
		return;
	}

	if (tl == 4 && memcmp(t, "bool", 4) == 0)
	{
		if (v->type == YML_BOOL)
			return;
		bool b = false;
		if (v->type == YML_STRING)
			b = (strcmp(v->value.string, "true") == 0 || strcmp(v->value.string, "1") == 0);
		else if (v->type == YML_INT)
			b = v->value.integer != 0;
		if (v->type == YML_STRING)
			free((char *)v->value.string);
		v->type = YML_BOOL;
		v->value.boolean = b;
		return;
	}
}

/* ── parse_scalar ──────────────────────────────────────────────────── */

static YMLValue *parse_scalar(Parser *p)
{
	Token *t = cur(p);
	if (t->type != TK_SCALAR)
	{
		parse_error(p, "expected scalar");
		return NULL;
	}
	advance(p);

	YMLValue *v = malloc(sizeof(YMLValue));
	if (!v)
	{
		parse_error(p, "OOM");
		return NULL;
	}

	switch (t->style)
	{
	case SCALAR_PLAIN:
		*v = infer_scalar(t->value, t->value_len);
		break;
	case SCALAR_SINGLE_QUOTED:
		v->type = YML_STRING;
		v->value.string = unescape_single_quoted(t->value, t->value_len);
		break;
	case SCALAR_DOUBLE_QUOTED:
		v->type = YML_STRING;
		v->value.string = unescape_double_quoted(t->value, t->value_len);
		break;
	case SCALAR_LITERAL:
		v->type = YML_STRING;
		v->value.string = malloc(t->value_len + 1);
		if (v->value.string)
		{
			memcpy((char *)v->value.string, t->value, t->value_len);
			((char *)v->value.string)[t->value_len] = '\0';
		}
		/* блочный скаляр: лексер выделил буфер — освобождаем его */
		free((char *)t->value);
		t->value = NULL;
		break;
	case SCALAR_FOLDED:
		v->type = YML_STRING;
		v->value.string = fold_scalar(t->value, t->value_len);
		free((char *)t->value);
		t->value = NULL;
		break;
	}
	return v;
}

/* ── parse_flow_sequence ────────────────────────────────────────────── */

static YMLValue *parse_flow_sequence(Parser *p)
{
	advance(p); /* skip [ */
	YMLValue *v = malloc(sizeof(YMLValue));
	if (!v)
	{
		parse_error(p, "OOM");
		return NULL;
	}
	v->type = YML_ARRAY;
	v->value.array = da_new(YMLValue, 4);

	while (cur(p)->type != TK_FLOW_SEQ_END && cur(p)->type != TK_EOF)
	{
		if (cur(p)->type == TK_FLOW_ENTRY)
		{
			advance(p);
			continue;
		}
		YMLValue *elem = parse_node(p, 0);
		if (p->ok || !elem)
			break;
		da_push(v->value.array, *elem);
		free(elem);
	}
	if (cur(p)->type == TK_FLOW_SEQ_END)
		advance(p);
	return v;
}

/* ── parse_flow_mapping ─────────────────────────────────────────────── */

static YMLValue *parse_flow_mapping(Parser *p)
{
	advance(p); /* skip { */
	_hm *hm = hm_new(8);
	YMLValue *v = malloc(sizeof(YMLValue));
	if (!v || !hm)
	{
		parse_error(p, "OOM");
		return NULL;
	}
	v->type = YML_OBJECT;
	v->value.object = hm;

	while (cur(p)->type != TK_FLOW_MAP_END && cur(p)->type != TK_EOF)
	{
		if (cur(p)->type == TK_FLOW_ENTRY)
		{
			advance(p);
			continue;
		}
		if (cur(p)->type == TK_MAP_KEY)
		{
			advance(p);
		} /* explicit ? */

		/* ключ должен быть скаляром */
		if (cur(p)->type != TK_SCALAR)
		{
			parse_error(p, "expected scalar key in flow mapping");
			break;
		}
		char *key = token_strdup(cur(p));
		advance(p);

		if (cur(p)->type != TK_MAP_VAL)
		{
			free(key);
			parse_error(p, "expected ':' in flow mapping");
			break;
		}
		advance(p);

		YMLValue *val = parse_node(p, 0);
		if (p->ok || !val)
		{
			free(key);
			break;
		}
		hm_set(hm, key, *val);
		free(key);
		free(val);
	}
	if (cur(p)->type == TK_FLOW_MAP_END)
		advance(p);
	return v;
}

/* ── parse_block_sequence ───────────────────────────────────────────── */

static YMLValue *parse_block_sequence(Parser *p, int indent)
{
	YMLValue *v = malloc(sizeof(YMLValue));
	if (!v)
	{
		parse_error(p, "OOM");
		return NULL;
	}
	v->type = YML_ARRAY;
	v->value.array = da_new(YMLValue, 4);

	while (cur(p)->type == TK_SEQ_ENTRY && cur(p)->col == indent)
	{
		advance(p); /* skip - */
		/* если сразу EOF или следующий - с тем же отступом — null элемент */
		if (cur(p)->type == TK_EOF || cur(p)->type == TK_DOC_END ||
			(cur(p)->type == TK_SEQ_ENTRY && cur(p)->col == indent))
		{
			YMLValue null_v = {.type = YML_NULL};
			da_push(v->value.array, null_v);
			continue;
		}
		YMLValue *elem = parse_node(p, indent + 1);
		if (p->ok || !elem)
			break;
		da_push(v->value.array, *elem);
		free(elem);
	}
	return v;
}

/* ── parse_block_mapping ─────────────────────────────────────────────── */

/*
 * Применить merge key "<<" — скопировать пары из src в dst,
 * не перезаписывая уже существующие ключи.
 */
static void apply_merge(_hm *dst, const YMLValue *src)
{
	if (!src)
		return;
	if (src->type == YML_OBJECT)
	{
		_hm *src_hm = (_hm *)src->value.object;
		size_t idx = 0;
		const char *key;
		YMLValue *val;
		while (hm_next(src_hm, &idx, &key, &val))
		{
			if (!hm_get(dst, key))
			{ /* не перезаписывать */
				YMLValue *cp = yml_deep_copy(val);
				if (cp)
				{
					hm_set(dst, key, *cp);
					free(cp);
				}
			}
		}
	}
	else if (src->type == YML_ARRAY)
	{
		/* << может быть массивом маппингов */
		size_t n = da_len(src->value.array);
		for (size_t i = 0; i < n; i++)
			apply_merge(dst, &src->value.array[i]);
	}
}

static YMLValue *parse_block_mapping(Parser *p, int indent)
{
	_hm *hm = hm_new(8);
	YMLValue *v = malloc(sizeof(YMLValue));
	if (!v || !hm)
	{
		parse_error(p, "OOM");
		return NULL;
	}
	v->type = YML_OBJECT;
	v->value.object = hm;

	for (;;)
	{
		/* явный ключ (? key : value) */
		bool explicit_key = (cur(p)->type == TK_MAP_KEY && cur(p)->col == indent);
		bool implicit_key = (cur(p)->type == TK_SCALAR && cur(p)->col == indent &&
							 peek(p, 1)->type == TK_MAP_VAL &&
							 peek(p, 1)->line == cur(p)->line);
		if (!explicit_key && !implicit_key)
			break;

		if (explicit_key)
			advance(p); /* skip ? */

		/* ключ */
		if (cur(p)->type != TK_SCALAR)
		{
			parse_error(p, "expected scalar key");
			break;
		}
		char *key = token_strdup(cur(p));
		if (!key)
		{
			parse_error(p, "OOM");
			break;
		}
		advance(p);

		/* : */
		if (cur(p)->type != TK_MAP_VAL)
		{
			/* нет значения — null */
			YMLValue null_v = {.type = YML_NULL};
			if (strcmp(key, "<<") == 0)
			{
				free(key);
				continue;
			}
			hm_set(hm, key, null_v);
			free(key);
			continue;
		}
		advance(p); /* skip : */

		/* значение — может быть на той же строке или на следующей с большим отступом */
		YMLValue *val;
		if (cur(p)->type == TK_EOF || cur(p)->type == TK_DOC_END ||
			cur(p)->type == TK_DOC_START)
		{
			val = malloc(sizeof(YMLValue));
			if (val)
				val->type = YML_NULL;
		}
		else
		{
			val = parse_node(p, indent + 1);
		}
		if (p->ok || !val)
		{
			free(key);
			break;
		}

		if (strcmp(key, "<<") == 0)
		{
			apply_merge(hm, val);
			_YMLDestroy(val, (struct _YMLOptionals){0});
		}
		else
		{
			hm_set(hm, key, *val);
			free(val);
		}
		free(key);
	}
	return v;
}

/* ── parse_node ──────────────────────────────────────────────────────── */

static YMLValue *parse_node(Parser *p, int min_indent)
{
	if (p->ok)
		return NULL;

	/* необязательный тег */
	const char *tag = NULL;
	size_t tag_len = 0;
	if (cur(p)->type == TK_TAG)
	{
		tag = cur(p)->value;
		tag_len = cur(p)->value_len;
		advance(p);
	}

	/* необязательный якорь */
	const char *anchor_name = NULL;
	size_t anchor_len = 0;
	if (cur(p)->type == TK_ANCHOR)
	{
		anchor_name = cur(p)->value;
		anchor_len = cur(p)->value_len;
		advance(p);
	}

	YMLValue *v = NULL;
	Token *t = cur(p);

	if (t->type == TK_ALIAS)
	{
		YMLValue *orig = anchor_find(p, t->value, t->value_len);
		if (!orig)
		{
			parse_error(p, "unknown alias");
			return NULL;
		}
		advance(p);
		v = yml_deep_copy(orig);
	}
	else if (t->type == TK_FLOW_SEQ_START)
	{
		v = parse_flow_sequence(p);
	}
	else if (t->type == TK_FLOW_MAP_START)
	{
		v = parse_flow_mapping(p);
	}
	else if (t->type == TK_SEQ_ENTRY && t->col >= min_indent)
	{
		v = parse_block_sequence(p, t->col);
	}
	else if (t->type == TK_MAP_KEY && t->col >= min_indent)
	{
		v = parse_block_mapping(p, t->col);
	}
	else if (t->type == TK_SCALAR && t->col >= min_indent)
	{
		/* смотрим вперёд: если следующий TK_MAP_VAL на той же строке — маппинг */
		Token *nxt = peek(p, 1);
		if (nxt->type == TK_MAP_VAL && nxt->line == t->line)
		{
			v = parse_block_mapping(p, t->col);
		}
		else
		{
			v = parse_scalar(p);
		}
	}
	else if (t->type == TK_EOF || t->type == TK_DOC_END || t->type == TK_DOC_START)
	{
		v = malloc(sizeof(YMLValue));
		if (v)
			v->type = YML_NULL;
	}
	else
	{
		/* неожиданный токен — пустое значение */
		v = malloc(sizeof(YMLValue));
		if (v)
			v->type = YML_NULL;
	}

	if (!v)
	{
		parse_error(p, "OOM");
		return NULL;
	}

	if (tag)
		apply_tag(v, tag, tag_len);

	if (anchor_name)
		anchor_add(p, anchor_name, anchor_len, v);

	return v;
}

/* ── YMLParseStream ──────────────────────────────────────────────────── */

YMLValue **_YMLParseStream(const char *yml_str, struct _YMLOptionals optionals)
{
	g_ok = 0;
	g_error[0] = '\0';

	const char *lex_err = NULL;
	Token *tokens = lex(yml_str, &lex_err);
	if (!tokens)
	{
		set_error(1, lex_err ? lex_err : "lexer error");
		if (optionals.ok)
			*optionals.ok = g_ok;
		if (optionals.error)
			*optionals.error = g_error;
		return NULL;
	}

	Parser p = {.tokens = tokens, .pos = 0, .anchors = da_new(_anchor_entry, 8), .ok = 0};

	YMLValue **docs = da_new(YMLValue *, 4);

	while (cur(&p)->type != TK_EOF)
	{
		if (cur(&p)->type == TK_DOC_START)
			advance(&p);
		if (cur(&p)->type == TK_EOF)
			break;
		YMLValue *doc = parse_node(&p, 0);
		if (p.ok)
		{
			set_error(p.ok, p.error);
			break;
		}
		da_push(docs, doc);
		if (cur(&p)->type == TK_DOC_END)
			advance(&p);
	}

	for (size_t i = 0; i < da_len(p.anchors); i++)
		free(p.anchors[i].name);
	da_free(p.anchors);
	da_free(tokens);

	if (optionals.ok)
		*optionals.ok = g_ok;
	if (optionals.error)
		*optionals.error = g_error;
	return docs;
}

/* ── публичные функции ─────────────────────────────────────────────── */

YMLValue *_YMLParse(const char *yml_str, struct _YMLOptionals optionals)
{
	g_ok = 0;
	g_error[0] = '\0';

	const char *lex_err = NULL;
	Token *tokens = lex(yml_str, &lex_err);
	if (!tokens)
	{
		set_error(1, lex_err ? lex_err : "lexer error");
		if (optionals.ok)
			*optionals.ok = g_ok;
		if (optionals.error)
			*optionals.error = g_error;
		return NULL;
	}

	Parser p = {
		.tokens = tokens,
		.pos = 0,
		.anchors = da_new(_anchor_entry, 8),
		.ok = 0,
	};

	/* пропустить необязательный --- */
	if (cur(&p)->type == TK_DOC_START)
		advance(&p);

	YMLValue *root = parse_node(&p, 0);

	/* синхронизировать ошибку в глобальное состояние */
	if (p.ok != 0)
	{
		set_error(p.ok, p.error);
		root = NULL; /* TODO: освободить частично построенное дерево */
	}

	/* освободить имена и deep-copy ноды якорей */
	for (size_t i = 0; i < da_len(p.anchors); i++)
	{
		free(p.anchors[i].name);
		_YMLDestroy(p.anchors[i].node, (struct _YMLOptionals){0});
	}
	da_free(p.anchors);
	da_free(tokens);

	if (optionals.ok)
		*optionals.ok = g_ok;
	if (optionals.error)
		*optionals.error = g_error;
	return root;
}

/* Рекурсивно освободить одно значение (не сам указатель root). */
static void yml_value_free(YMLValue *v)
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
			yml_value_free(&v->value.array[i]);
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

void _YMLDestroy(YMLValue *root, struct _YMLOptionals optionals)
{
	if (!root)
		return;
	yml_value_free(root);
	free(root);
	if (optionals.ok)
		*optionals.ok = 0;
}

void _YMLDestroyStream(YMLValue **stream, struct _YMLOptionals optionals)
{
	if (!stream)
		return;
	size_t n = da_len(stream);
	for (size_t i = 0; i < n; i++)
		_YMLDestroy(stream[i], (struct _YMLOptionals){0});
	da_free(stream);
	if (optionals.ok)
		*optionals.ok = 0;
}

YMLValue *_YMLMapGet(void *hm, const char *key, struct _YMLOptionals optionals)
{
	g_ok = 0;
	g_error[0] = '\0'; /* каждый вызов обновляет глобальное состояние */
	if (!hm)
	{
		set_error(1, "YMLMapGet: null object");
		if (optionals.ok)
			*optionals.ok = g_ok;
		if (optionals.error)
			*optionals.error = g_error;
		return NULL;
	}
	YMLValue *v = hm_get((_hm *)hm, key);
	if (!v)
	{
		snprintf(g_error, sizeof(g_error), "YMLMapGet: key '%s' not found", key);
		g_ok = 1;
		if (optionals.ok)
			*optionals.ok = g_ok;
		if (optionals.error)
			*optionals.error = g_error;
		return NULL;
	}
	if (optionals.type != YML_ANY && v->type != optionals.type)
	{
		snprintf(g_error, sizeof(g_error),
				 "YMLMapGet: key '%s' has type %d, expected %d",
				 key, v->type, optionals.type);
		g_ok = 2;
		if (optionals.ok)
			*optionals.ok = g_ok;
		if (optionals.error)
			*optionals.error = g_error;
		return NULL;
	}
	if (optionals.ok)
		*optionals.ok = 0;
	return v;
}

_YMLMapIter _YMLMapIterBegin(void *hm)
{
	return (_YMLMapIter){._hm = hm, ._i = 0};
}

bool _YMLMapIterNext(_YMLMapIter *iter, const char **key, YMLValue **value)
{
	if (!iter || !iter->_hm)
		return false;
	return hm_next((_hm *)iter->_hm, &iter->_i, key, value);
}

#endif

#endif