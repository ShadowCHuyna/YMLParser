/*
 * example_threads.c — парсинг YAML из нескольких потоков одновременно.
 *
 * Демонстрирует:
 *   - _Thread_local ошибки: каждый поток видит только свои ошибки
 *   - независимые деревья YMLValue на каждый поток
 *   - безопасное освобождение памяти внутри потока
 *
 * Сборка:
 *   make run-example E=example_threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

#define NTHREADS 6

/* Входные данные для каждого потока. */
typedef struct
{
	int id;
	const char *yaml;
	/* Результаты, записываемые потоком. */
	char name[64];
	long long value;
	int ok;
} WorkItem;

static WorkItem work[NTHREADS] = {
	{0, "name: Alpha\nvalue: 100\n"},
	{1, "name: Beta\nvalue:  200\n"},
	{2, "name: Gamma\nvalue: 300\n"},
	{3, "name: bad yaml: [unclosed\n"}, /* намеренная ошибка */
	{4, "name: Delta\nvalue: 400\n"},
	{5, "name: wrong_type\nvalue: 'str'\n"}, /* несовпадение типа */
};

static void *parse_worker(void *arg)
{
	WorkItem *w = arg;
	int ok = 0;
	char *err = NULL;

	YMLValue *root = YMLParse(w->yaml, .ok = &ok, .error = &err);
	if (ok != 0)
	{
		w->ok = ok;
		snprintf(w->name, sizeof(w->name), "(parse error) %s", err);
		return NULL;
	}

	YMLValue *name = YMLMapGet(root->value.object, "name",
							   .ok = &ok, .type = YML_STRING, .error = &err);
	if (ok != 0)
	{
		w->ok = ok;
		snprintf(w->name, sizeof(w->name), "(no name) %s", err);
		YMLDestroy(root);
		return NULL;
	}
	strncpy(w->name, name->value.string, sizeof(w->name) - 1);

	YMLValue *val = YMLMapGet(root->value.object, "value",
							  .ok = &ok, .type = YML_INT, .error = &err);
	if (ok != 0)
	{
		/* Ошибка только у этого потока — у остальных ok=0. */
		w->ok = ok;
		snprintf(w->name + strlen(w->name),
				 sizeof(w->name) - strlen(w->name),
				 " (value error: %s)", err);
		YMLDestroy(root);
		return NULL;
	}

	w->value = val->value.integer;
	w->ok = 0;
	YMLDestroy(root);
	return NULL;
}

int main(void)
{
	pthread_t threads[NTHREADS];

	/* Запускаем все потоки одновременно. */
	for (int i = 0; i < NTHREADS; i++)
	{
		work[i].id = i;
		pthread_create(&threads[i], NULL, parse_worker, &work[i]);
	}

	/* Ждём завершения. */
	for (int i = 0; i < NTHREADS; i++)
		pthread_join(threads[i], NULL);

	/* Печатаем результаты. */
	printf("%-3s  %-6s  %-30s  %s\n", "id", "ok", "name", "value");
	printf("%-3s  %-6s  %-30s  %s\n", "---", "------",
		   "------------------------------", "--------");
	for (int i = 0; i < NTHREADS; i++)
	{
		if (work[i].ok == 0)
			printf("%-3d  ok      %-30s  %lld\n", i, work[i].name, work[i].value);
		else
			printf("%-3d  err=%-2d  %s\n", i, work[i].ok, work[i].name);
	}

	/*
	 * Итоговая сумма по успешным потокам.
	 * Демонстрирует, что ошибки потока 3 и 5 не затронули остальных.
	 */
	long long sum = 0;
	int good = 0;
	for (int i = 0; i < NTHREADS; i++)
		if (work[i].ok == 0)
		{
			sum += work[i].value;
			good++;
		}

	printf("\nУспешно: %d/%d   сумма value = %lld\n", good, NTHREADS, sum);
	return 0;
}
