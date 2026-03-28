#include "_da.h"
#include <stdlib.h>
#include <string.h>

void *_da_new(size_t elem_size, size_t cap) {
    if (cap == 0) cap = 4;
    _da_hdr *hdr = malloc(sizeof(_da_hdr) + elem_size * cap);
    if (!hdr) return NULL;
    hdr->len = 0;
    hdr->cap = cap;
    return hdr + 1;
}

void *_da_push(void *da, const void *elem, size_t elem_size) {
    _da_hdr *hdr = (_da_hdr *)da - 1;
    if (hdr->len == hdr->cap) {
        hdr->cap *= 2;
        hdr = realloc(hdr, sizeof(_da_hdr) + elem_size * hdr->cap);
        if (!hdr) return da; /* TODO: propagate OOM */
    }
    memcpy((char *)(hdr + 1) + elem_size * hdr->len, elem, elem_size);
    hdr->len++;
    return hdr + 1;
}

void _da_free(void *da) {
    if (!da) return;
    free((_da_hdr *)da - 1);
}
