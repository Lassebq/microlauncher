#pragma once
#include <glib.h>
#include <glib-object.h>

typedef struct {
    gchar *name;
    GType type;
    size_t memberOffs;
    GParamFlags flags;
    GFreeFunc free_func;
} PropertyDef;

void gobj_util_set_prop(GObject *obj, PropertyDef prop, const GValue *value);

void gobj_util_get_prop(GObject *obj, PropertyDef prop, GValue *value);

GParamSpec *gobj_util_param_spec(PropertyDef prop);
