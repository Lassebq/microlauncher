#ifndef UTIL_H
#define UTIL_H

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
    /* pointer to string with null terminator */
    char *data;
    /* length without null terminator */
    int length;
    /* allocated size */
    int size;
} String;

String string_new(const char *str);

void string_append(String *str, const char *append);

void string_append_n(String *str, const char *append, int n);

void string_destroy(String *str);

#endif