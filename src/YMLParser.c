#include "YMLParser.h"
#include "_da.h"
#include "_hm.h"
#include "_lexer.h"
#include "_yml_free.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <inttypes.h>

/* ── глобальное состояние последней ошибки ─────────────────────────── */

static _Thread_local int g_ok;
static _Thread_local char g_error[256];

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

/* ── управление таблицей якорей ────────────────────────────────────── */

/* Освободить все записи в таблице якорей, сохранив выделенную ёмкость da. */
static void anchors_clear(Parser *p)
{
	for (size_t i = 0; i < da_len(p->anchors); i++)
	{
		free(p->anchors[i].name);
		_YMLDestroy(p->anchors[i].node, (struct _YMLOptionals){0});
	}
	((_da_hdr *)p->anchors - 1)->len = 0;
}

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
		if (t->owns_value)
		{
			free((char *)t->value);
			t->value = NULL;
		}
		break;
	case SCALAR_FOLDED:
		v->type = YML_STRING;
		v->value.string = fold_scalar(t->value, t->value_len);
		if (t->owns_value)
		{
			free((char *)t->value);
			t->value = NULL;
		}
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
		{
			anchors_clear(&p);
			advance(&p);
		}
		if (cur(&p)->type == TK_EOF)
			break;
		YMLValue *doc = parse_node(&p, 0);
		if (p.ok)
		{
			set_error(p.ok, p.error);
			if (doc)
				_YMLDestroy(doc, (struct _YMLOptionals){0});
			break;
		}
		da_push(docs, doc);
		if (cur(&p)->type == TK_DOC_END)
		{
			anchors_clear(&p);
			advance(&p);
		}
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
		if (root)
			_YMLDestroy(root, (struct _YMLOptionals){0});
		root = NULL;
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

void _YMLDestroy(YMLValue *root, struct _YMLOptionals optionals)
{
	if (!root)
		return;
	yml_value_free_impl(root);
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
