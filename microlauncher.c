#include "microlauncher.h"
#include "gio/gio.h"
#include "glib.h"
#include "json.h"
#include "json_object.h"
#include "json_types.h"
#include "json_util.h"
#include "microlauncher_gui.h"
#include "microlauncher_msa.h"
#include "util.h"
#include "xdgutil.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zip.h>

static GSList *instances;
static GSList *accounts;

static CURL *globalCurlHandle;
static GHashTable *manifest;
static struct Settings settings = {0};

// Used by main
static struct Instance *active_instance = NULL;
static struct User *active_user = NULL;

#define run_callback(cb, ...)                          \
	if(callbacks.cb) {                                 \
		callbacks.cb(__VA_ARGS__, callbacks.userdata); \
	}

struct Param {
	const char *name;
	char *val;
};

struct Param params[] = {
	{.name = "instance"},
	{.name = "user"},
	{NULL, NULL}
};

struct Callbacks callbacks;

struct Instance *microlauncher_instance_get(GSList *list, const char *id) {
	struct Instance *inst;
	GSList *node = list;
	while(node) {
		inst = node->data;
		if(strequal(inst->name, id)) {
			return inst;
		}
		node = node->next;
	}
	return NULL;
}

struct User *microlauncher_account_get(GSList *list, const char *id) {
	struct User *usr;
	GSList *node = list;
	while(node) {
		usr = node->data;
		if(strequal(usr->id, id)) {
			return usr;
		}
		node = node->next;
	}
	return NULL;
}

void microlauncher_load_account(json_object *obj, struct User *user) {
	user->name = g_strdup(json_get_string(obj, "name"));
	user->uuid = g_strdup(json_get_string(obj, "uuid"));
	user->type = json_get_int(obj, "type");
	user->id = g_strdup(json_get_string(obj, "id"));
	json_object *data = json_object_object_get(obj, "data");
	switch(user->type) {
		case ACCOUNT_TYPE_MSA:
			user->data = malloc(sizeof(struct MicrosoftUser));
			struct MicrosoftUser *userdata = user->data;
			userdata->access_token = g_strdup(json_get_string(data, "access_token"));
			userdata->refresh_token = g_strdup(json_get_string(data, "refresh_token"));
			break;
		case ACCOUNT_TYPE_OFFLINE:
			user->data = NULL;
			break;
	}
}

void microlauncher_save_account(json_object *obj, struct User *user) {
	json_set_string(obj, "name", user->name);
	json_set_string(obj, "uuid", user->uuid);
	json_set_int(obj, "type", user->type);
	json_set_string(obj, "id", user->id);
	json_object *data = json_object_new_object();
	struct MicrosoftUser *userdata;
	switch(user->type) {
		case ACCOUNT_TYPE_OFFLINE:
			break;
		case ACCOUNT_TYPE_MSA:
			userdata = (struct MicrosoftUser *)user->data;
			json_set_string(data, "access_token", userdata->access_token);
			json_set_string(data, "refresh_token", userdata->refresh_token);
			break;
	}
	json_object_object_add(obj, "data", data);
}

void microlauncher_load_instance(json_object *obj, struct Instance *instance) {
	instance->name = g_strdup(json_get_string(obj, "name"));
	instance->javaLocation = g_strdup(json_get_string(obj, "javaLocation"));
	instance->location = g_strdup(json_get_string(obj, "location"));
	instance->version = g_strdup(json_get_string(obj, "version"));
	instance->icon = g_strdup(json_get_string(obj, "icon"));
}

void microlauncher_save_instance(json_object *obj, struct Instance *instance) {
	json_set_string(obj, "name", instance->name);
	json_set_string(obj, "javaLocation", instance->javaLocation);
	json_set_string(obj, "location", instance->location);
	json_set_string(obj, "version", instance->version);
	json_set_string(obj, "icon", instance->icon);
}

bool microlauncher_init_config(void) {
	char pathbuf[PATH_MAX];
	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/instances.json", XDG_DATA_HOME);
	json_object *arr, *iter;
	instances = NULL;
	accounts = NULL;

	arr = json_from_file(pathbuf);

	if(arr && json_object_is_type(arr, json_type_array)) {
		size_t length = json_object_array_length(arr);

		for(size_t i = 0; i < length; i++) {
			iter = json_object_array_get_idx(arr, i);
			struct Instance *instance = g_new(struct Instance, 1);
			microlauncher_load_instance(iter, instance);
			instances = g_slist_append(instances, instance);
		}
	}
	json_object_put(arr);

	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/accounts.json", XDG_DATA_HOME);
	arr = json_from_file(pathbuf);
	if(arr && json_object_is_type(arr, json_type_array)) {
		size_t length = json_object_array_length(arr);

		for(size_t i = 0; i < length; i++) {
			iter = json_object_array_get_idx(arr, i);
			struct User *user = g_new(struct User, 1);
			microlauncher_load_account(iter, user);
			accounts = g_slist_append(accounts, user);
		}
	}
	json_object_put(arr);

	microlauncher_load_settings();

	return true;
}

struct Version *microlauncher_version_new(const char *version, const char *type, const char *releaseTime, const char *sha1, const char *url) {
	struct Version *ver = g_new(struct Version, 1);
	ver->id = g_strdup(version);
	ver->type = g_strdup(type);
	ver->releaseTime = g_strdup(releaseTime);
	ver->sha1 = g_strdup(sha1);
	ver->url = g_strdup(url);
	return ver;
}

void microlauncher_version_destroy(void *p) {
	struct Version *ver = (struct Version *)p;
	free(ver->id);
	free(ver->type);
	free(ver->releaseTime);
	free(ver->sha1);
	free(ver->url);
	free(ver);
}

bool microlauncher_init(int argc, char **argv) {
	char path[PATH_MAX];
	if(!xdgutil_init()) {
		fprintf(stderr, "Failed to initialize environment\n");
		return false;
	}
	globalCurlHandle = curl_easy_init();
	if(!globalCurlHandle) {
		fprintf(stderr, "Can't initialize curl\n"); // Non fatal
	}

	for(int i = 0; i < argc; i++) {
		if(!argv[i]) {
			break;
		}
		// starts with --
		if(argv[i][0] == '-' && argv[i][1] == '-') {
			struct Param *p = params;
			while(p->name && strcmp(p->name, argv[i] + 2) != 0) {
				p++;
			}
			if(p->name) {
				p->val = argv[i + 1];
			}
		}
	}
	if(!microlauncher_init_config()) {
		fprintf(stdout, "No config, using fresh config\n");
	}
	const char *manifest_url;
#ifdef USE_MOJANG_MANIFEST
	manifest_url = MOJANG_MANIFEST;
#else
	manifest_url = BETTERJSONS_MANIFEST;
#endif
	json_object *manifestJson = microlauncher_http_get_json(manifest_url, NULL, NULL);
	manifest = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, microlauncher_version_destroy);

	snprintf(path, PATH_MAX, "%s/versions", settings.launcher_root);
	GDir *dir = g_dir_open(path, 0, NULL);
	const char *filename;
	while(dir && (filename = g_dir_read_name(dir))) {
		snprintf(path, PATH_MAX, "%s/versions/%s/%s.json", settings.launcher_root, filename, filename);
		if(access(path, F_OK) == 0) {
			json_object *obj = json_from_file(path);
			if(obj) {
				struct Version *ver = microlauncher_version_new(json_get_string(obj, "id"), json_get_string(obj, "type"), json_get_string(obj, "releaseTime"), NULL, NULL);
				g_hash_table_replace(manifest, ver->id, ver);
			}
		}
	}

	json_object *versions = json_object_object_get(manifestJson, "versions");
	if(json_object_is_type(versions, json_type_array)) {
		size_t n = json_object_array_length(versions);
		for(size_t i = 0; i < n; i++) {
			json_object *iter = json_object_array_get_idx(versions, i);
			struct Version *ver = microlauncher_version_new(
				json_get_string(iter, "id"),
				json_get_string(iter, "type"),
				json_get_string(iter, "releaseTime"),
				json_get_string(iter, "sha1"),
				json_get_string(iter, "url"));
			g_hash_table_replace(manifest, ver->id, ver);
		}
	}
	return true;
}

GHashTable *microlauncher_get_manifest(void) {
	return manifest;
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
	FILE *file = (FILE *)userdata;
	return fwrite(ptr, size, nmemb, file);
}

void microlauncher_set_curl_opts(CURL *curl) {
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL);
}

bool microlauncher_http_get_with_handle(CURL *curl, const char *url, const char *save_location) {
	CURLcode code;
	if(!curl) {
		return false;
	}
	FILE *file = fopen_mkdir(save_location, "wb");
	if(!file) {
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	microlauncher_set_curl_opts(curl);
	code = curl_easy_perform(curl);
	fclose(file);
	return code == CURLE_OK;
}

bool microlauncher_http_get(const char *url, const char *save_location) {
	CURL *curl = curl_easy_init();
	bool b = microlauncher_http_get_with_handle(curl, url, save_location);
	curl_easy_cleanup(curl);
	return b;
}

size_t write_callback_string(void *ptr, size_t size, size_t nmemb, void *userdata) {
	String *string = (String *)userdata;
	string_append_n(string, ptr, size * nmemb);
	return size * nmemb;
}

json_object *microlauncher_http_get_json(const char *url, struct curl_slist *headers, const char *post) {
	CURLcode code;
	// Sharing curl handle is not safe in multithreaded scenario
	CURL *curl = curl_easy_init();
	if(!curl) {
		return NULL;
	}
	char buff[CURL_ERROR_SIZE];
	String str = string_new(NULL);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_string);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
	microlauncher_set_curl_opts(curl);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buff);
	if(headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	if(post) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	}
	code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	json_object *obj = NULL;
	if(code == CURLE_OK) {
		obj = json_tokener_parse(str.data);
		string_destroy(&str);
	} else {
		g_print("CURL error (%d) on %s\n", code, url);
		g_print("%s\n", buff);
	}
	return obj;
}

json_object *microlauncher_http_get_json_and_save(const char *url, const char *save_location) {
	json_object *obj = microlauncher_http_get_json(url, NULL, NULL);
	json_to_file(obj, save_location, JSON_C_TO_STRING_NOSLASHESCAPE);
	return obj;
}

char *microlauncher_get_library_path(const char *name, const char *classifier, char *path) {
	int n = strlen(name) + 1;
	char libname[n];
	memcpy(libname, name, n);
	char *lib[4] = {0};
	strsplit(libname, ':', lib, 4);
	replace_chr(lib[0], '.', '/');
	if(lib[0] && lib[1] && lib[2]) {
		if(lib[3]) {
			snprintf(path, PATH_MAX, "%s/%s/%s/%s-%s-%s.jar", lib[0], lib[1], lib[2], lib[1], lib[2], lib[3] ? lib[3] : classifier);
		} else {
			snprintf(path, PATH_MAX, "%s/%s/%s/%s-%s.jar", lib[0], lib[1], lib[2], lib[1], lib[2]);
		}
		return path;
	}
	return NULL;
}

void microlauncher_fetch_artifact(const char *url, const char *path, const char *label, const char *sha1, int size, int total_size, int *download_size) {
	if(!path) {
		return;
	}
	if(download_size) {
		*download_size += size;
	}
	if(!label) {
		label = util_basename(path); /* basepath */
	}
	if(total_size > 0) {
		run_callback(progress_update, (double)(*download_size) / total_size, label);
	}
	struct stat st;
	if(stat(path, &st) != 0) {
		goto redownload;
	}
	if(size != 0 && st.st_size != size) {
		goto redownload;
	}
	Sha1 hash;
	FILE *fd = fopen(path, "rb");
	if(!fd) {
		goto redownload;
	}
	get_sha1(fd, hash);
	if(sha1 && strcmp(hash, sha1) != 0) {
		goto redownload;
	}
	return; /* everything is fine and we can keep local lib */

redownload:
	if(url) {
		/* Sharing curl handle has better performance, but doesn't work in multithreaded scenario */
		if(!microlauncher_http_get_with_handle(globalCurlHandle, url, path)) {
			g_print("CURL error on %s\n", url);
		}
	}
}

void microlauncher_fetch_library(json_object *libObj, const char *libraries_path, const char *natives_path, int total_size, int *download_size) {
	json_object *downloads = json_object_object_get(libObj, "downloads");
	json_object *artifact = json_object_object_get(downloads, "artifact");
	json_object *classifiers = json_object_object_get(downloads, "classifiers");
	json_object *natives = json_object_object_get(libObj, "natives");
	const char *classifier = json_get_string(natives, OS_NAME);
	json_object *obj, *iter;

	const char *name = json_get_string(libObj, "name");
	const char *url_base = json_get_string(libObj, "url");
	const char *path = json_get_string(artifact, "path");
	const char *url = json_get_string(artifact, "url");
	char realpath[PATH_MAX];
	char pathbuff[PATH_MAX];
	if(!path) {
		path = microlauncher_get_library_path(name, NULL, pathbuff);
	}
	snprintf(realpath, PATH_MAX, "%s/%s", libraries_path, path);
	char url2[PATH_MAX];
	if(!url && url_base) {
		snprintf(url2, PATH_MAX, "%s/%s", url_base, path);
		url = url2;
	}

	microlauncher_fetch_artifact(
		url,
		realpath,
		NULL,
		json_get_string(artifact, "sha1"),
		json_get_int(artifact, "size"),
		total_size, download_size);

	if(classifier) {
		obj = json_object_object_get(classifiers, classifier);
		if(!obj) {
			return;
		}
		path = json_get_string(obj, "path");
		url = json_get_string(obj, "url");
		if(!path) {
			path = microlauncher_get_library_path(name, classifier, pathbuff);
		}
		snprintf(realpath, PATH_MAX, "%s/%s", libraries_path, path);
		if(!url && url_base) {
			snprintf(url2, PATH_MAX, "%s/%s", url_base, path);
			url = url2;
		}
		microlauncher_fetch_artifact(
			url,
			realpath,
			NULL,
			json_get_string(artifact, "sha1"),
			json_get_int(artifact, "size"),
			total_size, download_size);

		obj = json_object_object_get(libObj, "extract");
		obj = json_object_object_get(obj, "exclude");
		if(json_object_is_type(obj, json_type_array)) {
			size_t n = json_object_array_length(obj);
			const char *exclusions[n + 1];
			exclusions[n] = NULL;
			for(size_t i = 0; i < n; i++) {
				iter = json_object_array_get_idx(obj, i);
				exclusions[i] = json_object_get_string(iter);
			}
			extract_zip(realpath, natives_path, exclusions);
		}
	}
}

json_object *inherit_json(const char *versions_path, const char *id) {
	char path[PATH_MAX];
	if(!id) {
		return NULL;
	}
	snprintf(path, PATH_MAX, "%s/%s/%s.json", versions_path, id, id);
	struct Version *version = g_hash_table_lookup(manifest, id);
	// TODO let the user decide whenever to keep changes to local JSON or update with manifest one.
	if(version) {
		microlauncher_fetch_artifact(version->url, path, NULL, version->sha1, 0, 0, NULL);
	}
	json_object *thisObj = json_from_file(path);
	if(!thisObj) {
		return NULL;
	}
	json_object *obj = inherit_json(versions_path, json_get_string(thisObj, "inheritsFrom"));
	if(json_object_is_type(obj, json_type_object)) {
		json_object_object_foreach(thisObj, key, val) {
			json_object *thisMember = json_object_object_get(thisObj, key);
			json_object *otherMember = json_object_object_get(obj, key);
			if(json_object_get_type(thisMember) == json_object_get_type(otherMember)) {
				if(json_object_is_type(otherMember, json_type_array)) {
					for(size_t i = 0; i < json_object_array_length(thisMember); i++) {
						json_object_array_insert_idx(otherMember, i, json_object_array_get_idx(thisMember, i));
					}
				} else {
					json_object_object_add(obj, key, thisMember);
				}
			}
		}
		return obj;
	}
	return thisObj;
}

enum RuleAction {
	RULE_ACTION_ALLOW,
	RULE_ACTION_DISALLOW,
	RULE_ACTION_N
};

static char *ruleNames[RULE_ACTION_N] = {
	"allow", "disallow"};

static enum RuleAction get_action(const char *str) {
	for(int i = 0; i < RULE_ACTION_N; i++) {
		if(strequal(ruleNames[i], str)) {
			return i;
		}
	}
	return RULE_ACTION_ALLOW;
}

static bool check_rules(json_object *rules, GSList *featuresList) {
	json_object *iter, *os, *features;
	enum RuleAction action, appliedAction;
	if(!json_object_is_type(rules, json_type_array)) {
		return true;
	}
	appliedAction = RULE_ACTION_DISALLOW;
	size_t n = json_object_array_length(rules);
	for(size_t i = 0; i < n; i++) {
		iter = json_object_array_get_idx(rules, i);
		action = get_action(json_get_string(iter, "action"));
		os = json_object_object_get(iter, "os");
		if(json_object_is_type(os, json_type_object)) {
			if(!strequal(json_get_string(os, "name"), OS_NAME)) {
				continue;
			}
			// int matchlength;
			// if(re_match(json_get_string(os, "version"), /* FIXME */ osVersion, &matchlength) == -1) {
			// 	continue;
			// }
		}
		features = json_object_object_get(iter, "features");
		if(json_object_is_type(features, json_type_object)) {
			if(featuresList == NULL)
				continue;
			bool hasFeatures = true;
			json_object_object_foreach(features, key, val) {
				if(!json_object_get_boolean(val)) {
					hasFeatures = false;
					break;
				}
				if(!g_slist_find_custom(featuresList, key, (GCompareFunc)strcmp)) {
					hasFeatures = false;
					break;
				}
			}
			if(!hasFeatures) {
				continue;
			}
		}
		appliedAction = action;
	}
	return appliedAction == RULE_ACTION_ALLOW;
}

json_object *microlauncher_fetch_version(const char *versionId, const char *versions_path, const char *libraries_path, const char *natives_path, const char *assets_dir) {
	json_object *json, *libraries, *downloads, *artifact, *client, *iter, *assets_json, *obj;
	char path[PATH_MAX];
	char url[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/%s/%s.json", versions_path, versionId, versionId);
	json = inherit_json(versions_path, versionId);
	if(!json) {
		return NULL;
	}
	const char *id = json_get_string(json, "id");
	libraries = json_object_object_get(json, "libraries");
	downloads = json_object_object_get(json, "downloads");
	client = json_object_object_get(downloads, "client");
	snprintf(path, PATH_MAX, "%s/%s/%s.jar", versions_path, id, id);
	// Count total size
	int total_size = json_get_int(client, "size");
	int current_size = 0;
	if(json_object_is_type(libraries, json_type_array)) {
		size_t length = json_object_array_length(libraries);

		for(size_t i = 0; i < length; i++) {
			iter = json_object_array_get_idx(libraries, i);
			if(!check_rules(json_object_object_get(iter, "rules"), NULL)) {
				continue;
			}
			downloads = json_object_object_get(iter, "downloads");
			artifact = json_object_object_get(downloads, "artifact");
			if(artifact) {
				total_size += json_get_int(artifact, "size");
			}

			const char *classifier = json_get_string(json_object_object_get(iter, "natives"), OS_NAME);
			if(classifier) {
				obj = json_object_object_get(downloads, "classifiers");
				obj = json_object_object_get(obj, classifier);
				if(obj) {
					total_size += json_get_int(obj, "size");
				}
			}
		}
	}
	// Perform download
	run_callback(stage_update, "Downloading libraries");
	snprintf(path, PATH_MAX, "%s/%s/%s.jar", versions_path, id, id);

	microlauncher_fetch_artifact(
		json_get_string(client, "url"),
		path,
		NULL,
		json_get_string(client, "sha1"),
		json_get_int(client, "size"),
		total_size, &current_size);

	if(json_object_is_type(libraries, json_type_array)) {
		size_t length = json_object_array_length(libraries);

		for(size_t i = 0; i < length; i++) {
			iter = json_object_array_get_idx(libraries, i);
			if(check_rules(json_object_object_get(iter, "rules"), NULL)) {
				microlauncher_fetch_library(iter, libraries_path, natives_path, total_size, &current_size);
			}
		}
	}
	run_callback(stage_update, "Downloading assets");
	obj = json_object_object_get(json, "assetIndex");
	snprintf(path, PATH_MAX, "%s/indexes/%s.json", assets_dir, json_get_string(obj, "id"));
	total_size = json_get_int(obj, "totalSize") + json_get_int(obj, "size"); // assets.json + all assets size
	current_size = 0;
	microlauncher_fetch_artifact(json_get_string(obj, "url"), path, NULL, json_get_string(obj, "sha1"), json_get_int(obj, "size"), total_size, &current_size);
	assets_json = json_from_file(path);
	obj = json_object_object_get(assets_json, "objects");
	if(json_object_is_type(obj, json_type_object)) {
		json_object_object_foreach(obj, key, val) {
			const char *hash = json_get_string(val, "hash");
			snprintf(path, PATH_MAX, "%s/objects/%c%c/%s", assets_dir, *hash, *(hash + 1), hash);
			snprintf(url, PATH_MAX, "https://resources.download.minecraft.net/%c%c/%s", *hash, *(hash + 1), hash);
			microlauncher_fetch_artifact(url, path, key, hash, json_get_int(val, "size"), total_size, &current_size);
		}
	}

	// Finished
	run_callback(stage_update, NULL);
	return json;
}

char *microlauncher_get_javacp(json_object *json, const char *versions_path, const char *libraries_path) {
	json_object *libraries, *iter, *downloads, *artifact;
	char path[PATH_MAX];
	const char *id = json_get_string(json, "id");
	if(!id) {
		return NULL;
	}
	snprintf(path, PATH_MAX, "%s/%s/%s.jar", versions_path, id, id);
	String str = string_new(path);
	libraries = json_object_object_get(json, "libraries");

	if(json_object_is_type(libraries, json_type_array)) {
		size_t length = json_object_array_length(libraries);

		for(size_t i = 0; i < length; i++) {
			iter = json_object_array_get_idx(libraries, i);
			if(!check_rules(json_object_object_get(iter, "rules"), NULL)) {
				continue;
			}
			downloads = json_object_object_get(iter, "downloads");
			artifact = json_object_object_get(downloads, "artifact");
			const char *libpath = json_get_string(artifact, "path");
			const char *name = json_get_string(iter, "name");
			if(!libpath) {
				libpath = microlauncher_get_library_path(name, NULL, path);
			}
#ifdef _WIN32
			string_append_char(&str, ';');
#else
			string_append_char(&str, ':');
#endif
			string_append(&str, libraries_path);
			string_append_char(&str, '/');
			string_append(&str, libpath);
		}
	}
	return str.data;
}

static const char *ACCOUNT_TYPES[] = {
	"Offline",
	"Microsoft"};

const char *microlauncher_get_account_type_name(enum AccountType accountType) {
	return ACCOUNT_TYPES[accountType];
}

struct Settings *microlauncher_get_settings(void) {
	return &settings;
}

void microlauncher_load_settings(void) {
	char pathbuf[PATH_MAX];
	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/settings.json", XDG_DATA_HOME);
	json_object *obj = json_from_file(pathbuf);
	settings.instance = microlauncher_instance_get(instances, json_get_string(obj, "instance"));
	settings.user = microlauncher_account_get(accounts, json_get_string(obj, "user"));
	settings.fullscreen = json_get_bool(obj, "fullscreen");
	settings.demo = json_get_bool(obj, "demo");
	settings.width = json_get_int(obj, "width");
	settings.height = json_get_int(obj, "height");
	const char *root = getenv("MICROLAUNCHER_LAUNCHER_ROOT");
	if(root) {
		settings.launcher_root = g_strdup(root);
	} else if((root = json_get_string(obj, "launcherRoot"))) {
		settings.launcher_root = g_strdup(root);
	} else {
#ifdef _WIN32
		/* %appdata%/.minecraft */
		settings.launcher_root = g_strdup_printf("%s/.minecraft", XDG_DATA_HOME);
#else
		/* ~/.local/share/minecraft */
		settings.launcher_root = g_strdup_printf("%s/minecraft", XDG_DATA_HOME);
#endif
	}
	json_object_put(obj);
}

void microlauncher_save_settings(void) {
	char pathbuf[PATH_MAX];
	json_object *objIter;

	json_object *obj = json_object_new_object();
	if(settings.instance) {
		json_set_string(obj, "instance", settings.instance->name);
	}
	if(settings.user) {
		json_set_string(obj, "user", settings.user->id);
	}
	json_set_int(obj, "width", settings.width);
	json_set_int(obj, "height", settings.height);
	json_set_bool(obj, "fullscreen", settings.fullscreen);
	json_set_bool(obj, "demo", settings.demo);
	if(settings.launcher_root) {
		json_set_string(obj, "launcherRoot", settings.launcher_root);
	}

	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/settings.json", XDG_DATA_HOME);
	json_to_file(obj, pathbuf, JSON_C_TO_STRING_NOSLASHESCAPE | JSON_C_TO_STRING_PRETTY);
	json_object_put(obj);

	obj = json_object_new_array();
	GSList *iter = accounts;
	while(iter) {
		objIter = json_object_new_object();
		json_object_array_add(obj, objIter);
		struct User *user = iter->data;
		microlauncher_save_account(objIter, user);
		iter = iter->next;
	}
	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/accounts.json", XDG_DATA_HOME);
	json_to_file(obj, pathbuf, JSON_C_TO_STRING_NOSLASHESCAPE | JSON_C_TO_STRING_PRETTY);
	json_object_put(obj);

	obj = json_object_new_array();
	iter = instances;
	while(iter) {
		objIter = json_object_new_object();
		json_object_array_add(obj, objIter);
		struct Instance *instance = iter->data;
		microlauncher_save_instance(objIter, instance);
		iter = iter->next;
	}
	snprintf(pathbuf, PATH_MAX, "%s/microlauncher/instances.json", XDG_DATA_HOME);
	json_to_file(obj, pathbuf, JSON_C_TO_STRING_NOSLASHESCAPE | JSON_C_TO_STRING_PRETTY);
	json_object_put(obj);
}

bool microlauncher_auth_user(struct User *user) {
	bool b = true;
	run_callback(stage_update, "Authentication");
	int steps, i;
	i = 1;
	switch(user->type) {
		case ACCOUNT_TYPE_OFFLINE:
			user->accessToken = "-";
			user->uuid = random_uuid();
			break;
		case ACCOUNT_TYPE_MSA:
			steps = 5;
			struct MicrosoftUser *msuser = user->data;
			run_callback(progress_update, (double)i++ / steps, "Checking refresh token");
			if(microlauncher_msa_refresh_token(msuser->refresh_token, msuser) != 0) {
				b = false;
				break;
			}
			run_callback(progress_update, (double)i++ / steps, "Authenticating via Xbox");
			if(!microlauncher_msa_xboxlive_auth(msuser)) {
				b = false;
				break;
			}
			run_callback(progress_update, (double)i++ / steps, "Obtaining XSTS token");
			if(!microlauncher_msa_xboxlive_xsts(msuser)) {
				b = false;
				break;
			}
			run_callback(progress_update, (double)i++ / steps, "Getting access token");
			char *accessToken;
			if(!(accessToken = microlauncher_msa_login(msuser))) {
				b = false;
				break;
			}
			run_callback(progress_update, (double)i++ / steps, "Getting profile");
			struct MinecraftProfile profile = microlauncher_msa_get_profile(accessToken);
			if(!profile.username) {
				b = false;
				break;
			}
			user->name = profile.username;
			user->uuid = profile.uuid;
			user->accessToken = accessToken;
			break;
	}
	run_callback(stage_update, NULL);
	return b;
}

static char *apply_replaces(const char *const *replaces, const char *str) {
	int i = 0;
	String strBuff = string_new(str);
	while(replaces[i]) {
		replace_str(&strBuff, replaces[i], replaces[i + 1]);
		i += 2;
	}
	return strBuff.data;
}

static int add_arguments(json_object *array, const char *const *replaces, GSList *features, char **argv, char **str_to_free) {
	json_object *iter;

	size_t len = json_object_array_length(array);
	int j = 0;
	for(size_t i = 0; i < len; i++) {
		iter = json_object_array_get_idx(array, i);
		switch(json_object_get_type(iter)) {
			case json_type_object:
				if(check_rules(json_object_object_get(iter, "rules"), features)) {
					argv[j] = str_to_free[j] = apply_replaces(replaces, json_get_string(iter, "value"));
					j++;
				}
				break;
			case json_type_string:
				if(strstr(json_object_get_string(iter), "xuid")) {
					break;
				}
				if(strstr(json_object_get_string(iter), "clientId")) {
					break;
				}
				if(strstr(json_object_get_string(iter), "clientid")) {
					break;
				}

				argv[j] = str_to_free[j] = apply_replaces(replaces, json_object_get_string(iter));
				j++;
				break;
			default:
				break;
		}
	}
	return j;
}

bool microlauncher_launch_instance(const struct Instance *instance, struct User *user) {
	if(!instance || !user) {
		return false;
	}
	bool ret = false;
	int m = 0;
	char *malloc_strs[1024];
	char path[PATH_MAX];
	char versions_dir[PATH_MAX];
	char libraries_dir[PATH_MAX];
	char assets_dir[PATH_MAX];
	char natives_dir[PATH_MAX];
	const char *main_class;
	snprintf(versions_dir, PATH_MAX, "%s/versions", settings.launcher_root);
	snprintf(libraries_dir, PATH_MAX, "%s/libraries", settings.launcher_root);
	snprintf(assets_dir, PATH_MAX, "%s/assets", settings.launcher_root);
	snprintf(natives_dir, PATH_MAX, "%s/natives", settings.launcher_root);
	json_object *json = microlauncher_fetch_version(instance->version, versions_dir, libraries_dir, natives_dir, assets_dir);
	if(!json) {
		return false;
	}
	if(!microlauncher_auth_user(user)) {
		return false;
	}
	const char *id = json_get_string(json, "id");
	const char *minecraftArguments = json_get_string(json, "minecraftArguments");
	char *cp = microlauncher_get_javacp(json, versions_dir, libraries_dir);
	json_object *arguments = json_object_object_get(json, "arguments");
	json_object *argumentsGame = json_object_object_get(arguments, "game");
	json_object *argumentsJvm = json_object_object_get(arguments, "jvm");
	int n = 1;
	n += user->accessToken ? strlen(user->accessToken) : 0;
	n += user->uuid ? strlen(user->uuid) : 0;
	n += strlen("token::");
	char auth_session[n];
	snprintf(auth_session, sizeof(auth_session), "token:%s:%s", user->accessToken ? user->accessToken : "", user->uuid ? user->uuid : "");
	char width_str[11];
	snprintf(width_str, sizeof(width_str), "%d", settings.width);
	char height_str[11];
	snprintf(height_str, sizeof(height_str), "%d", settings.height);

	const char *replaces[] = {
		"${auth_player_name}", user->name,
		"${auth_session}", auth_session,
		"${auth_uuid}", user->uuid,
		"${auth_xuid}", "0",
		"${clientid}", "-",
		"${version_name}", id,
		"${game_directory}", instance->location,
		"${assets_root}", assets_dir,
		"${assets_index_name}", json_get_string(json, "assets"),
		"${auth_access_token}", user->accessToken,
		"${user_properties}", "{}",
		"${user_type}", user->type == ACCOUNT_TYPE_MSA ? "msa" : "legacy",
		"${version_type}", json_get_string(json, "type"),
		"${launcher_name}", LAUNCHER_NAME,
		"${launcher_version}", LAUNCHER_VERSION,
		"${classpath}", cp,
		"${natives_directory}", natives_dir,
		"${resolution_width}", width_str,
		"${resolution_height}", height_str,
		"${instance_icon}", instance->icon,
		// TODO quickplay
		NULL};
	GSList *features = NULL;
	features = g_slist_append(features, g_strdup("has_custom_resolution"));
	if(settings.demo) {
		features = g_slist_append(features, g_strdup("is_demo_user"));
	}
	int c = 0;
	main_class = json_get_string(json, "mainClass");

	c = 0;
	char *argv[256];
	argv[c++] = instance->javaLocation ? instance->javaLocation : "java"; /* Try java from path unless explicitly set */
// JVM args
#ifdef __linux
	argv[c++] = "-Dorg.lwjgl.glfw.libname=/usr/lib/libglfw.so";
#endif
	if(json_object_is_type(argumentsJvm, json_type_array)) {
		int k = add_arguments(argumentsJvm, replaces, features, argv + c, malloc_strs + m);
		c += k;
		m += k;
	} else {
		snprintf(path, PATH_MAX, "-Djava.library.path=%s", natives_dir);
		argv[c++] = path;
		argv[c++] = "-cp";
		argv[c++] = cp;
	}
	argv[c++] = (char *)main_class;

	// Game args
	if(json_object_is_type(argumentsGame, json_type_array)) {
		int k = add_arguments(argumentsGame, replaces, features, argv + c, malloc_strs + m);
		c += k;
		m += k;
	} else {
		if(!minecraftArguments) {
			goto cleanup;
		}
		char *gameArgs[255 - c];
		char *str = g_strdup(minecraftArguments);
		int argsCount = strsplit(str, ' ', gameArgs, 255 - c);
		for(int i = 0; i < argsCount; i++) {
			argv[c++] = malloc_strs[m++] = apply_replaces(replaces, gameArgs[i]);
		}
		free(str);
		if(settings.fullscreen) {
			argv[c++] = "--fullscreen";
		}
		if(settings.width != 0) {
			argv[c++] = "--width";
			argv[c++] = width_str;
		}
		if(settings.height != 0) {
			argv[c++] = "--height";
			argv[c++] = height_str;
		}
		if(settings.demo) {
			argv[c++] = "--demo";
		}
	}
	argv[c] = NULL;

	ret = true;
	for(int i = 0; i < c; i++) {
		g_print("%s\n", argv[i]);
	}
	GPid pid = util_fork_execv(instance->location, argv);

	run_callback(instance_started, pid);
	int status = 0;
	if(util_waitpid(pid, &status)) {
		g_print("Process exited with code %d\n", status);
	} else {
		g_print("Process exited abnormally\n");
	}
	if(callbacks.instance_finished) {
		callbacks.instance_finished(callbacks.userdata);
	}
cleanup:

	for(int i = 0; i < m; i++) {
		free(malloc_strs[i]);
	}
	free(cp);
	json_object_put(json);
	return ret;
}

void microlauncher_set_callbacks(struct Callbacks cb) {
	callbacks = cb;
}

GSList **microlauncher_get_accounts(void) {
	return &accounts;
}

GSList **microlauncher_get_instances(void) {
	return &instances;
}

static void microlauncher_deinit(void) {
	curl_easy_cleanup(globalCurlHandle);
}

int main(int argc, char **argv) {
	if(!microlauncher_init(argc, argv)) {
		return EXIT_FAILURE;
	}

	if(microlauncher_launch_instance(active_instance, active_user)) {
		return EXIT_SUCCESS;
	}

	int exit = microlauncher_gui_show();
	microlauncher_save_settings();
	microlauncher_deinit();
	return exit;
}
