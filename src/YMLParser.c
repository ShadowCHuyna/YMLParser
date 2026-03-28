#include "YMLParser.h"
#include "_da.h"
#include "_hm.h"
#include "_lexer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* ── глобальное состояние последней ошибки ─────────────────────────── */

static int   g_ok;
static char  g_error[256];

static void set_error(int code, const char *msg) {
    g_ok = code;
    snprintf(g_error, sizeof(g_error), "%s", msg);
}

int YMLErrorPrint(void) {
    if (g_ok == 0) return 0;
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
typedef struct { char *name; YMLValue *node; } _anchor_entry;

typedef struct {
    Token          *tokens;  /* da<Token> от лексера */
    size_t          pos;     /* индекс текущего токена */

    _anchor_entry  *anchors; /* da<_anchor_entry> */

    int             ok;
    char            error[256];
} Parser;

/* ── утилиты парсера ───────────────────────────────────────────────── */

static Token *cur(Parser *p) { return &p->tokens[p->pos]; }
static Token *peek(Parser *p, size_t offset) { return &p->tokens[p->pos + offset]; }
static void   advance(Parser *p) { if (cur(p)->type != TK_EOF) p->pos++; }

static void parse_error(Parser *p, const char *msg) {
    Token *t = cur(p);
    snprintf(p->error, sizeof(p->error),
             "line %d col %d: %s", t->line, t->col, msg);
    p->ok = 1;
}

/* Скопировать value/value_len токена в heap (NUL-terminated). */
static char *token_strdup(const Token *t) {
    char *s = malloc(t->value_len + 1);
    if (!s) return NULL;
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

/* ── публичные функции ─────────────────────────────────────────────── */

YMLValue *_YMLParse(const char *yml_str, struct _YMLOptionals optionals) {
    g_ok = 0;
    g_error[0] = '\0';

    const char *lex_err = NULL;
    Token *tokens = lex(yml_str, &lex_err);
    if (!tokens) {
        set_error(1, lex_err ? lex_err : "lexer error");
        if (optionals.ok)    *optionals.ok    = g_ok;
        if (optionals.error) *optionals.error = g_error;
        return NULL;
    }

    Parser p = {
        .tokens  = tokens,
        .pos     = 0,
        .anchors = da_new(_anchor_entry, 8),
        .ok      = 0,
    };

    /* пропустить необязательный --- */
    if (cur(&p)->type == TK_DOC_START) advance(&p);

    YMLValue *root = parse_node(&p, 0);

    /* синхронизировать ошибку в глобальное состояние */
    if (p.ok != 0) {
        set_error(p.ok, p.error);
        root = NULL; /* TODO: освободить частично построенное дерево */
    }

    da_free(p.anchors);
    da_free(tokens);

    if (optionals.ok)    *optionals.ok    = g_ok;
    if (optionals.error) *optionals.error = g_error;
    return root;
}

YMLValue **_YMLParseStream(const char *yml_str, struct _YMLOptionals optionals) {
    (void)yml_str; (void)optionals;
    /* TODO */
    return NULL;
}

/* Рекурсивно освободить одно значение (не сам указатель root). */
static void yml_value_free(YMLValue *v) {
    if (!v) return;
    switch (v->type) {
    case YML_STRING:
        free((char*)v->value.string);
        break;
    case YML_ARRAY: {
        size_t n = da_len(v->value.array);
        for (size_t i = 0; i < n; i++)
            yml_value_free(&v->value.array[i]);
        da_free(v->value.array);
        break;
    }
    case YML_OBJECT:
        hm_free((_hm*)v->value.object);
        break;
    default:
        break;
    }
}

void _YMLDestroy(YMLValue *root, struct _YMLOptionals optionals) {
    if (!root) return;
    yml_value_free(root);
    free(root);
    if (optionals.ok) *optionals.ok = 0;
}

void _YMLDestroyStream(YMLValue **stream, struct _YMLOptionals optionals) {
    if (!stream) return;
    size_t n = da_len(stream);
    for (size_t i = 0; i < n; i++)
        _YMLDestroy(stream[i], (struct _YMLOptionals){0});
    da_free(stream);
    if (optionals.ok) *optionals.ok = 0;
}

YMLValue *_YMLMapGet(YMLValue *object, const char *key, struct _YMLOptionals optionals) {
    if (!object || object->type != YML_OBJECT) {
        set_error(1, "YMLMapGet: not an object");
        if (optionals.ok)    *optionals.ok    = g_ok;
        if (optionals.error) *optionals.error = g_error;
        return NULL;
    }
    YMLValue *v = hm_get((_hm*)object->value.object, key);
    if (!v) {
        snprintf(g_error, sizeof(g_error), "YMLMapGet: key '%s' not found", key);
        g_ok = 1;
        if (optionals.ok)    *optionals.ok    = g_ok;
        if (optionals.error) *optionals.error = g_error;
        return NULL;
    }
    if (optionals.type != YML_ANY && v->type != optionals.type) {
        snprintf(g_error, sizeof(g_error),
                 "YMLMapGet: key '%s' has type %d, expected %d",
                 key, v->type, optionals.type);
        g_ok = 2;
        if (optionals.ok)    *optionals.ok    = g_ok;
        if (optionals.error) *optionals.error = g_error;
        return NULL;
    }
    if (optionals.ok) *optionals.ok = 0;
    return v;
}

_YMLMapIter _YMLMapIterBegin(YMLValue *object) {
    void *hm = (object && object->type == YML_OBJECT) ? object->value.object : NULL;
    return (_YMLMapIter){ ._hm = hm, ._i = 0 };
}

bool _YMLMapIterNext(_YMLMapIter *iter, const char **key, YMLValue **value) {
    if (!iter || !iter->_hm) return false;
    return hm_next((_hm*)iter->_hm, &iter->_i, key, value);
}
