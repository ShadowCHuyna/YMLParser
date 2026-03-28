#include <stdio.h>
#include "YMLParser.h"

static const char *yml =
    "# Пример YAML-документа\n"
    "name: Alice\n"
    "age: 30\n"
    "score: 9.5\n"
    "active: true\n"
    "nothing: ~\n"
    "\n"
    "address:\n"
    "  city: Moscow\n"
    "  zip: '101000'\n"
    "\n"
    "tags:\n"
    "  - developer\n"
    "  - yaml\n"
    "\n"
    "hex_color: 0xFF5733\n"
    "description: |\n"
    "  Многострочное\n"
    "  описание\n"
    "\n"
    "defaults: &d\n"
    "  timeout: 30\n"
    "  retries: 3\n"
    "\n"
    "service:\n"
    "  <<: *d\n"
    "  host: localhost\n";

int main(void) {
    int ok = 0;
    char *error = NULL;

    YMLValue *root = YMLParse(yml, .ok=&ok, .error=&error);
    if (ok != 0) {
        printf("parse error %d: %s\n", ok, error);
        return ok;
    }

    /* скалярные типы */
    YMLValue *name    = YMLMapGet(root->value.object, "name",    .ok=&ok);
    YMLValue *age     = YMLMapGet(root->value.object, "age",     .ok=&ok);
    YMLValue *score   = YMLMapGet(root->value.object, "score",   .ok=&ok);
    YMLValue *active  = YMLMapGet(root->value.object, "active",  .ok=&ok);
    YMLValue *nothing = YMLMapGet(root->value.object, "nothing", .ok=&ok);
    YMLValue *hex     = YMLMapGet(root->value.object, "hex_color", .ok=&ok);
    YMLValue *desc    = YMLMapGet(root->value.object, "description", .ok=&ok);
    printf("name:    %s\n",  name->value.string);
    printf("age:     %lld\n", (long long)age->value.integer);
    printf("score:   %g\n",  score->value.number);
    printf("active:  %s\n",  active->value.boolean ? "true" : "false");
    printf("nothing: %s\n",  nothing->type == YML_NULL ? "null" : "?");
    printf("hex:     %lld (0x%llX)\n", (long long)hex->value.integer,
                                        (unsigned long long)hex->value.integer);
    printf("desc:    %s", desc->value.string);

    /* вложенный объект */
    YMLValue *addr = YMLMapGet(root->value.object, "address", .ok=&ok);
    YMLValue *city = YMLMapGet(addr->value.object, "city", .ok=&ok);
    YMLValue *zip  = YMLMapGet(addr->value.object, "zip",  .ok=&ok);
    printf("city:    %s\n", city->value.string);
    printf("zip:     %s (string, not int)\n", zip->value.string);

    /* массив */
    YMLValue *tags = YMLMapGet(root->value.object, "tags", .ok=&ok);
    printf("tags:    ");
    for (size_t i = 0; i < ArrayLen(tags->value.array); i++)
        printf("%s%s", tags->value.array[i].value.string,
               i+1 < ArrayLen(tags->value.array) ? ", " : "\n");

    /* итерация по всем ключам */
    printf("\nВсе ключи:");
    YMLMapForech(root->value.object, key, val) {
        printf(" %s", key);
    }
    printf("\n");

    /* merge key — service унаследовал timeout и retries */
    YMLValue *svc = YMLMapGet(root->value.object, "service", .ok=&ok);
    YMLValue *timeout = YMLMapGet(svc->value.object, "timeout", .ok=&ok);
    YMLValue *host    = YMLMapGet(svc->value.object, "host",    .ok=&ok);
    printf("\nservice.timeout: %lld (из merge)\n", (long long)timeout->value.integer);
    printf("service.host:    %s\n", host->value.string);

    YMLDestroy(root);
    return 0;
}
