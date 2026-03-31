#include "_lexer.h"
#include "_da.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* ── транскодирование UTF-16 / UTF-32 → UTF-8 ──────────────────────── */

/* Кодировать Unicode codepoint в UTF-8; вернуть число записанных байт. */
static int enc_utf8(uint32_t cp, char *out)
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

/*
 * Транскодировать UTF-16 (LE или BE) в heap-строку UTF-8.
 * src_bytes — длина в байтах (не считая BOM, который уже пропущен).
 * Возвращает NUL-terminated строку или NULL при OOM.
 */
static char *decode_utf16(const unsigned char *src, size_t src_bytes, bool be)
{
	/* Худший случай: каждый code unit → 3 байта UTF-8. */
	size_t cap = src_bytes / 2 * 3 + 1;
	char *buf = malloc(cap);
	if (!buf)
		return NULL;
	size_t w = 0;

	for (size_t i = 0; i + 1 < src_bytes; i += 2)
	{
		uint16_t u = be
			? (uint16_t)((src[i] << 8) | src[i + 1])
			: (uint16_t)(src[i] | (src[i + 1] << 8));

		uint32_t cp;
		if (u >= 0xD800 && u <= 0xDBFF) /* высокий суррогат */
		{
			if (i + 3 >= src_bytes)
				break;
			uint16_t lo = be
				? (uint16_t)((src[i + 2] << 8) | src[i + 3])
				: (uint16_t)(src[i + 2] | (src[i + 3] << 8));
			if (lo < 0xDC00 || lo > 0xDFFF)
				break; /* некорректная суррогатная пара */
			cp = 0x10000u + ((uint32_t)(u - 0xD800u) << 10) + (lo - 0xDC00u);
			i += 2;
		}
		else
			cp = u;

		w += (size_t)enc_utf8(cp, buf + w);
	}
	buf[w] = '\0';
	return buf;
}

/*
 * Транскодировать UTF-32 (LE или BE) в heap-строку UTF-8.
 * src_bytes — длина в байтах (не считая BOM).
 * Возвращает NUL-terminated строку или NULL при OOM.
 */
static char *decode_utf32(const unsigned char *src, size_t src_bytes, bool be)
{
	/* Худший случай: каждый codepoint → 4 байта UTF-8. */
	size_t cap = src_bytes / 4 * 4 + 1;
	char *buf = malloc(cap);
	if (!buf)
		return NULL;
	size_t w = 0;

	for (size_t i = 0; i + 3 < src_bytes; i += 4)
	{
		uint32_t cp = be
			? ((uint32_t)src[i] << 24) | ((uint32_t)src[i + 1] << 16)
			| ((uint32_t)src[i + 2] << 8) | (uint32_t)src[i + 3]
			: (uint32_t)src[i] | ((uint32_t)src[i + 1] << 8)
			| ((uint32_t)src[i + 2] << 16) | ((uint32_t)src[i + 3] << 24);
		w += (size_t)enc_utf8(cp, buf + w);
	}
	buf[w] = '\0';
	return buf;
}

/*
 * Определить кодировку входа и при необходимости транскодировать в UTF-8.
 *
 * Возвращает:
 *   NULL        — src уже UTF-8 (out_src указывает на начало данных,
 *                 может быть src+3 если был UTF-8 BOM)
 *   heap-буфер  — UTF-8 копия; caller обязан free() после использования
 *   (char*)-1   — ошибка OOM; *error_out заполнен
 *
 * Обнаружение по YAML 1.2.2 §5.2: BOM или паттерн нулевых байт.
 */
static char *normalize_encoding(const char *src, const char **out_src,
                                const char **error_out)
{
	const unsigned char *b = (const unsigned char *)src;

	/* UTF-32 BE: BOM 00 00 FE FF */
	if (b[0] == 0x00 && b[1] == 0x00 && b[2] == 0xFE && b[3] == 0xFF)
	{
		size_t len = strlen(src + 4); /* приближённо: ищем \0 в хвосте */
		char *r = decode_utf32(b + 4, len, true);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* UTF-32 LE: BOM FF FE 00 00 */
	if (b[0] == 0xFF && b[1] == 0xFE && b[2] == 0x00 && b[3] == 0x00)
	{
		/* Длина: ищем четыре нулевых байта подряд как терминатор. */
		size_t len = 0;
		while (b[4 + len] || b[4 + len + 1] || b[4 + len + 2] || b[4 + len + 3])
			len += 4;
		char *r = decode_utf32(b + 4, len, false);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* UTF-16 BE: BOM FE FF */
	if (b[0] == 0xFE && b[1] == 0xFF)
	{
		size_t len = 0;
		while (b[2 + len] || b[2 + len + 1])
			len += 2;
		char *r = decode_utf16(b + 2, len, true);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* UTF-16 LE: BOM FF FE (и не UTF-32 LE — уже проверен выше) */
	if (b[0] == 0xFF && b[1] == 0xFE)
	{
		size_t len = 0;
		while (b[2 + len] || b[2 + len + 1])
			len += 2;
		char *r = decode_utf16(b + 2, len, false);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* UTF-8 BOM: EF BB BF — просто пропустить */
	if (b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
	{
		*out_src = src + 3;
		return NULL;
	}
	/* Implicit UTF-32 BE: 00 00 00 xx */
	if (b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x00)
	{
		size_t len = 0;
		while (b[len] || b[len + 1] || b[len + 2] || b[len + 3])
			len += 4;
		char *r = decode_utf32(b, len, true);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* Implicit UTF-32 LE: xx 00 00 00 */
	if (b[1] == 0x00 && b[2] == 0x00 && b[3] == 0x00)
	{
		size_t len = 0;
		while (b[len] || b[len + 1] || b[len + 2] || b[len + 3])
			len += 4;
		char *r = decode_utf32(b, len, false);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* Implicit UTF-16 BE: 00 xx 00 xx */
	if (b[0] == 0x00 && b[2] == 0x00)
	{
		size_t len = 0;
		while (b[len] || b[len + 1])
			len += 2;
		char *r = decode_utf16(b, len, true);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}
	/* Implicit UTF-16 LE: xx 00 xx 00 */
	if (b[1] == 0x00 && b[3] == 0x00)
	{
		size_t len = 0;
		while (b[len] || b[len + 1])
			len += 2;
		char *r = decode_utf16(b, len, false);
		if (!r) { *error_out = "OOM"; return (char *)-1; }
		*out_src = r;
		return r;
	}

	/* UTF-8 без BOM — обычный путь */
	*out_src = src;
	return NULL;
}

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
	t.value = buf;
	t.value_len = buf_len;
	t.owns_value = true;
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
	/* Транскодировать UTF-16/32 → UTF-8 если необходимо. */
	const char *use_src = src;
	char *converted = normalize_encoding(src, &use_src, error_out);
	if (converted == (char *)-1)
		return NULL; /* OOM при транскодировании */

	Lexer l = {.src = use_src, .p = use_src, .line = 1, .col = 0, .flow_depth = 0, .error = NULL};
	Token *tokens = da_new(Token, 64);
	if (!tokens)
	{
		free(converted);
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

	free(converted); /* NULL-safe; освобождаем UTF-8 буфер если был выделен */

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
