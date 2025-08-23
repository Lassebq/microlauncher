#pragma once

#include "glib.h"
#include <curl/curl.h>
#include <json_types.h>
#include <stdbool.h>

#define MOJANG_MANIFEST "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"
#define BETTERJSONS_MANIFEST "https://mcphackers.org/BetterJSONs/version_manifest_v2.json"

#ifndef MICROSOFT_CLIENT_ID
#define MICROSOFT_CLIENT_ID "95984717-05f1-4b52-8a66-064d0e1e5b55"
#endif

#define LAUNCHER_NAME "MicroLauncher"

struct Callbacks {
	void (*instance_started)(GPid pid, void *userdata);
	void (*instance_finished)(void *userdata);
	void (*progress_update)(double precentage, const char *progress_msg, void *userdata);
	void (*stage_update)(const char *progress_msg, void *userdata);
	void *userdata;
};

struct Version {
	char *id;
	char *type;
	char *releaseTime;
	char *url;
	char *sha1;
};

enum AccountType {
	ACCOUNT_TYPE_OFFLINE = 0,
	ACCOUNT_TYPE_MSA = 1
};

struct User {
	char *name;
	char *uuid;
	char *accessToken;
	char *id;
	enum AccountType type;
	void *data;
};

struct Instance {
	char *name;
	char *location;
	char *version;
	char *icon;
	char *javaLocation;
};

struct Settings {
	struct User *user;
	struct Instance *instance;
	int width;
	int height;
	bool fullscreen;
	bool demo;
	char *launcher_root;
	char *manifest_url;
};

struct Instance *microlauncher_instance_get(GSList *list, const char *id);
struct User *microlauncher_account_get(GSList *list, const char *id);
bool microlauncher_launch_instance(const struct Instance *instance, struct User *user);
void microlauncher_save_settings(void);
void microlauncher_load_settings(void);
struct Settings *microlauncher_get_settings(void);
GSList **microlauncher_get_instances(void);
GSList **microlauncher_get_accounts(void);
void microlauncher_set_callbacks(struct Callbacks callbacks);
const char *microlauncher_get_account_type_name(enum AccountType accountType);
json_object *microlauncher_http_get_json(const char *url, struct curl_slist *headers, const char *post);
bool microlauncher_auth_user(struct User *user);
GHashTable *microlauncher_get_manifest(void);