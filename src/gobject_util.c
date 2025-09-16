#include <glib-object.h>
#include <glibconfig.h>
#include <util/gobject_util.h>
#include <stdio.h>
#include <util/util.h>

void gobj_util_set_prop(GObject *obj, PropertyDef prop, const GValue *value) {
	void **member = offset_apply(obj, prop.memberOffs);
	prop.free_func(*member);
	switch(prop.type) {
		case G_TYPE_STRING:
			*(gchar **)member = g_strdup(g_value_get_string(value));
			break;
		case G_TYPE_POINTER:
			*member = g_value_get_pointer(value);
			break;
		case G_TYPE_INT:
			*(gint *)member = g_value_get_int(value);
			break;
		case G_TYPE_INT64:
			*(gint64 *)member = g_value_get_int64(value);
			break;
		case G_TYPE_LONG:
			*(glong *)member = g_value_get_long(value);
			break;
		case G_TYPE_ULONG:
			*(gulong *)member = g_value_get_ulong(value);
			break;
		case G_TYPE_FLOAT:
			*(gfloat *)member = g_value_get_float(value);
			break;
		case G_TYPE_DOUBLE:
			*(gdouble *)member = g_value_get_double(value);
			break;
		case G_TYPE_UINT:
			*(guint *)member = g_value_get_uint(value);
			break;
		case G_TYPE_UINT64:
			*(guint64 *)member = g_value_get_uint64(value);
			break;
		case G_TYPE_BOOLEAN:
			*(gboolean *)member = g_value_get_boolean(value);
			break;
		default:
			break;
	}
	g_object_notify(obj, prop.name);
}

void gobj_util_get_prop(GObject *obj, PropertyDef prop, GValue *value) {
	void **member = offset_apply(obj, prop.memberOffs);
	switch(prop.type) {
		case G_TYPE_STRING:
			g_value_set_string(value, *(gchar **)member);
			break;
		case G_TYPE_POINTER:
			g_value_set_pointer(value, *member);
			break;
		case G_TYPE_INT:
			g_value_set_int(value, *(gint *)member);
			break;
		case G_TYPE_INT64:
			g_value_set_int64(value, *(gint64 *)member);
			break;
		case G_TYPE_LONG:
			g_value_set_long(value, *(glong *)member);
			break;
		case G_TYPE_ULONG:
			g_value_set_ulong(value, *(gulong *)member);
			break;
		case G_TYPE_FLOAT:
			g_value_set_float(value, *(gfloat *)member);
			break;
		case G_TYPE_DOUBLE:
			g_value_set_double(value, *(gdouble *)member);
			break;
		case G_TYPE_UINT:
			g_value_set_uint(value, *(guint *)member);
			break;
		case G_TYPE_UINT64:
			g_value_set_uint64(value, *(guint64 *)member);
			break;
		case G_TYPE_BOOLEAN:
			g_value_set_boolean(value, *(gboolean *)member);
			break;
		default:
			break;
	}
}

GParamSpec *gobj_util_param_spec(PropertyDef prop) {
	switch(prop.type) {
		case G_TYPE_STRING:
			return g_param_spec_string(prop.name, NULL, NULL, NULL, prop.flags);
		case G_TYPE_POINTER:
			return g_param_spec_pointer(prop.name, NULL, NULL, prop.flags);
		case G_TYPE_INT:
			return g_param_spec_int(prop.name, NULL, NULL, G_MININT, G_MAXINT, 0, prop.flags);
		case G_TYPE_INT64:
			return g_param_spec_int64(prop.name, NULL, NULL, G_MININT64, G_MAXINT64, 0, prop.flags);
		case G_TYPE_LONG:
			return g_param_spec_long(prop.name, NULL, NULL, G_MINLONG, G_MAXLONG, 0, prop.flags);
		case G_TYPE_ULONG:
			return g_param_spec_ulong(prop.name, NULL, NULL, 0, G_MAXULONG, 0, prop.flags);
		case G_TYPE_FLOAT:
			return g_param_spec_float(prop.name, NULL, NULL, G_MINFLOAT, G_MAXFLOAT, 0.0, prop.flags);
		case G_TYPE_DOUBLE:
			return g_param_spec_double(prop.name, NULL, NULL, G_MINDOUBLE, G_MAXDOUBLE, 0.0, prop.flags);
		case G_TYPE_UINT:
			return g_param_spec_uint(prop.name, NULL, NULL, 0, G_MAXUINT, 0, prop.flags);
		case G_TYPE_UINT64:
			return g_param_spec_uint64(prop.name, NULL, NULL, 0, G_MAXUINT64, 0, prop.flags);
		case G_TYPE_BOOLEAN:
			return g_param_spec_boolean(prop.name, NULL, NULL, false, prop.flags);
		default:
			return NULL;
	}
}
