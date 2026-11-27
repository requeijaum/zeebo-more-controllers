/* The libc functions the portable core needs (and that gcc may call for struct copies). */
#include <stddef.h>

void *memset(void *p, int c, size_t n)
{
    unsigned char *q = (unsigned char *)p;
    while (n--) {
        *q++ = (unsigned char)c;
    }
    return p;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *a = (unsigned char *)d;
    const unsigned char *b = (const unsigned char *)s;
    while (n--) {
        *a++ = *b++;
    }
    return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
    const unsigned char *a = (const unsigned char *)x, *b = (const unsigned char *)y;
    while (n--) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        a++;
        b++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}
