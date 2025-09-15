#pragma once

#include <glib.h>

#ifdef _WIN32
#include <windows.h>
#endif

G_BEGIN_DECLS

#include <openssl/sha.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __APPLE__
#define OS_NAME "osx"
#elif __linux
#define OS_NAME "linux"
#elif _WIN32
#define OS_NAME "windows"
#endif

#if defined(__amd64__) || defined(__amd64) || defined(_M_X64)
#define OS_ARCH "amd64"
#elif defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(_M_X86)
#ifdef _WIN32
#define OS_ARCH "x86"
#else
#define OS_ARCH "i386"
#endif
#elif defined(__arm__) || defined(_M_ARM)
#define OS_ARCH "arm"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define OS_ARCH "aarch64"
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
#define OS_ARCH "ppc"
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#define OS_ARCH "ppc64"
#else
#define OS_ARCH "unknown";
#endif

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

gboolean rmdir_recursive(const gchar *path, GError **error);

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

void extract_zip(const char *sourcepath, const char *destpath, const char **exclusions);

char *get_escaped_command(char *const *cmdline);

char **get_commandv(char *cmdline);

GPid util_fork_execv(const char *dir, char *const *argv);

bool util_waitpid(GPid pid, int *status);

bool util_kill_process(GPid pid);

#define offset_apply(pointer, offs) ((void *)((char *)pointer + offs))

#ifdef _WIN32
HRESULT CreateShortcut(LPCSTR pszTargetfile, LPCSTR pszTargetargs, LPCSTR pszLinkfile, int iShowmode, LPCSTR pszCurdir, LPCSTR pszIconfile, int iIconindex);
#endif

G_END_DECLS
