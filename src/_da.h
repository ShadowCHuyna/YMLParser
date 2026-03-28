#pragma once
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
