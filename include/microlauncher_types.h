#pragma once
#include <gio/gio.h>

struct Callbacks {
	void (*instance_started)(GPid pid, void *userdata);
	void (*instance_finished)(void *userdata);
	void (*progress_update)(double precentage, const char *progress_msg, void *userdata);
	void (*stage_update)(const char *progress_msg, void *userdata);
	void (*show_error)(const char *error_message, void *userdata);
	void *userdata;
};

#define run_callback(cb, ...)                          \
	if(callbacks.cb) {                                 \
		callbacks.cb(__VA_ARGS__, callbacks.userdata); \
	}

struct Version {
	char *id;
	char *type;
	char *releaseTime;
	char *url;
	char *sha1;
};