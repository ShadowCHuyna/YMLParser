#include "_hm.h"
#include "_da.h"
#include <stdlib.h>
#include <string.h>

/* djb2 hash */
static size_t hash_str(const char *s) {
    size_t h = 5381;
    while (*s) h = h * 33 ^ (unsigned char)*s++;
    return h;
}

/* Округлить cap до ближайшей степени двойки >= cap */
static size_t next_pow2(size_t n) {
    if (n == 0) return 4;
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16; n |= n >> 32;
    return n + 1;
}

_hm *hm_new(size_t cap) {
    _hm *hm = malloc(sizeof(_hm));
    if (!hm) return NULL;
    cap = next_pow2(cap < 4 ? 4 : cap);
    hm->entries = calloc(cap, sizeof(_hm_entry));
    if (!hm->entries) { free(hm); return NULL; }
    hm->cap = cap;
    hm->len = 0;
    return hm;
}

/* Перехеширование при load factor > 0.7 */
static bool hm_grow(_hm *hm) {
    size_t new_cap = hm->cap * 2;
    _hm_entry *new_entries = calloc(new_cap, sizeof(_hm_entry));
    if (!new_entries) return false;
    for (size_t i = 0; i < hm->cap; i++) {
        if (!hm->entries[i].key) continue;
        size_t idx = hash_str(hm->entries[i].key) & (new_cap - 1);
        while (new_entries[idx].key) idx = (idx + 1) & (new_cap - 1);
        new_entries[idx] = hm->entries[i];
    }
    free(hm->entries);
    hm->entries = new_entries;
    hm->cap = new_cap;
    return true;
}

bool hm_set(_hm *hm, const char *key, YMLValue value) {
    if (hm->len * 10 >= hm->cap * 7) /* load > 0.7 */
        if (!hm_grow(hm)) return false;

    size_t idx = hash_str(key) & (hm->cap - 1);
    while (hm->entries[idx].key) {
        if (strcmp(hm->entries[idx].key, key) == 0) {
            hm->entries[idx].value = value; /* обновить */
            return true;
        }
        idx = (idx + 1) & (hm->cap - 1);
    }
    hm->entries[idx].key = strdup(key);
    if (!hm->entries[idx].key) return false;
    hm->entries[idx].value = value;
    hm->len++;
    return true;
}

YMLValue *hm_get(const _hm *hm, const char *key) {
    size_t idx = hash_str(key) & (hm->cap - 1);
    while (hm->entries[idx].key) {
        if (strcmp(hm->entries[idx].key, key) == 0)
            return &hm->entries[idx].value;
        idx = (idx + 1) & (hm->cap - 1);
    }
    return NULL;
}

bool hm_next(const _hm *hm, size_t *idx, const char **key, YMLValue **value) {
    while (*idx < hm->cap) {
        _hm_entry *e = &hm->entries[(*idx)++];
        if (e->key) {
            *key   = e->key;
            *value = &e->value;
            return true;
        }
    }
    return false;
}

/* Рекурсивное освобождение значения (без free самого указателя) */
static void free_value(YMLValue *v) {
    if (!v) return;
    switch (v->type) {
    case YML_STRING: free((char *)v->value.string); break;
    case YML_ARRAY: {
        size_t n = da_len(v->value.array);
        for (size_t i = 0; i < n; i++) free_value(&v->value.array[i]);
        da_free(v->value.array);
        break;
    }
    case YML_OBJECT: hm_free((_hm *)v->value.object); break;
    default: break;
    }
}

void hm_free(_hm *hm) {
    if (!hm) return;
    for (size_t i = 0; i < hm->cap; i++) {
        if (!hm->entries[i].key) continue;
        free(hm->entries[i].key);
        free_value(&hm->entries[i].value);
    }
    free(hm->entries);
    free(hm);
}
