#include <util/xdgutil.h>
#include <glib.h>

#if defined(__unix) || defined(__APPLE__)
#include <pwd.h>
#endif

#ifdef __linux
#include <linux/limits.h>
#endif

#if _WIN32
#include <shlobj.h>
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *HOME;
char *TEMPDIR;
char *XDG_DATA_HOME;
char *XDG_CONFIG_HOME;
char *XDG_CACHE_HOME;

bool xdgutil_init(void) {
	char dir[PATH_MAX];

	HOME = getenv("HOME");
	if(!HOME) {
#ifdef _WIN32
		if(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, dir) == S_OK) {
			HOME = strdup(dir);
		}
#else
		HOME = getpwuid(getuid())->pw_dir;
#endif
	}
	if(!HOME) {
		return false;
	}
#ifdef _WIN32
	if(!GetTempPathA(PATH_MAX, dir)) {
		return false;
	}
	dir[strlen(dir) - 1] = '\0'; // Remove last backslash
	TEMPDIR = strdup(dir);
#else
	TEMPDIR = "/tmp";
#endif

	XDG_DATA_HOME = getenv("XDG_DATA_HOME");
	if(!XDG_DATA_HOME) {
#ifdef _WIN32

		if(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, dir) == S_OK) {
			XDG_DATA_HOME = strdup(dir);
		} else {
#endif
			snprintf(dir, PATH_MAX, "%s/%s", HOME, ".local/share");
			XDG_DATA_HOME = strdup(dir);
#ifdef _WIN32
		}
#endif
	}

	XDG_CONFIG_HOME = getenv("XDG_CONFIG_HOME");
	if(!XDG_CONFIG_HOME) {
#ifdef _WIN32

		if(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, dir) == S_OK) {
			XDG_CONFIG_HOME = strdup(dir);
		} else {
#endif
			snprintf(dir, PATH_MAX, "%s/%s", HOME, ".config");
			XDG_CONFIG_HOME = strdup(dir);
#ifdef _WIN32
		}
#endif
	}

	XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
	if(!XDG_CACHE_HOME) {
		snprintf(dir, PATH_MAX, "%s/%s", HOME, ".cache");
		XDG_CACHE_HOME = strdup(dir);
	}
	return true;
}
