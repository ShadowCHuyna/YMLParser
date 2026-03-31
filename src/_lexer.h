#pragma once
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
#include <stdbool.h>
#include <stddef.h>

#ifndef YML_PRIVATE
#	define YML_PRIVATE
#endif

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
	bool owns_value;   /* true = value указывает на malloc-буфер лексера, парсер обязан free() */
} Token;

/*
 * Лексировать src и вернуть da<Token>.
 * При синтаксической ошибке:
 *   если error != NULL — записывает сообщение в *error (static буфер)
 *   возвращает NULL
 *
 * Освобождение: da_free() — токены не владеют строками (value указывает в src).
 */
YML_PRIVATE Token *lex(const char *src, const char **error);
