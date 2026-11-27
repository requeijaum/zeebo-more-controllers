/* string.h for the BREW build: the module links with -nostdinc and no libc, so the few functions
 * the portable core uses come from src/brew/zhp_libc.c. Host builds use the real <string.h>. */
#ifndef ZHP_SHIM_STRING_H
#define ZHP_SHIM_STRING_H
#include <stddef.h>
void *memset(void *p, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
#endif
