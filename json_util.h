#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include "json_types.h"
#include <stdbool.h>
#include <stdint.h>

int32_t json_get_int(json_object *obj, const char *key);

const char *json_get_string(json_object *obj, const char *key);

bool json_get_bool(json_object *obj, const char *key);

void json_set_string(json_object *obj, const char *key, const char *value);

void json_set_int(json_object *obj, const char *key, int value);

void json_set_bool(json_object *obj, const char *key, bool value);

const char *json_to_string(json_object *json);

json_object *json_from_file(const char *path);

bool json_to_file(json_object *obj, const char *path, int flags);

#endif