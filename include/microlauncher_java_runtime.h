

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

struct _JavaRuntime {
	GObject parent_instance;
	char *version;
	char *location;
};

G_DECLARE_FINAL_TYPE(JavaRuntime, microlauncher_java_runtime, MICROLAUNCHER, JAVA_RUNTIME, GObject);

JavaRuntime *microlauncher_java_runtime_new(const char *version, const char *location);

G_END_DECLS