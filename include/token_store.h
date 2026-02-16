#ifndef GUARD_TOKEN_STORE_H
#define GUARD_TOKEN_STORE_H
#include "platform.h"
#include <ctime>
#include <string>
#include <curl/curl.h>

const std::string KEYCHAIN_SERVICE = "plsync-token-service";

bool save_access_tkn(Platform platform, const std::string &tkn, std::time_t duration);
bool save_refresh_tkn(Platform platform, const std::string &tkn);

/* 
 * Get access token or refresh it if it's expired.
 * Only call this if you're sure that refresh_token 
 * is valid. Use is_refresh_tkn_valid to check
 * otherwise you must initialise the platform,
 * see 'init.h'.
 */
bool get_or_fetch_access_tkn(Platform platform, std::shared_ptr<CURL> curl, std::string &tkn);
bool get_refresh_tkn(Platform platform, std::string &tkn);


bool is_refresh_tkn_valid(Platform platform);
#endif
