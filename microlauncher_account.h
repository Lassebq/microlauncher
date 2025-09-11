#pragma once

#include "microlauncher_types.h"
#include <stdbool.h>
#include <glib-object.h>
#include "gio/gio.h"

G_BEGIN_DECLS

enum AccountType {
	ACCOUNT_TYPE_OFFLINE = 0,
	ACCOUNT_TYPE_MSA = 1
};

struct _MicrolauncherAccount {
	GObject parent_instance;
	char *name;
	char *uuid;
	char *id;
	enum AccountType type;
	void *data;
};

const char *microlauncher_get_account_type_name(enum AccountType accountType);

#define MICROLAUNCHER_ACCOUNT_TYPE (microlauncher_account_get_type())
G_DECLARE_FINAL_TYPE(MicrolauncherAccount, microlauncher_account, MICROLAUNCHER, ACCOUNT, GObject);

bool microlauncher_account_auth_user(struct Callbacks callbacks, MicrolauncherAccount *user, GCancellable *cancellable);

void microlauncher_account_set_name(MicrolauncherAccount *self, const char *name);

void microlauncher_account_set_uuid(MicrolauncherAccount *self, const char *uuid);

void microlauncher_account_set_userdata(MicrolauncherAccount *self, void *data);

MicrolauncherAccount *microlauncher_account_new(const char *name, enum AccountType type);

G_END_DECLS