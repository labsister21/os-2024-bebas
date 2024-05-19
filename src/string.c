#include <stdint.h>
#include <stddef.h>
#include "header/stdlib/string.h"

void* memset(void *s, int c, size_t n) {
    uint8_t *buf = (uint8_t*) s;
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t) c;
    return s;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    uint8_t *dstbuf       = (uint8_t*) dest;
    const uint8_t *srcbuf = (const uint8_t*) src;
    for (size_t i = 0; i < n; i++)
        dstbuf[i] = srcbuf[i];
    return dstbuf;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *buf1 = (const uint8_t*) s1;
    const uint8_t *buf2 = (const uint8_t*) s2;
    for (size_t i = 0; i < n; i++) {
        if (buf1[i] < buf2[i])
            return -1;
        else if (buf1[i] > buf2[i])
            return 1;
    }

    return 0;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *dstbuf       = (uint8_t*) dest;
    const uint8_t *srcbuf = (const uint8_t*) src;
    if (dstbuf < srcbuf) {
        for (size_t i = 0; i < n; i++)
            dstbuf[i]   = srcbuf[i];
    } else {
        for (size_t i = n; i != 0; i--)
            dstbuf[i-1] = srcbuf[i-1];
    }

    return dest;
}

char* strtok(char* str, const char* delimiters)
{
    static char* next_token = NULL;

    if (str != NULL) {
        next_token = str;
    }

    if (next_token == NULL) {
        return NULL;
    }

    char* token_start = next_token;
    while (*next_token != '\0') {
        if (strchr(delimiters, *next_token) != NULL) {
            *next_token = '\0';
            next_token++;
            break;
        }
        next_token++;
    }

    return token_start;
}

char* strchr(const char* str, int c) {
    while (*str != '\0') {
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    if (c == '\0') {
        return (char*)str;
    }
    return NULL;
}

size_t strspn(const char *str1, const char *str2) {
    const char *p;
    const char *a;
    size_t count = 0;

    // iterate over the entire string
    for (p = str1; *p != '\0'; ++p) {
        // for each character, iterate over the delimiters
        for (a = str2; *a != '\0'; ++a) {
            if (*p == *a)
                break;
        }
        // if we've reached the end of the delimiters without finding a match, return the count
        if (*a == '\0')
            return count;
        // otherwise, increment the count
        ++count;
    }

    return count;
}

int strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char *)str1 - *(unsigned char *)str2;
}

char *strcpy(char *dest, const char *src) {
    char *save = dest;
    while((*dest++ = *src++))
        ;
    return save;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    // Copy characters from src to dest
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // Pad with null characters if src is shorter than n
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}