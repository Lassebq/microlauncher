#include <glib.h>
#include <glib/gstdio.h>
#ifdef __linux
#include <linux/limits.h>
#endif
#if defined(__unix) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#include <uuid/uuid.h>
#endif
#ifdef __WIN32
#include <processthreadsapi.h>
#include <windows.h>
#include <winscard.h>
#endif
#include <openssl/sha.h>
#include <stdbool.h>
#include <string.h>
#include <util/util.h>
#include <zip.h>

String string_new_n(int n) {
	String string = {
		.length = 0,
		.size = n,
		.data = malloc(n),
	};
	string.data[0] = '\0';
	return string;
}

String string_new(const char *str) {
	if(str == NULL) {
		String string = {
			.length = 0,
			.size = BUFSIZ,
			.data = malloc(BUFSIZ)};
		string.data[0] = '\0';
		return string;
	}
	int n = strlen(str) + 1;
	char *data = malloc(n);
	memcpy(data, str, n);
	String string = {
		.length = n - 1,
		.size = n,
		.data = data,
	};
	return string;
}

void string_append_char(String *str, char append) {
	if(str->size < str->length + 2) {
		str->size += BUFSIZ;
		str->data = realloc(str->data, str->size);
	}
	str->data[str->length] = append;
	str->length++;
	str->data[str->length] = '\0';
}

void string_append_n(String *str, const char *append, int n) {
	int oldsize = str->size;
	while(str->size < str->length + n + 1) {
		str->size += BUFSIZ;
	}
	if(oldsize < str->size) {
		str->data = realloc(str->data, str->size);
	}
	memcpy(str->data + str->length, append, n);
	str->length += n;
	str->data[str->length] = '\0';
}

void string_append(String *str, const char *append) {
	string_append_n(str, append, strlen(append));
}

void string_destroy(String *str) {
	free(str->data);
}

FILE *fopen_mkdir(const char *path, const char *mode) {
	gchar *dirname = g_path_get_dirname(path);
	FILE *fd = NULL;
	if(g_mkdir_with_parents(dirname, 0755) == 0) {
		fd = fopen(path, mode);
	}
	g_free(dirname);
	return fd;
}

gboolean rmdir_recursive(const gchar *path, GError **error) {
	GDir *dir = g_dir_open(path, 0, error);
	if(!dir) {
		return FALSE;
	}

	const gchar *entry;
	gboolean ok = TRUE;
	while((entry = g_dir_read_name(dir))) {
		gchar *full_path = g_build_filename(path, entry, NULL);

		if(g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
			if(!rmdir_recursive(full_path, error)) {
				ok = FALSE;
			}
		} else {
			if(g_remove(full_path) != 0) {
				g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
							"Failed to remove file: %s", full_path);
				ok = FALSE;
			}
		}
		g_free(full_path);
		if(!ok) {
			break;
		}
	}
	g_dir_close(dir);

	if(g_rmdir(path) != 0) {
		g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
					"Failed to remove directory: %s", path);
		ok = FALSE;
	}

	return ok;
}

char *read_file_as_string(const char *path) {
	String str = string_new(NULL);
	FILE *file = fopen(path, "rb");
	if(!file) {
		return NULL;
	}
	char buff[BUFSIZ];
	int n;
	while((n = fread(buff, 1, BUFSIZ, file))) {
		string_append_n(&str, buff, n);
	}
	fclose(file);
	return str.data;
}

bool write_string_to_file(const char *str, const char *path) {
	FILE *file = fopen_mkdir(path, "wb");
	if(!file) {
		return false;
	}
	int len = strlen(str);
	int n = 0;
	size_t written;
	while((written = fwrite(str + n, 1, MIN(len - n, BUFSIZ), file))) {
		n += written;
	}
	fclose(file);
	return true;
}

bool strequal(const char *str1, const char *str2) {
	if(str1 == str2) {
		return true;
	}
	if(str1 == NULL || str2 == NULL) {
		return false;
	}
	return strcmp(str1, str2) == 0;
}

void get_sha256(FILE *fd, char *hash) {
	unsigned char digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256_ctx;
	SHA256_Init(&sha256_ctx);

	unsigned char buffer[4096]; // Buffer to read file in chunks
	size_t bytes_read;

	while((bytes_read = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
		SHA256_Update(&sha256_ctx, buffer, bytes_read);
	}

	SHA256_Final(digest, &sha256_ctx);

	fclose(fd);

	for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(&hash[i * 2], "%02x", digest[i]);
	}
	hash[SHA256_DIGEST_LENGTH * 2] = '\0';
}

void get_sha1(FILE *fd, char *hash) {
	unsigned char digest[SHA_DIGEST_LENGTH];
	SHA_CTX sha1_ctx;
	SHA1_Init(&sha1_ctx);

	unsigned char buffer[4096]; // Buffer to read file in chunks
	size_t bytes_read;

	while((bytes_read = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
		SHA1_Update(&sha1_ctx, buffer, bytes_read);
	}

	SHA1_Final(digest, &sha1_ctx);

	fclose(fd);

	for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
		sprintf(&hash[i * 2], "%02x", digest[i]);
	}
	hash[SHA_DIGEST_LENGTH * 2] = '\0';
}

int strsplit(char *src, char delim, char **dest, int max) {
	char *s;
	char *str = src;
	for(int i = 0; i < max; i++) {
		if(*str == 0) {
			return i;
		}
		dest[i] = str;
		s = strchr(str, delim);
		if(s == NULL) {
			return i + 1;
		}
		*s = '\0';
		str = s + 1;
	}
	return 0;
}

void replace_chr(char *src, char replace, char replacement) {
	int n = strlen(src);
	for(int i = 0; i < n; i++) {
		if(src[i] == replace) {
			src[i] = replacement;
		}
	}
}

void replace_str(String *src, const char *replace, const char *replacement) {
	int replaceSize = strlen(replace);
	String str = string_new_n(src->length);
	int k = 0;
	while(src->data[k]) {
		if(strncmp(src->data + k, replace, replaceSize) == 0) {
			string_append(&str, replacement);
			k += replaceSize;
			continue;
		}
		string_append_char(&str, src->data[k]);
		k++;
	}
	src->length = str.length;
	src->size = str.size;
	src->data = malloc(str.size);
	memcpy(src->data, str.data, str.length + 1);
	string_destroy(&str);
}

void bytes_as_hex(unsigned char *bytes, int size, char *dest) {
	for(int i = 0; i < size; i++) {
		sprintf(&dest[i * 2], "%02x", bytes[i]);
	}
}

gconstpointer g_slist_find_custom_value(GSList *list, gconstpointer val, GCompareFunc comp) {
	GSList *element = g_slist_find_custom(list, val, comp);
	if(element) {
		return element->data;
	}
	return NULL;
}

const char *util_basename(const char *path) {
	const char *basename = strrchr(path, '/');
	if(basename) {
		return basename + 1;
	} else {
		return path;
	}
}

char *random_uuid(void) {
	char *str = malloc(37);
	uuid_t uuid;
#ifdef _WIN32
	OLECHAR widestr[39];
	CoCreateGuid(&uuid);
	StringFromGUID2(&uuid, widestr, 39); // "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
	/* Remove surrounding brackets */
	wcstombs(str, widestr + 1, 36);
	str[36] = '\0';
	return str;
#else
	uuid_generate_random(uuid);
	uuid_unparse_lower(uuid, str);
	return str;
#endif
}

static bool str_starts_with_strv(const char **strv, const char *str) {
	while(*strv) {
		if(strncmp(str, *strv, strlen(*strv)) == 0) {
			return true;
		}
		strv++;
	}
	return false;
}

void extract_zip(const char *sourcepath, const char *destpath, const char **exclusions) {
	struct zip *zip;
	zip_stat_t stat;
	zip_file_t *file;
	FILE *out;
	size_t sum;
	zip_int64_t read;
	char pathbuf[PATH_MAX];
	char buf[BUFSIZ];
	zip = zip_open(sourcepath, 0, NULL);
	if(!zip) {
		return;
	}
	size_t n = zip_get_num_entries(zip, 0);

	for(size_t i = 0; i < n; i++) {
		if(zip_stat_index(zip, i, 0, &stat) != 0) {
			continue;
		}
		if(str_starts_with_strv(exclusions, stat.name)) {
			continue;
		}
		file = zip_fopen_index(zip, i, 0);
		if(!file) {
			continue;
		}
		snprintf(pathbuf, PATH_MAX, "%s/%s", destpath, stat.name);
		out = fopen_mkdir(pathbuf, "wb");
		if(!out) {
			zip_fclose(file);
			continue;
		}

		sum = 0;
		while(sum != stat.size) {
			read = zip_fread(file, buf, sizeof(buf));
			if(read < 0) {
				goto cleanup;
			}
			fwrite(buf, 1, read, out);
			sum += read;
		}
	cleanup:
		fclose(out);
		zip_fclose(file);
	}
}

char *get_escaped_command(char *const *cmdline) {
	String str = string_new(NULL);
	bool toquote;
	char *const *arg = cmdline;
	while(*arg) {
		toquote = strchr(*arg, ' ') != NULL;
		if(toquote) {
			string_append_char(&str, '"');
		}
		string_append_n(&str, *arg, strlen(*arg));
		if(toquote) {
			string_append_char(&str, '"');
		}
		arg++;
		if(*arg) {
			string_append_char(&str, ' ');
		}
	}
	return str.data;
}

char **get_commandv(char *cmdline) {
	if(!cmdline || strnlen(cmdline, 1) == 0) {
		return NULL;
	}
	char *cmdv[256];
	char *pointer, *token_start;
	char **heap;
	size_t i = 0;
	bool quoted = false;
	pointer = cmdline;
	token_start = pointer;
	for(; *pointer != '\0'; pointer++) {
		char ch = *pointer;
		if(ch == '"') {
			if(!quoted) {
				token_start++;
			} else {
				*pointer = '\0';
			}
			quoted = !quoted;
			continue;
		}
		if(ch == ' ' && !quoted) {
			*pointer = '\0';
			cmdv[i++] = token_start;
			token_start = pointer + 1;
			continue;
		}
	}
	cmdv[i++] = token_start;
	cmdv[i++] = NULL;
	heap = malloc(sizeof(void *) * i);
	memcpy(heap, cmdv, sizeof(void *) * i);
	return heap;
}

GPid util_fork_execv(const char *dir, char *const *argv) {
#ifndef _WIN32
	GPid pid = fork();
	if(pid == 0) {
		chdir(dir);
		execvp(argv[0], argv);
		_exit(EXIT_FAILURE);
	}
	return pid;
#else
	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	char *cmdline = get_escaped_command(argv);
	if(CreateProcessA(NULL, cmdline, NULL, NULL, false, 0, NULL, dir, &si, &pi)) {
		CloseHandle(pi.hThread);
		return pi.hProcess;
	} else {
		return NULL;
	}
#endif
}

bool util_waitpid(GPid pid, int *exitcode) {
#ifndef _WIN32
	int status;
	waitpid(pid, &status, 0);
	if(WIFEXITED(status)) {
		*exitcode = WEXITSTATUS(status);
		return true;
	} else {
		return false;
	}
#else
	DWORD exitCode = 0;
	HANDLE procHandle = (HANDLE)pid;
	WaitForSingleObject(procHandle, INFINITE);
	if(GetExitCodeProcess(procHandle, &exitCode)) {
		CloseHandle(procHandle);
		*exitcode = exitCode;
		return true;
	} else {
		CloseHandle(procHandle);
		return false;
	}
#endif
}

bool util_kill_process(GPid pid) {
#ifdef __linux
	return kill(pid, SIGKILL) == 0;
#elif __WIN32
	return TerminateProcess((HANDLE)pid, 0);
#endif
}
