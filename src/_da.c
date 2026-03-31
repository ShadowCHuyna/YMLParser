#include "_da.h"
#include <stdlib.h>
#include <string.h>

void *_da_new(size_t elem_size, size_t cap)
{
	if (cap == 0)
		cap = 4;
	_da_hdr *hdr = malloc(sizeof(_da_hdr) + elem_size * cap);
	if (!hdr)
		return NULL;
	hdr->len = 0;
	hdr->cap = cap;
	return hdr + 1;
}

void *_da_push(void *da, const void *elem, size_t elem_size)
{
	_da_hdr *hdr = (_da_hdr *)da - 1;
	if (hdr->len == hdr->cap)
	{
		size_t new_cap = hdr->cap * 2;
		_da_hdr *new_hdr = realloc(hdr, sizeof(_da_hdr) + elem_size * new_cap);
		if (!new_hdr)
			return NULL; /* старый блок жив, cap не тронут */
		new_hdr->cap = new_cap;
		hdr = new_hdr;
	}
	memcpy((char *)(hdr + 1) + elem_size * hdr->len, elem, elem_size);
	hdr->len++;
	return hdr + 1;
}

void _da_free(void *da)
{
	if (!da)
		return;
	free((_da_hdr *)da - 1);
}
