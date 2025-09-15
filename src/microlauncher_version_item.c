#include <glib-object.h>
#include <glib.h>
#include <microlauncher_version_item.h>

enum {
	PROP_VERSION_LABEL = 1,
	PROP_VERSION_TYPE,
	PROP_RELEASE_TIME,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = {NULL};

G_DEFINE_TYPE(VersionItem, microlauncher_version_item, G_TYPE_OBJECT)

static void microlauncher_version_item_init(VersionItem *self) {
	self->version = NULL;
	self->type = NULL;
	self->releaseTime = NULL;
}

void microlauncher_version_item_dispose(GObject *self) {
	VersionItem *item = MICROLAUNCHER_VERSION_ITEM(self);
	g_clear_pointer(&item->version, g_free);
	g_clear_pointer(&item->type, g_free);
	g_clear_pointer(&item->releaseTime, g_free);
	G_OBJECT_CLASS(microlauncher_version_item_parent_class)->dispose(self);
}

static void microlauncher_version_item_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	VersionItem *self = MICROLAUNCHER_VERSION_ITEM(object);
	switch(property_id) {
		case PROP_VERSION_LABEL:
			g_free(self->version);
			self->version = g_strdup(g_value_get_string(value));
			return;
		case PROP_VERSION_TYPE:
			g_free(self->type);
			self->type = g_strdup(g_value_get_string(value));
			return;
		case PROP_RELEASE_TIME:
			g_free(self->releaseTime);
			self->releaseTime = g_strdup(g_value_get_string(value));
			return;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void microlauncher_version_item_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	VersionItem *self = MICROLAUNCHER_VERSION_ITEM(object);
	switch(property_id) {
		case PROP_VERSION_LABEL:
			g_value_set_string(value, self->version);
			return;
		case PROP_VERSION_TYPE:
			g_value_set_string(value, self->type);
			return;
		case PROP_RELEASE_TIME:
			g_value_set_string(value, self->releaseTime);
			return;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void microlauncher_version_item_class_init(VersionItemClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = microlauncher_version_item_dispose;

	properties[PROP_VERSION_LABEL] = g_param_spec_string(
		"version", NULL, NULL,
		NULL, G_PARAM_READABLE | G_PARAM_WRITABLE);

	properties[PROP_VERSION_TYPE] = g_param_spec_string(
		"type", NULL, NULL,
		NULL, G_PARAM_READABLE | G_PARAM_WRITABLE);

	properties[PROP_RELEASE_TIME] = g_param_spec_string(
		"releaseTime", NULL, NULL,
		NULL, G_PARAM_READABLE | G_PARAM_WRITABLE);
	object_class->get_property = microlauncher_version_item_get_property;
	object_class->set_property = microlauncher_version_item_set_property;
	g_object_class_install_properties(object_class, G_N_ELEMENTS(properties), properties);
}

VersionItem *microlauncher_version_item_new(const char *version, const char *type, const char *releaseTime) {
	VersionItem *self = g_object_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL);
	self->version = g_strdup(version);
	self->type = g_strdup(type);
	self->releaseTime = g_strdup(releaseTime);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_VERSION_LABEL]);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_VERSION_TYPE]);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_RELEASE_TIME]);
	return self;
}
