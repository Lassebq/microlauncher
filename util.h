#pragma once
#include "glib.h"
#include "json_types.h"
#include <stdbool.h>
#include <stdio.h>
#include <openssl/sha.h>

typedef char Sha1[SHA_DIGEST_LENGTH * 2 + 1];
typedef char Sha256[SHA256_DIGEST_LENGTH * 2 + 1];

typedef struct {
    /* pointer to string with null terminator */
    char *data;
    /* length without null terminator */
    int length;
    /* allocated size */
    int size;
} String;

String string_new_n(int n);

String string_new(const char *str);

void string_append_char(String *str, char append);

void string_append(String *str, const char *append);

void string_append_n(String *str, const char *append, int n);

void string_destroy(String *str);

FILE *fopen_mkdir(const char *path, const char *mode);

char *read_file_as_string(const char *path);

bool write_string_to_file(const char *str, const char *path);

bool strequal(const char *str1, const char *str2);

void get_sha256(FILE *fd, char *hash);

void get_sha1(FILE *fd, char *hash);

void replace_chr(char *src, char replace, char replacement);

void replace_str(String *src, const char *replace, const char *replacement);

int strsplit(char *src, char delim, char **dest, int max);

void bytes_as_hex(unsigned char *bytes, int size, char *dest);

gconstpointer g_slist_find_custom_value(GSList *list, gconstpointer val, GCompareFunc comp);

const char *util_basename(const char *path);

char *random_uuid(void);