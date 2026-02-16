#include "../include/token_refresher.h"
#include "../include/token_store.h"
#include "../include/util.h"
#include <cassert>
#include <ctime>
#include <cstdio>
#include <string>
#include <iostream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

TokenRefresher::TokenRefresher(
	Platform platform, const std::string &refresh_url, 
	const std::string &client_id, const std::string &client_secret
): platform(platform), refresh_url(refresh_url), client_id(client_id), 
   client_secret(client_secret)
{
}

YoutubeTokenRefresher::YoutubeTokenRefresher() 
	: TokenRefresher(Platform::YOUTUBE, "https://oauth2.googleapis.com/token", 
					 get_setting("client_id", Platform::YOUTUBE), 
					 get_setting("client_secret", Platform::YOUTUBE))
{
}

SpotifyTokenRefresher::SpotifyTokenRefresher()
	: TokenRefresher(Platform::SPOTIFY, "https://accounts.spotify.com/api/token", 
					 get_setting("client_id", Platform::SPOTIFY))
{
}

bool TokenRefresher::access_tkn_valid() {
	std::string _;
	std::time_t expiry;
	if(!fetch_access_tkn(platform, _, expiry)) {
		// not in keychain
		return false;
	}
	return expiry > time(nullptr);
}

bool TokenRefresher::refresh_tkn_valid() {
	std::string _;
	return fetch_refresh_tkn(platform, _);
}

bool TokenRefresher::refresh() {
	// store post fields in buff
	std::string refresh_tkn;
	fetch_refresh_tkn(platform, refresh_tkn);
	size_t fields_sz = 512;
	std::string fields(fields_sz, 0);
	int nbyte = snprintf(
		fields.data(), fields_sz, "client_id=%s&grant_type=refresh_token&refresh_token=%s",
		client_id.c_str(), refresh_tkn.c_str()
	);
	if (!client_secret.empty()) {
		snprintf(fields.data()+nbyte, fields_sz, "&client_secret=%s", client_secret.c_str());
	}

	// TODO move logic into http_requests.cpp
	// init and cleanup for each requrest is highly inefficient
	CURL *curl = curl_easy_init();
	if (!curl) {
		std::cerr << "Failed to setup easy curl" << '\n';
		return false;
	}
	std::string res;
	curl_easy_setopt(curl, CURLOPT_URL, refresh_url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields.c_str());
	CURLcode status = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if(status != CURLE_OK) {
		std::cerr << "Failed to refresh " << platform << " token" << '\n';
		return false;
	}

	nlohmann::json jres = nlohmann::json::parse(res, /*cb=*/nullptr, /*allow_except=*/false);
	if (jres.is_discarded()) {
		// TODO remove
		std::cerr << "Couldn't process " << title(platform) << " refresh response\n";
		return false;
	}
	if (jres.contains("error")) {
		std::cerr << res << '\n';
		std::cerr << "Refreshing " << title(platform) << " token failed\n";
		return false;
	}

	if (!save_access_tkn(platform, jres["access_token"], jres["expires_in"])) {
		return false;
	}
	if (jres.contains("refresh_token")) {
		if (!save_refresh_tkn(platform, jres["refresh_token"])) {
			return false;
		}
	}
	return true;
}
