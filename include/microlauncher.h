#pragma once

#include <curl/curl.h>
#include <gio/gio.h>
#include <glib.h>
#include <json_types.h>
#include <microlauncher_account.h>
#include <microlauncher_instance.h>
#include <stdbool.h>

#define MOJANG_MANIFEST "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json"
#define BETTERJSONS_MANIFEST "https://mcphackers.org/BetterJSONs/version_manifest_v2.json"
#ifndef MANIFEST_URL
#define MANIFEST_URL BETTERJSONS_MANIFEST
#endif

#ifndef MICROSOFT_CLIENT_ID
#define MICROSOFT_CLIENT_ID "95984717-05f1-4b52-8a66-064d0e1e5b55"
#endif

#define LAUNCHER_NAME "MicroLauncher"

extern char *EXEC_BINARY;

struct Settings {
	MicrolauncherAccount *user;
	MicrolauncherInstance *instance;
	GSList *javaRuntimes;
	int width;
	int height;
	bool fullscreen;
	bool demo;
	bool allowUpdate;
	char *launcher_root;
	char *manifest_url;
	char *gpu_id;
};

MicrolauncherInstance *microlauncher_instance_get(GSList *list, const char *id);
MicrolauncherAccount *microlauncher_account_get(GSList *list, const char *id);
bool microlauncher_launch_instance(const MicrolauncherInstance *instance, MicrolauncherAccount *user, GCancellable *cancellable);
void microlauncher_save_settings(void);
void microlauncher_load_settings(void);
struct Settings *microlauncher_get_settings(void);
GSList **microlauncher_get_instances(void);
GSList **microlauncher_get_accounts(void);
void microlauncher_set_callbacks(struct Callbacks callbacks);
bool microlauncher_auth_user(MicrolauncherAccount *user, GCancellable *cancellable);
json_object *microlauncher_http_get_json(const char *url, struct curl_slist *headers, const char *post);
GHashTable *microlauncher_get_manifest(void);