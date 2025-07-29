#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

String string_new(const char *str) {
	if(str == NULL) {
		String string = {
			.length = 0,
			.size = BUFSIZ,
			.data = malloc(BUFSIZ)
        };
		string.data[0] = '\0';
		return string;
	}
	int n = strlen(str);
	char *data = malloc(n + 1);
	String string = {
		.length = n,
		.size = n + 1,
		.data = data,
	};
	return string;
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
