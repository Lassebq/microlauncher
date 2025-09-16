#include <glib.h>
#include <glib/gprintf.h>
#include <json.h>
#include <json_object.h>
#include <json_types.h>
#include <microlauncher.h>
#include <microlauncher_msa.h>
#include <stdlib.h>
#include <util/util.h>
#include <util/json_util.h>

bool microlauncher_msa_xboxlive_auth(struct MicrosoftUser *user) {
	time_t tm;
	time(&tm);
	if(tm < user->valid_until - 5) {
		return true;
	}
	json_object *post = json_object_new_object();
	json_object *Properties = json_object_new_object();
	json_set_string(Properties, "AuthMethod", "RPS");
	json_set_string(Properties, "SiteName", "user.auth.xboxlive.com");
	char *strdup = g_strdup_printf("d=%s", user->access_token);
	json_set_string(Properties, "RpsTicket", strdup);
	json_object_object_add(post, "Properties", Properties);
	json_set_string(post, "RelyingParty", "http://auth.xboxlive.com");
	json_set_string(post, "TokenType", "JWT");
	free(strdup);
	const char *str = json_to_string(post);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "Accept: application/json");
	json_object *response = microlauncher_http_get_json(URL_XBL_AUTH, headers, str);
	json_object_put(post);
	str = json_get_string(response, "Token");
	if(!str) {
		return false;
	}
	user->xbl_token = g_strdup(str);
	json_object *displayClaims = json_object_object_get(response, "DisplayClaims");
	if(json_object_is_type(displayClaims, json_type_object)) {
		json_object *arr = json_object_object_get(displayClaims, "xui");
		if(json_object_is_type(arr, json_type_array)) {
			size_t n = json_object_array_length(arr);
			for(size_t i = 0; i < n; i++) {
				json_object *iter = json_object_array_get_idx(arr, i);
				if(json_object_is_type(iter, json_type_object)) {
					user->uhs = g_strdup(json_get_string(iter, "uhs"));
					break;
				}
			}
		}
	}
	json_object_put(response);
	return true;
}

bool microlauncher_msa_xboxlive_xsts(struct MicrosoftUser *user, char **error_message) {
	time_t tm;
	time(&tm);
	if(tm < user->valid_until - 5) {
		return true;
	}
	json_object *post = json_object_new_object();
	json_object *Properties = json_object_new_object();
	json_object *arr = json_object_new_array();
	json_set_string(Properties, "SandboxId", "RETAIL");
	json_object_object_add(Properties, "UserTokens", arr);
	json_object_array_add(arr, json_object_new_string(user->xbl_token));
	json_object_object_add(post, "Properties", Properties);
	json_set_string(post, "RelyingParty", "rp://api.minecraftservices.com/");
	json_set_string(post, "TokenType", "JWT");
	const char *str = json_to_string(post);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "Accept: application/json");
	json_object *response = microlauncher_http_get_json(URL_XBL_XSTS, headers, str);
	if(!response) {
		return false;
	}
	json_object_put(post);
	str = json_get_string(response, "Token");
	if(!str) {
		const char *msg = NULL;
		int64_t xerr = json_get_int64(response, "XErr");
		switch(xerr) {
			case 2148916233:
				msg = "You don't have an Xbox account!";
				break;
			case 2148916235:
				msg = "Xbox Live is banned in your country!";
				break;
			case 2148916236:
			case 2148916237:
				msg = "Your account needs adult verification (South Korea)";
				break;
			case 2148916238:
				msg = "The account is a child and cannot proceed unless the account is added to a Family by an adult.";
				break;
			default:
				msg = json_get_string(response, "Message");
				if(!msg) {
					msg = "Unknown";
				}
				break;
		}

		*error_message = g_strdup_printf("Xbox Error %ld: %s", xerr, msg);
		return false;
	}
	user->xsts_token = g_strdup(str);
	json_object_put(response);
	return true;
}

bool microlauncher_msa_login(struct MicrosoftUser *user, char **error_message) {
	time_t tm;
	time(&tm);
	if(tm < user->valid_until - 5) {
		return true;
	}
	json_object *post = json_object_new_object();
	char *str = g_strdup_printf("XBL3.0 x=%s;%s", user->uhs, user->xsts_token);
	json_set_string(post, "identityToken", str);
	free(str);
	const char *postStr = json_to_string(post);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "Accept: application/json");
	json_object *response = microlauncher_http_get_json(URL_MCAPI_XBOXLOGIN, headers, postStr);
	json_object_put(post);
	if(!response) {
		return false;
	}
	char *accessToken = g_strdup(json_get_string(response, "access_token"));
	if(!accessToken) {
		*error_message = g_strdup(json_get_string(response, "error"));
		json_object_put(response);
		return false;
	}
	user->mc_access_token = accessToken;
	time(&tm);
	user->valid_until = tm + json_get_int(response, "expires_in");
	json_object_put(response);
	return true;
}

struct MinecraftProfile microlauncher_msa_get_profile(const char *accessToken) {
	struct MinecraftProfile profile = {0};
	struct curl_slist *headers = NULL;
	char *str = g_strdup_printf("Authorization: Bearer %s", accessToken);
	headers = curl_slist_append(headers, str);
	json_object *response = microlauncher_http_get_json(URL_MCAPI_PROFILE, headers, NULL);
	free(str);
	profile.username = g_strdup(json_get_string(response, "name"));
	profile.uuid = g_strdup(json_get_string(response, "id"));
	json_object_put(response);
	return profile;
}

/**
 * Return value:
 * 0 - successful response
 * 1 - authorization pending
 * -1 - cancel/other error
 */
int microlauncher_msa_check_token(const char *postcontent, struct MicrosoftUser *user) {
	time_t tm;
	time(&tm);
	if(tm < user->valid_until - 5) {
		return 0;
	}
	if(tm < user->oauth_valid_until - 5) {
		return 0;
	}
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	headers = curl_slist_append(headers, "Accept: application/json");
	json_object *obj = microlauncher_http_get_json(URL_OAUTH2_TOKEN, headers, postcontent);
	const char *error = json_get_string(obj, "error");
	if(strequal(error, "authorization_pending")) {
		return 1;
	} else if(error != NULL) {
		return -1;
	} else {
		if(strequal(json_get_string(obj, "token_type"), "Bearer")) {
			user->access_token = g_strdup(json_get_string(obj, "access_token"));
			user->refresh_token = g_strdup(json_get_string(obj, "refresh_token"));

			time(&tm);
			user->oauth_valid_until = tm + json_get_int(obj, "expires_in");
			return 0;
		}
	}
	return -1;
}

int microlauncher_msa_device_token(const char *device_code, struct MicrosoftUser *user) {
	char *postcontent = g_strdup_printf("grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=%s&device_code=%s", MICROSOFT_CLIENT_ID, device_code);
	int ret = microlauncher_msa_check_token(postcontent, user);
	free(postcontent);
	return ret;
}

int microlauncher_msa_refresh_token(const char *refresh_token, struct MicrosoftUser *user) {
	char *postcontent = g_strdup_printf("grant_type=refresh_token&client_id=%s&refresh_token=%s", MICROSOFT_CLIENT_ID, refresh_token);
	int ret = microlauncher_msa_check_token(postcontent, user);
	free(postcontent);
	return ret;
}

struct DeviceCodeOAuthResponse microlauncher_msa_get_device_code(void) {
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	headers = curl_slist_append(headers, "Accept: application/json");
	json_object *obj = microlauncher_http_get_json(URL_OAUTH2_DEVICE, headers, "client_id=" MICROSOFT_CLIENT_ID "&scope=XboxLive.signin%20offline_access");

	struct DeviceCodeOAuthResponse response = {0};
	response.user_code = g_strdup(json_get_string(obj, "user_code"));
	response.device_code = g_strdup(json_get_string(obj, "device_code"));
	response.verification_uri = g_strdup(json_get_string(obj, "verification_uri"));
	response.expires_in = json_get_int(obj, "expires_in");
	response.interval = json_get_int(obj, "interval");
	return response;
}

void microlauncher_msa_destroy_device_code(struct DeviceCodeOAuthResponse *response) {
	if(response->device_code) {
		free(response->device_code);
	}
	if(response->user_code) {
		free(response->user_code);
	}
	if(response->verification_uri) {
		free(response->verification_uri);
	}
	free(response);
}
