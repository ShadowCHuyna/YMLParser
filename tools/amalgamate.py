#!/usr/bin/env python3
"""
tools/amalgamate.py — собирает YMLParser.h (single-header) из src/.

Запуск:
    python3 tools/amalgamate.py          # → YMLParser.h в корне проекта

Структура выходного файла:
    #ifndef YMLPARSER_H
      ... публичный API из src/YMLParser.h ...
    #ifdef YMLPARSER_IMPLEMENTATION
      ... все внутренние заголовки и .c файлы слиты в один блок ...
    #endif  // YMLPARSER_IMPLEMENTATION
    #endif  // YMLPARSER_H
"""

import os
import re
import sys
import argparse
from datetime import date

# ── пути ───────────────────────────────────────────────────────────────

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(ROOT, "src")
OUT  = os.path.join(ROOT, "YMLParser.h")

# ── порядок инлайна ────────────────────────────────────────────────────
# Сначала внутренние заголовки (типы + объявления),
# потом реализации (.c).

IMPL_FILES = [
	os.path.join(SRC, "_da.h"),
	os.path.join(SRC, "_hm.h"),
	os.path.join(SRC, "_lexer.h"),
	os.path.join(SRC, "_yml_utils.h"),
	os.path.join(SRC, "_da.c"),
	os.path.join(SRC, "_hm.c"),
	os.path.join(SRC, "_lexer.c"),
	os.path.join(SRC, "YMLParser.c"),
]

# Строки #include, которые нужно выбросить (всё уже инлайнено).
STRIP_INCLUDES = {
	'#include "YMLParser.h"',
	'#include "_da.h"',
	'#include "_hm.h"',
	'#include "_lexer.h"',
	'#include "_yml_utils.h"',
}

# ── обработка одного файла ─────────────────────────────────────────────

def inline_file(path: str) -> str:
	"""
	Читает файл и убирает:
		- #pragma once
		- внутренние #include "..."
	Системные #include <...> оставляет — они идемпотентны.
	"""
	with open(path, encoding="utf-8") as f:
		content = f.read()

	# убрать #pragma once
	content = re.sub(r'[ \t]*#pragma once[ \t]*\n', '', content)

	# убрать внутренние includes
	for inc in STRIP_INCLUDES:
		content = re.sub(
			r'[ \t]*' + re.escape(inc) + r'[ \t]*\n',
			'',
			content,
		)

	return content


# ── генерация ──────────────────────────────────────────────────────────

SEPARATOR = "/* {name} {dashes} */\n"

def separator(rel_path: str) -> str:
	name = rel_path.replace("\\", "/")
	dashes = "─" * max(0, 60 - len(name))
	return f"/* ── {name} {dashes} */\n"


def generate() -> str:
	parts = []

	# ── шапка файла ────────────────────────────────────────────────────
	parts.append(f"""\
/*
 * YMLParser.h — single-header YAML 1.2.2 parser (C11).
 *
 * ── Использование ──────────────────────────────────────────────────
 *
 *   В ОДНОМ .c-файле перед include:
 *     #define YMLPARSER_IMPLEMENTATION
 *     #include "YMLParser.h"
 *
 *   Во всех остальных файлах:
 *     #include "YMLParser.h"
 *
 * ── Требования ─────────────────────────────────────────────────────
 *
 *   Компилятор: C11 или новее  (_Thread_local, <stdbool.h>, <stdint.h>)
 *   Флаги:      -lm            (HUGE_VAL / NAN из <math.h>)
 */
""")

	# ── публичный API ──────────────────────────────────────────────────
	# Берём из src/YMLParser.h — источник истины для API.
	public_api_path = os.path.join(SRC, "YMLParser.h")
	public_api = inline_file(public_api_path)  # убирает #pragma once, внутренних include там нет

	parts.append("#ifndef YMLPARSER_H\n#define YMLPARSER_H\n\n")
	parts.append("/* ════════════════════════ PUBLIC API ════════════════════════ */\n\n")
	parts.append(public_api.rstrip("\n") + "\n\n")

	# ── блок реализации ────────────────────────────────────────────────
	parts.append("#ifdef YMLPARSER_IMPLEMENTATION\n")
	parts.append(
		"/* YML_PRIVATE — скрывает внутренние функции. */\n"
		"#define YML_PRIVATE static\n"
	)

	# _POSIX_C_SOURCE нужен до системных <include> для strdup

	# инлайн файлов
	for path in IMPL_FILES:
		rel = os.path.relpath(path, ROOT)
		parts.append(separator(rel))

		content = inline_file(path)


		parts.append(content.strip("\n") + "\n\n")

	# parts.append("#endif /* YMLPARSER_IMPLEMENTATION_DONE */\n")
	parts.append("#endif /* YMLPARSER_IMPLEMENTATION */\n\n")
	parts.append("#endif /* YMLPARSER_H */\n")

	return "".join(parts)


# ── точка входа ────────────────────────────────────────────────────────

def main():
	parser = argparse.ArgumentParser(description=__doc__,
										formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("--check", action="store_true",
						help="проверить актуальность YMLParser.h (для CI), "
								"выйти с кодом 1 если файл устарел")
	args = parser.parse_args()

	result = generate()

	if args.check:
		if not os.path.exists(OUT):
			print(f"FAIL: {OUT} не существует — запусти make single-header")
			sys.exit(1)
		with open(OUT, encoding="utf-8") as f:
			current = f.read()
		# Сравниваем без строки с датой (она меняется каждый день)
		def strip_date(s):
			return re.sub(r'Generated \d{4}-\d{2}-\d{2}', 'Generated DATE', s)
		if strip_date(current) != strip_date(result):
			print(f"FAIL: {OUT} устарел — запусти make single-header")
			sys.exit(1)
		print(f"OK: {os.path.relpath(OUT, ROOT)} актуален")
		return

	with open(OUT, "w", encoding="utf-8") as f:
		f.write(result)

	lines = result.count("\n")
	print(f"Wrote {os.path.relpath(OUT, ROOT)}  ({lines} lines)")


if __name__ == "__main__":
	main()
