#include "xdgutil.h"
#include "linux/limits.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include <pwd.h>
#include <string.h>
#include <unistd.h>

char *HOME;
char *XDG_DATA_HOME;
char *XDG_CONFIG_HOME;
char *XDG_CACHE_HOME;

bool xdgutil_init(void) {
	HOME = getenv("HOME");
	if(!HOME) {
		HOME = getpwuid(getuid())->pw_dir;
	}
	if(!HOME) {
		return false;
	}
	char dir[PATH_MAX];

	XDG_DATA_HOME = getenv("XDG_DATA_HOME");
	if(!XDG_DATA_HOME) {
		snprintf(dir, PATH_MAX, "%s/%s", HOME, ".local/share");
		XDG_DATA_HOME = strdup(dir);
	}

	XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
	if(!XDG_CONFIG_HOME) {
		snprintf(dir, PATH_MAX, "%s/%s", HOME, ".config");
		XDG_CONFIG_HOME = strdup(dir);
	}

	XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
	if(!XDG_CACHE_HOME) {
		snprintf(dir, PATH_MAX, "%s/%s", HOME, ".cache");
		XDG_CACHE_HOME = strdup(dir);
	}
	return true;
}
