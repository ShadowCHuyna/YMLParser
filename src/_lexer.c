#include "_lexer.h"
#include "_da.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── состояние лексера ─────────────────────────────────────────────── */

typedef struct {
    const char *src;
    const char *p;      /* текущая позиция */
    int         line;   /* 1-based */
    int         col;    /* 0-based */
    int         flow_depth; /* 0 = блочный контекст */
    const char *error;  /* static string при ошибке */
} Lexer;

/* ── утилиты ───────────────────────────────────────────────────────── */

static char lc(Lexer *l) { return *l->p; }

static void ladvance(Lexer *l) {
    if (*l->p == '\n') { l->line++; l->col = 0; }
    else                l->col++;
    l->p++;
}

static void skip_spaces(Lexer *l) {
    while (*l->p == ' ' || *l->p == '\t') ladvance(l);
}

static void skip_line(Lexer *l) {
    while (*l->p && *l->p != '\n') ladvance(l);
    if (*l->p == '\n') ladvance(l);
}

/* Пропустить пустые строки и комментарии. */
static void skip_ws(Lexer *l) {
    for (;;) {
        skip_spaces(l);
        if (*l->p == '#') { skip_line(l); continue; }
        if (*l->p == '\n') { ladvance(l); continue; }
        if (*l->p == '\r' && *(l->p+1) == '\n') { ladvance(l); ladvance(l); continue; }
        break;
    }
}

static Token make_tok(Lexer *l, TK_Type type) {
    return (Token){ .type=type, .line=l->line, .col=l->col,
                    .value=NULL, .value_len=0, .style=SCALAR_PLAIN };
}

static bool is_flow(Lexer *l) { return l->flow_depth > 0; }

/* ':' является MAP_VAL если за ним пробел/\n/\r/EOF или мы в flow */
static bool is_map_val_colon(Lexer *l) {
    char next = *(l->p + 1);
    return is_flow(l) || next == ' ' || next == '\t' ||
           next == '\n' || next == '\r' || next == '\0';
}

/* '-' является SEQ_ENTRY если за ним пробел/\n или начало строки */
static bool is_seq_entry_dash(Lexer *l) {
    char next = *(l->p + 1);
    return next == ' ' || next == '\t' || next == '\n' ||
           next == '\r' || next == '\0';
}

/* ── скаляры ───────────────────────────────────────────────────────── */

/* Одиночные кавычки: '' = экранированная ' */
static Token lex_single_quoted(Lexer *l) {
    Token t = make_tok(l, TK_SCALAR);
    t.style = SCALAR_SINGLE_QUOTED;
    ladvance(l); /* skip ' */
    const char *start = l->p;
    while (*l->p) {
        if (*l->p == '\'') {
            if (*(l->p+1) == '\'') { ladvance(l); ladvance(l); continue; }
            break;
        }
        ladvance(l);
    }
    t.value = start;
    t.value_len = (size_t)(l->p - start);
    if (*l->p == '\'') ladvance(l);
    return t;
}

/* Двойные кавычки: стандартные escape-последовательности YAML */
static Token lex_double_quoted(Lexer *l) {
    Token t = make_tok(l, TK_SCALAR);
    t.style = SCALAR_DOUBLE_QUOTED;
    ladvance(l); /* skip " */
    const char *start = l->p;
    while (*l->p && *l->p != '"') {
        if (*l->p == '\\') ladvance(l); /* skip escape */
        ladvance(l);
    }
    t.value = start;
    t.value_len = (size_t)(l->p - start);
    if (*l->p == '"') ladvance(l);
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
static Token lex_block_scalar(Lexer *l, ScalarStyle style) {
    Token t = make_tok(l, TK_SCALAR);
    t.style = style;
    ladvance(l); /* skip | or > */

    /* разбор заголовка */
    char chomping = 'c'; /* c=clip, '-'=strip, '+'=keep */
    int  explicit_indent = 0;
    while (*l->p == '-' || *l->p == '+' || (*l->p >= '1' && *l->p <= '9')) {
        if (*l->p == '-' || *l->p == '+') chomping = *l->p;
        else explicit_indent = *l->p - '0';
        ladvance(l);
    }
    skip_spaces(l);
    if (*l->p == '#') skip_line(l);
    if (*l->p == '\n') ladvance(l);

    /* определить базовый отступ */
    int base_indent = explicit_indent;
    if (base_indent == 0) {
        const char *pp = l->p;
        while (*pp == ' ') pp++;
        base_indent = (int)(pp - l->p);
    }

    /* собрать строки тела */
    size_t buf_cap = 256;
    char *buf = malloc(buf_cap);
    if (!buf) { l->error = "OOM"; t.value = ""; t.value_len = 0; return t; }
    size_t buf_len = 0;

    while (*l->p) {
        /* пустая строка или строка полностью из пробелов */
        const char *line_start = l->p;
        int spaces = 0;
        while (*l->p == ' ') { ladvance(l); spaces++; }
        if (*l->p == '\n' || *l->p == '\r' || *l->p == '\0') {
            /* пустая строка — добавить \n */
            if (buf_len + 1 >= buf_cap) {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap);
                if (!buf) { l->error = "OOM"; t.value = ""; t.value_len = 0; return t; }
            }
            buf[buf_len++] = '\n';
            if (*l->p == '\r') ladvance(l);
            if (*l->p == '\n') ladvance(l);
            continue;
        }
        /* строка с меньшим отступом — конец блока */
        if (spaces < base_indent) {
            l->p = line_start;
            l->col -= spaces;
            break;
        }
        /* пропустить base_indent пробелов */
        int extra = spaces - base_indent;
        /* добавить extra leading spaces (для literal) */
        while (buf_len + (size_t)extra + 1 >= buf_cap) {
            buf_cap *= 2;
            buf = realloc(buf, buf_cap);
            if (!buf) { l->error = "OOM"; t.value = ""; t.value_len = 0; return t; }
        }
        for (int i = 0; i < extra; i++) buf[buf_len++] = ' ';

        /* содержимое строки */
        while (*l->p && *l->p != '\n' && *l->p != '\r') {
            if (buf_len + 1 >= buf_cap) {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap);
                if (!buf) { l->error = "OOM"; t.value = ""; t.value_len = 0; return t; }
            }
            buf[buf_len++] = *l->p;
            ladvance(l);
        }
        if (buf_len + 1 >= buf_cap) {
            buf_cap *= 2;
            buf = realloc(buf, buf_cap);
            if (!buf) { l->error = "OOM"; t.value = ""; t.value_len = 0; return t; }
        }
        buf[buf_len++] = '\n';
        if (*l->p == '\r') ladvance(l);
        if (*l->p == '\n') ladvance(l);
    }

    /* chomping: удалить trailing newlines */
    if (chomping == '-') {
        while (buf_len > 0 && buf[buf_len-1] == '\n') buf_len--;
    } else if (chomping == 'c') {
        /* оставить ровно один \n в конце */
        while (buf_len > 1 && buf[buf_len-1] == '\n' && buf[buf_len-2] == '\n')
            buf_len--;
    }
    /* '+' — оставить как есть */

    buf[buf_len] = '\0';
    t.value = buf;        /* парсер освобождает этот буфер после strdup в YMLValue */
    t.value_len = buf_len;
    return t;
}

/*
 * Plain scalar: читаем до конца "безопасного" контента.
 * В блочном контексте: стоп на ': ', ' #', ',', '[', ']', '{', '}', '\n'
 * В flow-контексте: стоп также на ',' / ':' / ']' / '}'
 */
static Token lex_plain(Lexer *l) {
    Token t = make_tok(l, TK_SCALAR);
    t.style = SCALAR_PLAIN;
    const char *start = l->p;
    while (*l->p) {
        char c = *l->p;
        char n = *(l->p+1);
        if (c == '\n' || c == '\r') break;
        if (c == '#' && (l->p > start) && *(l->p-1) == ' ') break;
        if (is_flow(l) && (c == ',' || c == ']' || c == '}')) break;
        if (c == ':' && (n == ' ' || n == '\t' || n == '\n' || n == '\r' || n == '\0')) break;
        if (c == ':' && is_flow(l)) break;
        ladvance(l);
    }
    /* trim trailing spaces */
    const char *end = l->p;
    while (end > start && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
    t.value = start;
    t.value_len = (size_t)(end - start);
    return t;
}

/* &name или *name */
static Token lex_anchor_alias(Lexer *l, TK_Type type) {
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
static Token lex_tag(Lexer *l) {
    Token t = make_tok(l, TK_TAG);
    const char *start = l->p;
    ladvance(l); /* skip first ! */
    if (*l->p == '!') ladvance(l);          /* !!tag */
    else if (*l->p == '<') {                 /* !<uri> */
        while (*l->p && *l->p != '>') ladvance(l);
        if (*l->p == '>') ladvance(l);
    }
    while (*l->p && *l->p != ' ' && *l->p != '\t' &&
           *l->p != '\n' && *l->p != '\r') ladvance(l);
    t.value = start;
    t.value_len = (size_t)(l->p - start);
    return t;
}

/* ── главный цикл лексера ──────────────────────────────────────────── */

Token *lex(const char *src, const char **error_out) {
    Lexer l = { .src=src, .p=src, .line=1, .col=0, .flow_depth=0, .error=NULL };
    Token *tokens = da_new(Token, 64);
    if (!tokens) { if (error_out) *error_out = "OOM"; return NULL; }

    for (;;) {
        skip_ws(&l);
        int tok_line = l.line, tok_col = l.col;

        if (*l.p == '\0') {
            da_push(tokens, make_tok(&l, TK_EOF));
            break;
        }

        /* --- / ... в начале строки (col==0 или только пробелы до) */
        if (l.col == 0 && l.p[0] == '-' && l.p[1] == '-' && l.p[2] == '-' &&
            (l.p[3] == ' ' || l.p[3] == '\n' || l.p[3] == '\r' || l.p[3] == '\0')) {
            ladvance(&l); ladvance(&l); ladvance(&l);
            Token t = { .type=TK_DOC_START, .line=tok_line, .col=tok_col };
            da_push(tokens, t);
            continue;
        }
        if (l.col == 0 && l.p[0] == '.' && l.p[1] == '.' && l.p[2] == '.' &&
            (l.p[3] == ' ' || l.p[3] == '\n' || l.p[3] == '\r' || l.p[3] == '\0')) {
            ladvance(&l); ladvance(&l); ladvance(&l);
            Token t = { .type=TK_DOC_END, .line=tok_line, .col=tok_col };
            da_push(tokens, t);
            continue;
        }

        char c = lc(&l);

        /* flow indicators */
        if (c == '[') { ladvance(&l); l.flow_depth++;
            Token t = { .type=TK_FLOW_SEQ_START, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }
        if (c == ']') { ladvance(&l); if (l.flow_depth > 0) l.flow_depth--;
            Token t = { .type=TK_FLOW_SEQ_END, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }
        if (c == '{') { ladvance(&l); l.flow_depth++;
            Token t = { .type=TK_FLOW_MAP_START, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }
        if (c == '}') { ladvance(&l); if (l.flow_depth > 0) l.flow_depth--;
            Token t = { .type=TK_FLOW_MAP_END, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }
        if (c == ',' && is_flow(&l)) {
            ladvance(&l);
            Token t = { .type=TK_FLOW_ENTRY, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }

        /* block seq entry */
        if (c == '-' && !is_flow(&l) && is_seq_entry_dash(&l)) {
            ladvance(&l);
            Token t = { .type=TK_SEQ_ENTRY, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }

        /* explicit map key */
        if (c == '?' && (*(l.p+1) == ' ' || *(l.p+1) == '\n')) {
            ladvance(&l);
            Token t = { .type=TK_MAP_KEY, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }

        /* map value */
        if (c == ':' && is_map_val_colon(&l)) {
            ladvance(&l);
            Token t = { .type=TK_MAP_VAL, .line=tok_line, .col=tok_col };
            da_push(tokens, t); continue; }

        /* anchor / alias */
        if (c == '&') { Token t = lex_anchor_alias(&l, TK_ANCHOR); t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }
        if (c == '*') { Token t = lex_anchor_alias(&l, TK_ALIAS);  t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }

        /* tag */
        if (c == '!') { Token t = lex_tag(&l); t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }

        /* block scalars */
        if (c == '|') { Token t = lex_block_scalar(&l, SCALAR_LITERAL); t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }
        if (c == '>') { Token t = lex_block_scalar(&l, SCALAR_FOLDED);  t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }

        /* quoted scalars */
        if (c == '\'') { Token t = lex_single_quoted(&l); t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }
        if (c == '"')  { Token t = lex_double_quoted(&l);  t.line=tok_line; t.col=tok_col; da_push(tokens, t); continue; }

        /* plain scalar */
        {
            Token t = lex_plain(&l);
            t.line = tok_line; t.col = tok_col;
            da_push(tokens, t);
        }
    }

    if (l.error) {
        if (error_out) *error_out = l.error;
        da_free(tokens);
        return NULL;
    }
    if (error_out) *error_out = NULL;
    return tokens;
}
