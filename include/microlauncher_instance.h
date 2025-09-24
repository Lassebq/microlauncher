#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

struct _MicrolauncherInstance {
	GObject parent_instance;
	char *name;
	char *location;
	char *version;
	char *icon;
	char *javaLocation;
	GSList *extraGameArgs;
	GSList *jvmArgs;
};

G_DECLARE_FINAL_TYPE(MicrolauncherInstance, microlauncher_instance, MICROLAUNCHER, INSTANCE, GObject);

MicrolauncherInstance *microlauncher_instance_clone(MicrolauncherInstance *inst);

MicrolauncherInstance *microlauncher_instance_new(void);

MicrolauncherInstance *microlauncher_instance_clone(MicrolauncherInstance *inst);

G_END_DECLS