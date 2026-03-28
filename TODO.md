# YMLParser TODO

## [X] доработка api
Итоговые решения:
- `YML_INT` (int64_t) + `YML_FLOAT` (double) вместо `YML_NUMBER`
- `YML_ANY = -1` — сентинел для `_YMLOptionals.type` (по умолчанию)
- `YMLParseStream` → `da<YMLValue*>` для multi-document
- complex keys не поддерживаются (только строковые)
- теги: встроенные (`!!str`, `!!int`, `!!float`, `!!bool`, `!!null`) — да; кастомные — игнорируются
- якоря/алиасы разворачивает парсер, API о них не знает
- merge key `<<` — поддерживается парсером
- `value.array` — `da<YMLValue>` со скрытым заголовком, прямая индексация `arr[i]`
- `value.object` — `void*` (_hm*), доступ только через `YMLMapGet`/`YMLMapForech`

## [X] архитектура парсера
Файловая структура:
```
src/
  YMLParser.h        публичный API
  YMLParser.c        реализация API + рекурсивный парсер
  _da.h / _da.c      dynamic array со скрытым заголовком (len+cap)
  _hm.h / _hm.c      hash map с открытой адресацией, строковые ключи
  _lexer.h / _lexer.c лексер → da<Token>
```

Поток обработки:
```
const char* → lex() → da<Token> → parse_node() → YMLValue*
```

Парсер — рекурсивный спуск по da<Token>:
- `parse_node(min_indent)`     — точка входа для любого значения
- `parse_block_mapping(indent)` — ключи с одинаковым отступом
- `parse_block_sequence(indent)`— элементы `-` с одинаковым отступом
- `parse_flow_mapping()`        — внутри `{}`
- `parse_flow_sequence()`       — внутри `[]`
- `parse_scalar()`              — plain, quoted, block (|/>)

Инференс типа plain-скаляра (YAML Core Schema):
```
""  ~  null  Null  NULL       → YML_NULL
true True TRUE / false ...    → YML_BOOL
[-+]?[0-9]+  0x...  0o...     → YML_INT
[-+]?[0-9]*\.[0-9]+  .inf .nan→ YML_FLOAT
всё остальное                 → YML_STRING
```

Якоря/алиасы:
- `&name` — сохранить указатель построенного узла в `da<{name, *node}>`
- `*name` — найти, вернуть тот же указатель (не владеет — не освобождать дважды при Destroy)

Память: только `malloc`/`realloc`/`free`/`strdup`.

## [ ] реализация парсера
Порядок реализации:
1. `_da.c`     — `_da_new`, `_da_push`, `_da_free`
2. `_hm.c`     — `hm_new`, `hm_set`, `hm_get`, `hm_next`, `hm_free`
3. `_lexer.c`  — `lex()`: plain scalars, quoted, block scalars (|/>), flow-индикаторы
4. `YMLParser.c` — `infer_scalar`, `parse_node` и производные

## [ ] тесты
