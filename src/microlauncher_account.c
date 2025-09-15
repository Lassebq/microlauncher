#include <microlauncher_account.h>
#include <microlauncher_msa.h>
#include <util.h>
#include <gobject_util.h>
#include <glib-object.h>
#include <glib.h>
#include <stdint.h>

enum {
	PROP_NAME = 1,
	PROP_TYPE,
	PROP_SESSION_UUID,
	PROP_ID,
	PROP_ACCESS_TOKEN,
	PROP_DATA,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = {NULL};

static PropertyDef prop_definitions[N_PROPERTIES] = {
	[PROP_NAME] = {
		"name",
		G_TYPE_STRING,
		offsetof(MicrolauncherAccount, name),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_TYPE] = {
		"type",
		G_TYPE_INT,
		offsetof(MicrolauncherAccount, type),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_SESSION_UUID] = {
		"session-uuid",
		G_TYPE_STRING,
		offsetof(MicrolauncherAccount, uuid),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_ID] = {
		"id",
		G_TYPE_STRING,
		offsetof(MicrolauncherAccount, id),
		G_PARAM_READWRITE,
		g_free
	},
	[PROP_DATA] = {
		"data",
		G_TYPE_POINTER,
		offsetof(MicrolauncherAccount, data),
		G_PARAM_READWRITE,
		g_free
	}
};

G_DEFINE_TYPE(MicrolauncherAccount, microlauncher_account, G_TYPE_OBJECT)

static void microlauncher_account_init(MicrolauncherAccount *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			*(void **)offset_apply(self, def.memberOffs) = NULL;
		}
	}
}

void microlauncher_account_dispose(GObject *self) {
	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			g_clear_pointer((void **)offset_apply(self, def.memberOffs), def.free_func);
		}
	}
	G_OBJECT_CLASS(microlauncher_account_parent_class)->dispose(self);
}

static void microlauncher_account_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
	PropertyDef def = prop_definitions[property_id];
	if(def.name) {
		gobj_util_set_prop(object, def, value);
		g_object_notify_by_pspec(object, properties[property_id]);
		return;
	} else {
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void microlauncher_account_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
	MicrolauncherAccount *self = MICROLAUNCHER_ACCOUNT(object);
	struct MicrosoftUser *userdata = (struct MicrosoftUser *)self->data;
	PropertyDef def = prop_definitions[property_id];
	if(def.name) {
		gobj_util_get_prop(object, def, value);
	} else {
		switch (property_id) {
			case PROP_ACCESS_TOKEN:
				switch(self->type) {
					case ACCOUNT_TYPE_OFFLINE:
						g_value_set_static_string(value, "-");
						break;
					case ACCOUNT_TYPE_MSA:
						g_value_set_string(value, userdata->mc_access_token);
						break;
				}
				break;
			default:
				break;
		}
	}
}

static void microlauncher_account_class_init(MicrolauncherAccountClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = microlauncher_account_dispose;

	for(guint i = 1; i < N_PROPERTIES; i++) {
		PropertyDef def = prop_definitions[i];
		if(def.name) {
			properties[i] = gobj_util_param_spec(def);
		}
	}
	properties[PROP_ACCESS_TOKEN] = g_param_spec_string("access-token", NULL, NULL, NULL, G_PARAM_READABLE);
	object_class->get_property = microlauncher_account_get_property;
	object_class->set_property = microlauncher_account_set_property;
	g_object_class_install_properties(object_class, G_N_ELEMENTS(properties), properties);
}

static const char *ACCOUNT_TYPES[] = {
	"Offline",
	"Microsoft"};

const char *microlauncher_get_account_type_name(enum AccountType accountType) {
	return ACCOUNT_TYPES[accountType];
}

bool microlauncher_account_auth_user(struct Callbacks callbacks, MicrolauncherAccount *user, GCancellable *cancellable) {
	run_callback(stage_update, "Authentication");
	int steps, i;
	i = 1;
	const char *errorMessage;
	char *tmpError = NULL;
	switch(user->type) {
		case ACCOUNT_TYPE_OFFLINE:
			break;
		case ACCOUNT_TYPE_MSA:
			steps = 5;
			struct MicrosoftUser *msuser = user->data;
			run_callback(progress_update, (double)i++ / steps, "Checking access token");
			if(microlauncher_msa_refresh_token(msuser->refresh_token, msuser) != 0 || (cancellable && g_cancellable_is_cancelled(cancellable))) {
				errorMessage = "Failed check access token";
				goto cancel;
			}
			run_callback(progress_update, (double)i++ / steps, "Authenticating via Xbox");
			if(!microlauncher_msa_xboxlive_auth(msuser) || (cancellable && g_cancellable_is_cancelled(cancellable))) {
				errorMessage = "Could not authenticate with Xbox Live";
				goto cancel;
			}
			run_callback(progress_update, (double)i++ / steps, "Obtaining XSTS token");
			if(!microlauncher_msa_xboxlive_xsts(msuser, &tmpError) || (cancellable && g_cancellable_is_cancelled(cancellable))) {
				errorMessage = tmpError;
				goto cancel;
			}
			run_callback(progress_update, (double)i++ / steps, "Getting access token");
			if(!microlauncher_msa_login(msuser, &tmpError) || (cancellable && g_cancellable_is_cancelled(cancellable))) {
				errorMessage = tmpError ? tmpError : "Failed to get access token";
				goto cancel;
			}
			run_callback(progress_update, (double)i++ / steps, "Getting profile");
			struct MinecraftProfile profile = microlauncher_msa_get_profile(msuser->mc_access_token);
			if(!profile.username || (cancellable && g_cancellable_is_cancelled(cancellable))) {
				errorMessage = "Could not get Minecraft profile";
				goto cancel;
			}
			microlauncher_account_set_name(user, profile.username);
			microlauncher_account_set_uuid(user, profile.uuid);
			break;
	}
	run_callback(stage_update, NULL);
	return true;
cancel:
	run_callback(stage_update, NULL);
	if(!g_cancellable_is_cancelled(cancellable)) {
		run_callback(show_error, errorMessage);
	}
	free(tmpError);
	return false;
}

void microlauncher_account_set_name(MicrolauncherAccount *self, const char *name) {
	free(self->name);
	self->name = g_strdup(name);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_NAME]);
}

void microlauncher_account_set_uuid(MicrolauncherAccount *self, const char *uuid) {
	free(self->uuid);
	self->uuid = g_strdup(uuid);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_SESSION_UUID]);
}

void microlauncher_account_set_userdata(MicrolauncherAccount *self, void *data) {
	free(self->data);
	self->data = data;
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DATA]);
}

MicrolauncherAccount *microlauncher_account_new(const char *name, enum AccountType type) {
	MicrolauncherAccount *self = g_object_new(MICROLAUNCHER_ACCOUNT_TYPE, NULL);
	self->name = NULL;
	self->type = type;
	self->uuid = random_uuid();
	self->id = random_uuid();
	self->data = NULL;
	microlauncher_account_set_name(self, name);
	return self;
}
