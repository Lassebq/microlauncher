#include "microlauncher_java_runtime.h"
#include <glib-object.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <util/gobject_util.h>
#include <util/util.h>

enum {
	PROP_VERSION = 1,
	PROP_LOCATION,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = {NULL};

// clang-format off
static PropertyDef prop_definitions[N_PROPERTIES] = {
	[PROP_VERSION] = {
		"version",
		G_TYPE_STRING,
		offsetof(JavaRuntime, version),
		G_PARAM_READABLE,
		g_free
	},
	[PROP_LOCATION] = {
		"location",
		G_TYPE_STRING,
		offsetof(JavaRuntime, location),
		G_PARAM_READWRITE,
		g_free
	}
};
// clang-format on

G_DEFINE_TYPE(JavaRuntime, microlauncher_java_runtime, G_TYPE_OBJECT)

static void microlauncher_java_runtime_init(JavaRuntime *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		gobj_util_init_prop(G_OBJECT(self), def);
	}
}

void microlauncher_java_runtime_dispose(GObject *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		gobj_util_dispose_prop(self, def);
	}
	G_OBJECT_CLASS(microlauncher_java_runtime_parent_class)->dispose(self);
}

static void set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	PropertyDef def = prop_definitions[property_id];
	if(gobj_util_set_prop(object, def, value)) {
		return;
	}
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
}

static void get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	JavaRuntime *self = MICROLAUNCHER_JAVA_RUNTIME(object);
	PropertyDef def = prop_definitions[property_id];
	if(property_id == PROP_VERSION && !self->version && self->location) {
		// #ifdef G_OS_WIN32
		// 		if(!str_ends_with(self->location, "java.exe")) {
		// 			if(!str_ends_with(self->location, "javaw.exe")) {
		// 				return;
		// 			}
		// 		}
		// #else
		// 		if(!str_ends_with(self->location, "java")) {
		// 			return;
		// 		}
		// #endif
		char *out = util_str_execv(NULL, (char *[]){self->location, "-version", NULL});
		if(!out) {
			return;
		}
		// g_print("%s", out);
		int minStr, maxStr;
		char *str, *str2;
		str = strstr(out, "version ");
		if(str) {
			minStr = (str - out) + strlen("version ");
			str2 = out + strlen(out);
			str = strchr(out, '\n');
			if(str) {
				str2 = MIN(str2, str);
			}
			str = strchr(out, ' ');
			if(str) {
				str2 = MIN(str2, str);
			}
			maxStr = (str2 - out);
			if(out[minStr] == '"') {
				str = strchr(out + minStr + 1, '"');
				if(str) {
					maxStr = str - out;
					minStr++;
				}
			}
			char *ver = g_memdup2(out + minStr, maxStr - minStr + 1);
			if(ver) {
				ver[maxStr - minStr] = '\0';
				self->version = ver;
				g_object_notify_by_pspec(object, properties[PROP_VERSION]);
			}
		}
		free(out);
	}
	if(gobj_util_get_prop(object, def, value)) {
		return;
	}
	G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
}

static void microlauncher_java_runtime_class_init(JavaRuntimeClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = microlauncher_java_runtime_dispose;

	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			properties[i] = gobj_util_param_spec(def);
		}
	}
	object_class->get_property = get_property;
	object_class->set_property = set_property;
	g_object_class_install_properties(object_class, G_N_ELEMENTS(properties), properties);
}

JavaRuntime *microlauncher_java_runtime_new(const char *version, const char *location) {
	JavaRuntime *self = g_object_new(microlauncher_java_runtime_get_type(), NULL);
	self->version = g_strdup(version);
	self->location = g_strdup(location);
	return self;
}
