#pragma once

#include "microlauncher.h"

#define URL_XBL_AUTH "https://user.auth.xboxlive.com/user/authenticate"
#define URL_XBL_XSTS "https://xsts.auth.xboxlive.com/xsts/authorize"
#define URL_OAUTH2_TOKEN "https://login.microsoftonline.com/consumers/oauth2/v2.0/token"
#define URL_OAUTH2_DEVICE "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode"
#define URL_MCAPI_XBOXLOGIN "https://api.minecraftservices.com/authentication/login_with_xbox"
#define URL_MCAPI_PROFILE "https://api.minecraftservices.com/minecraft/profile"

struct MinecraftProfile {
	char *username;
	char *uuid;
};

struct MicrosoftUser {
	char *access_token;
	char *refresh_token;
	char *xbl_token;
	char *uhs;
	char *xsts_token;
	char *mc_access_token;
};

struct DeviceCodeOAuthResponse {
	char *user_code;
	char *device_code;
	int expires_in;
	int interval;
	char *verification_uri;
};

bool microlauncher_msa_xboxlive_auth(struct MicrosoftUser *user);
bool microlauncher_msa_xboxlive_xsts(struct MicrosoftUser *user);
struct DeviceCodeOAuthResponse microlauncher_msa_get_device_code(void);
int microlauncher_msa_device_token(const char *device_code, struct MicrosoftUser *user);
int microlauncher_msa_refresh_token(const char *refresh_token, struct MicrosoftUser *user);
void microlauncher_msa_destroy_device_code(struct DeviceCodeOAuthResponse *response);
char *microlauncher_msa_login(struct MicrosoftUser *user);
struct MinecraftProfile microlauncher_msa_get_profile(const char *accessToken);
