#pragma once
#include <glib-object.h>
#include <glib.h>
#include <stdint.h>

typedef struct {
	gchar *name;
	GType type;
	int32_t memberOffs;
	GParamFlags flags;
	GFreeFunc free_func;
} PropertyDef;

void gobj_util_init_prop(GObject *obj, PropertyDef prop);

void gobj_util_dispose_prop(GObject *obj, PropertyDef prop);

gboolean gobj_util_set_prop(GObject *obj, PropertyDef prop, const GValue *value);

gboolean gobj_util_get_prop(GObject *obj, PropertyDef prop, GValue *value);

GParamSpec *gobj_util_param_spec(PropertyDef prop);
