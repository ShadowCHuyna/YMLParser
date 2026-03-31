/*
 * example_nested.c — глубокие структуры, массивы объектов,
 *                    рекурсивный обход, теги, якоря/merge.
 *
 * Сборка:
 *   make run-example E=example_nested
 */

#include <stdio.h>
#include <string.h>
#define YMLPARSER_IMPLEMENTATION
#include "YMLParser.h"

/* ─────────────────────────────────────────────────────────────────────
 * Конфигурация микросервиса: массив сервисов, каждый — объект,
 * общие дефолты вынесены через merge key.
 * ───────────────────────────────────────────────────────────────────── */
static const char *config =
	"defaults: &defaults\n"
	"  timeout: 30\n"
	"  retries: 3\n"
	"  tls: false\n"
	"\n"
	"services:\n"
	"  - name: auth\n"
	"    host: auth.internal\n"
	"    port: 8001\n"
	"    <<: *defaults\n"
	"    tls: true\n" /* перекрывает defaults.tls */
	"\n"
	"  - name: storage\n"
	"    host: storage.internal\n"
	"    port: 8002\n"
	"    <<: *defaults\n"
	"\n"
	"  - name: gateway\n"
	"    host: 0.0.0.0\n"
	"    port: 443\n"
	"    <<: *defaults\n"
	"    timeout: 60\n" /* перекрывает defaults.timeout */
	"    routes:\n"
	"      - path: /api/v1\n"
	"        backend: auth\n"
	"      - path: /storage\n"
	"        backend: storage\n"
	"\n"
	"database:\n"
	"  primary:\n"
	"    host: db1.internal\n"
	"    port: 5432\n"
	"    pool: 10\n"
	"  replicas:\n"
	"    - host: db2.internal\n"
	"      port: 5432\n"
	"      pool: 5\n"
	"    - host: db3.internal\n"
	"      port: 5433\n"
	"      pool: 5\n";

/* Рекурсивно напечатать любое значение с отступом. */
static void print_value(const YMLValue *v, int indent)
{
	if (!v)
	{
		printf("(null pointer)\n");
		return;
	}
	switch (v->type)
	{
	case YML_NULL:
		printf("~\n");
		break;
	case YML_BOOL:
		printf("%s\n", v->value.boolean ? "true" : "false");
		break;
	case YML_INT:
		printf("%lld\n", (long long)v->value.integer);
		break;
	case YML_FLOAT:
		printf("%g\n", v->value.number);
		break;
	case YML_STRING:
		printf("'%s'\n", v->value.string);
		break;
	case YML_ARRAY:
	{
		printf("[\n");
		for (size_t i = 0; i < ArrayLen(v->value.array); i++)
		{
			printf("%*s  [%zu] ", indent, "", i);
			print_value(&v->value.array[i], indent + 4);
		}
		printf("%*s]\n", indent, "");
		break;
	}
	case YML_OBJECT:
	{
		printf("{\n");
		YMLMapForech(v->value.object, key, val)
		{
			printf("%*s  %-12s ", indent, "", key);
			print_value(val, indent + 4);
		}
		printf("%*s}\n", indent, "");
		break;
	}
	default:
		printf("?\n");
	}
}

int main(void)
{
	int ok = 0;
	char *err = NULL;

	YMLValue *root = YMLParse(config, .ok = &ok, .error = &err);
	if (ok != 0)
	{
		fprintf(stderr, "parse error %d: %s\n", ok, err);
		return 1;
	}

	/* ── обход массива сервисов ── */
	printf("=== Сервисы ===\n");
	YMLValue *services = YMLMapGet(root->value.object, "services", .ok = &ok, .type = YML_ARRAY);
	if (ok != 0)
	{
		YMLErrorPrint();
		YMLDestroy(root);
		return 1;
	}

	for (size_t i = 0; i < ArrayLen(services->value.array); i++)
	{
		YMLValue *svc = &services->value.array[i];
		if (svc->type != YML_OBJECT)
			continue;

		YMLValue *name = YMLMapGet(svc->value.object, "name", .ok = &ok);
		YMLValue *host = YMLMapGet(svc->value.object, "host", .ok = &ok);
		YMLValue *port = YMLMapGet(svc->value.object, "port", .ok = &ok);
		YMLValue *timeout = YMLMapGet(svc->value.object, "timeout", .ok = &ok);
		YMLValue *retries = YMLMapGet(svc->value.object, "retries", .ok = &ok);
		YMLValue *tls = YMLMapGet(svc->value.object, "tls", .ok = &ok);
		if (ok != 0)
		{
			fprintf(stderr, "missing field: %s\n", err);
			break;
		}

		printf("  %-10s  %s:%-5lld  timeout=%-3lld  retries=%lld  tls=%s\n",
			   name->value.string,
			   host->value.string,
			   (long long)port->value.integer,
			   (long long)timeout->value.integer,
			   (long long)retries->value.integer,
			   tls->value.boolean ? "yes" : "no");

		/* routes — необязательное поле */
		YMLValue *routes = YMLMapGet(svc->value.object, "routes", .ok = &ok);
		if (ok == 0 && routes->type == YML_ARRAY)
		{
			for (size_t r = 0; r < ArrayLen(routes->value.array); r++)
			{
				YMLValue *route = &routes->value.array[r];
				YMLValue *path = YMLMapGet(route->value.object, "path", .ok = &ok);
				YMLValue *backend = YMLMapGet(route->value.object, "backend", .ok = &ok);
				if (ok == 0)
					printf("    route: %-15s → %s\n", path->value.string, backend->value.string);
			}
		}
	}

	/* ── база данных: primary + replicas ── */
	printf("\n=== База данных ===\n");
	YMLValue *db = YMLMapGet(root->value.object, "database", .ok = &ok);
	YMLValue *primary = YMLMapGet(db->value.object, "primary", .ok = &ok);
	if (ok != 0)
	{
		YMLErrorPrint();
		YMLDestroy(root);
		return 1;
	}

	YMLValue *ph = YMLMapGet(primary->value.object, "host", .ok = &ok);
	YMLValue *pp = YMLMapGet(primary->value.object, "port", .ok = &ok);
	YMLValue *pool = YMLMapGet(primary->value.object, "pool", .ok = &ok);
	printf("  primary: %s:%lld  pool=%lld\n",
		   ph->value.string,
		   (long long)pp->value.integer,
		   (long long)pool->value.integer);

	YMLValue *replicas = YMLMapGet(db->value.object, "replicas", .ok = &ok);
	if (ok == 0)
	{
		for (size_t i = 0; i < ArrayLen(replicas->value.array); i++)
		{
			YMLValue *r = &replicas->value.array[i];
			YMLValue *rh = YMLMapGet(r->value.object, "host", .ok = &ok);
			YMLValue *rp = YMLMapGet(r->value.object, "port", .ok = &ok);
			YMLValue *rpoo = YMLMapGet(r->value.object, "pool", .ok = &ok);
			if (ok == 0)
				printf("  replica  %s:%lld  pool=%lld\n",
					   rh->value.string,
					   (long long)rp->value.integer,
					   (long long)rpoo->value.integer);
		}
	}

	/* ── рекурсивный dump всего дерева ── */
	printf("\n=== Полный дамп дерева ===\n");
	print_value(root, 0);

	YMLDestroy(root);
	return 0;
}
