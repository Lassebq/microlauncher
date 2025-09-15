#include <microlauncher_instance.h>
#include <util/util.h>
#include <gobject_util.h>
#include <glib-object.h>
#include <glib.h>
#include <stdint.h>

enum {
	PROP_NAME = 1,
	PROP_LOCATION,
	PROP_VERSION,
	PROP_ICON,
	PROP_JAVA_LOCATION,
	PROP_GAME_ARGS_LIST,
	PROP_JVM_ARGS_LIST,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = {NULL};

static PropertyDef prop_definitions[N_PROPERTIES] = {
	[PROP_NAME] = {
		"name",
		G_TYPE_STRING,
		offsetof(MicrolauncherInstance, name),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_VERSION] = {
		"version",
		G_TYPE_STRING,
		offsetof(MicrolauncherInstance, version),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_LOCATION] = {
		"location",
		G_TYPE_STRING,
		offsetof(MicrolauncherInstance, location),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_JAVA_LOCATION] = {
		"java-location",
		G_TYPE_STRING,
		offsetof(MicrolauncherInstance, javaLocation),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_ICON] = {
		"icon",
		G_TYPE_STRING,
		offsetof(MicrolauncherInstance, icon),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_GAME_ARGS_LIST] = {
		"game-args",
		G_TYPE_POINTER,
		offsetof(MicrolauncherInstance, extraGameArgs),
		G_PARAM_READWRITE,
		(GFreeFunc)g_slist_free
	},
	[PROP_JVM_ARGS_LIST] = {
		"jvm-args",
		G_TYPE_POINTER,
		offsetof(MicrolauncherInstance, jvmArgs),
		G_PARAM_READWRITE,
		(GFreeFunc)g_slist_free
	}
};

G_DEFINE_TYPE(MicrolauncherInstance, microlauncher_instance, G_TYPE_OBJECT)

static void microlauncher_instance_init(MicrolauncherInstance *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			*(void **)offset_apply(self, def.memberOffs) = NULL;
		}
	}
}

void microlauncher_instance_dispose(GObject *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			g_clear_pointer((void **)offset_apply(self, def.memberOffs), def.free_func);
		}
	}
	G_OBJECT_CLASS(microlauncher_instance_parent_class)->dispose(self);
}

static void microlauncher_instance_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	PropertyDef def = prop_definitions[property_id];
	if(def.name) {
		gobj_util_set_prop(object, def, value);
		g_object_notify_by_pspec(object, properties[property_id]);
		return;
	} else {
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void microlauncher_instance_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	PropertyDef def = prop_definitions[property_id];
	gobj_util_get_prop(object, def, value);
}

static void microlauncher_instance_class_init(MicrolauncherInstanceClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = microlauncher_instance_dispose;

	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			properties[i] = gobj_util_param_spec(def);
		}
	}
	object_class->get_property = microlauncher_instance_get_property;
	object_class->set_property = microlauncher_instance_set_property;
	g_object_class_install_properties(object_class, G_N_ELEMENTS(properties), properties);
}

MicrolauncherInstance *microlauncher_instance_clone(MicrolauncherInstance *inst) {
	MicrolauncherInstance *self = g_object_new(MICROLAUNCHER_INSTANCE_TYPE, NULL);
	GValue value;
	for(guint i = 1; i < N_PROPERTIES; i++) {
		value = (GValue)G_VALUE_INIT;
		PropertyDef def = prop_definitions[i];
		if(i == PROP_GAME_ARGS_LIST || i == PROP_JVM_ARGS_LIST) {
			g_object_get_property(G_OBJECT(inst), def.name, &value);
			GSList *newList = NULL;
			GSList *node = g_value_get_pointer(&value);
			while(node) {
				newList = g_slist_append(newList, g_strdup(node->data));
				node = node->next;
			}
			g_value_set_pointer(&value, newList);
			g_object_set_property(G_OBJECT(self), def.name, &value);
			continue;
		}
		if(def.name) {
			g_object_get_property(G_OBJECT(inst), def.name, &value);
			g_object_set_property(G_OBJECT(self), def.name, &value);
		}
	}
	return self;
}

MicrolauncherInstance *microlauncher_instance_new(void) {
	MicrolauncherInstance *self = g_object_new(MICROLAUNCHER_INSTANCE_TYPE, NULL);
	return self;
}
