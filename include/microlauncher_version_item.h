#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

struct _VersionItem {
	GObject parent_instance;
	char *version;
	char *type;
	char *releaseTime;
};

#define MICROLAUNCHER_VERSION_ITEM_TYPE (microlauncher_version_item_get_type())
G_DECLARE_FINAL_TYPE(VersionItem, microlauncher_version_item, MICROLAUNCHER, VERSION_ITEM, GObject);

VersionItem *microlauncher_version_item_new(const char *version, const char *type, const char *releaseDate);

G_END_DECLS