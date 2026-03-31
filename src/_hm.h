#pragma once
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
 *
 * ВНИМАНИЕ: указатель инвалидируется при любом последующем hm_set на той же карте,
 * если он вызывает перераспределение (load > 0.7). Не сохраняйте результат
 * за пределами текущего блока кода.
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
