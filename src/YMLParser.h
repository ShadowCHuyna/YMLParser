#pragma once
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
} __da_header; /* должен совпадать с _da_hdr из _da.h */

/*
 * Возвращает количество элементов в da-массиве.
 * Пример: for (size_t i = 0; i < ArrayLen(arr); i++) { ... arr[i] ... }
 */
#define ArrayLen(arr) ((arr) ? ((__da_header *)(arr) - 1)->len : (size_t)0)

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
