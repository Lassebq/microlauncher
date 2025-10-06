#include <json.h>
#include <json_object.h>
#include <json_types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <util/util.h>

int32_t json_get_int(json_object *obj, const char *key) {
	json_object *val = json_object_object_get(obj, key);
	return (json_object_is_type(val, json_type_int)) ? json_object_get_int(val) : 0;
}

int64_t json_get_int64(json_object *obj, const char *key) {
	json_object *val = json_object_object_get(obj, key);
	return (json_object_is_type(val, json_type_int)) ? json_object_get_int64(val) : 0;
}

const char *json_get_string(json_object *obj, const char *key) {
	json_object *val = json_object_object_get(obj, key);
	return (json_object_is_type(val, json_type_string)) ? json_object_get_string(val) : NULL;
}

bool json_get_bool_fallback(json_object *obj, const char *key, bool fallback) {
	json_object *val = json_object_object_get(obj, key);
	return (json_object_is_type(val, json_type_boolean)) ? json_object_get_boolean(val) : fallback;
}

bool json_get_bool(json_object *obj, const char *key) {
	json_object *val = json_object_object_get(obj, key);
	return (json_object_is_type(val, json_type_boolean)) ? json_object_get_boolean(val) : false;
}

void json_set_string(json_object *obj, const char *key, const char *value) {
	if(!value) {
		return;
	}
	json_object *val = json_object_object_get(obj, key);
	if(!json_object_is_type(val, json_type_string)) {
		val = json_object_new_string(value);
		json_object_object_add(obj, key, val);
		return;
	}
	json_object_object_del(obj, key);
	json_object_set_string(val, value);
}

void json_set_int(json_object *obj, const char *key, int value) {
	json_object *val = json_object_object_get(obj, key);
	if(!json_object_is_type(val, json_type_int)) {
		val = json_object_new_int(value);
		json_object_object_add(obj, key, val);
		return;
	}
	json_object_object_del(obj, key);
	json_object_set_int(val, value);
}

void json_set_bool(json_object *obj, const char *key, bool value) {
	json_object *val = json_object_object_get(obj, key);
	if(!json_object_is_type(val, json_type_boolean)) {
		val = json_object_new_boolean(value);
		json_object_object_add(obj, key, val);
		return;
	}
	json_object_object_del(obj, key);
	json_object_set_boolean(val, value);
}
const char *json_to_string(json_object *json) {
	return json_object_to_json_string_ext(json, JSON_C_TO_STRING_NOSLASHESCAPE);
}

json_object *json_from_file(const char *path) {
	char *str = read_file_as_string(path);
	if(str) {
		json_object *obj = json_tokener_parse(str);
		free(str);
		return obj;
	}
	return NULL;
}

bool json_to_file(json_object *obj, const char *path, int flags) {
	if(obj) {
		const char *str = json_object_to_json_string_ext(obj, flags);
		return write_string_to_file(str, path);
	}
	return false;
}
