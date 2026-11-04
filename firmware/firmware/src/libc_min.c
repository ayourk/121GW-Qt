/*
 * The few C library functions the application uses (it links without a C
 * library; the original takes them from the IAR runtime: strlen 0x08023CA0).
 */
#include <stddef.h>
#include <string.h>

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p != 0) {
        p++;
    }
    return (size_t)(p - s);
}

/*
 * Custom: the string functions FatFs R0.16 calls (common/fatfs-r016/ff.c). Plain byte loops; the compiler must
 * not turn them back into calls to themselves.
 */
#define NO_LOOP_CALLS __attribute__((optimize("no-tree-loop-distribute-patterns")))

NO_LOOP_CALLS void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n-- != 0) {
        *d++ = *s++;
    }
    return dst;
}

NO_LOOP_CALLS void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;

    while (n-- != 0) {
        *d++ = (unsigned char)c;
    }
    return dst;
}

NO_LOOP_CALLS int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a;
    const unsigned char *q = b;

    for (; n != 0; n--, p++, q++) {
        if (*p != *q) {
            return (int)*p - (int)*q;
        }
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) {
            return (char *)s;
        }
        if (*s == 0) {
            return NULL;
        }
    }
}
