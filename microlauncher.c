#include "json_object.h"
#include "json_tokener.h"
#include "json_types.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdbool.h>
#include <unistd.h>
#include "util.h"
#include "microlauncher_gui.h"
#include "xdgutil.h"

static CURL *curl;

static const char *instance_path = NULL;
static const char *account = NULL;

struct Param {
    const char *name;
    const char **stored_string;
};

static const struct Param params[] = {
    {"instance", &instance_path},
    {"user", &account},
    {NULL, NULL}
};

void microlauncher_init(int argc, char **argv) {
    curl = curl_easy_init();
    if(!curl) {
        exit(EXIT_FAILURE);
    }
    if(!xdgutil_init()) {
		fprintf(stderr, "Could not initialize environment\n");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < argc; i++) {
        if(!argv[i]) {
            break;
        }
        // starts with --
        if(argv[i][0] == '-' && argv[i][1] == '-') {
            const struct Param *p = params;
            while(p->name && strcmp(p->name, argv[i]+2) != 0) {
                p++;
            }
            if(p->name) {
               *p->stored_string = argv[i+1]; 
            }
        }
    }
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    FILE *file = (FILE *)userdata;
    return fwrite(ptr, size, nmemb, file) * size;
}

bool microlauncher_http_get(const char *url, const char *save_location) {
    CURLcode code;
    if (!curl) {
        return false;
    }
    FILE *file = fopen(save_location, "w");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(file);
    return code == CURLE_OK;
}

size_t write_callback_string(void *ptr, size_t size, size_t nmemb, void *userdata) {
    String *string = (String *)userdata;
    string_append_n(string, ptr, size * nmemb);
    return size * nmemb;
}

json_object *microlauncher_http_get_json(const char *url) {
    CURLcode code;
    if (!curl) {
        return false;
    }
    String str = string_new(NULL);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
    code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    json_object *obj = NULL;
    if(code == CURLE_OK) {
        obj = json_tokener_parse(str.data);
    }
    return obj;
}

void microlauncher_json_save(json_object *obj, const char *location) {
    FILE *file = fopen(location, "w");
    size_t written = 0;
    size_t n = BUFSIZ;
    const char *str = json_object_to_json_string(obj);
    while(str[written]) {
        n = strnlen(str+written, BUFSIZ);
        written += fwrite(str+written, 1, n, file);
    }
    fclose(file);
}

struct User {
    char *username;
    char *uuid;
    char *token; 
};

struct User microlauncher_get_user(const char *user) {
    struct User usr = {0};
    usr.username = strdup(user);
    return usr;
}

int microlauncher_launch_instance(const char *instance, const char *user) {
    return 0;
}

int main(int argc, char **argv) {
    microlauncher_init(argc, argv);

    if(microlauncher_launch_instance(instance_path, account)) {
        return EXIT_SUCCESS;
    }

    return microlauncher_gui_show();
}
