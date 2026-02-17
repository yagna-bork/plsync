#include "../include/init.h"
#include "../include/config.h"
#include "../include/util.h"
#include "../include/token_store.h"
#include "../include/api.h"
#include "../include/youtube_api.h"
#include "../include/spotify_api.h"
#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <iterator>
#include <utility>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

static bool get_user_permissions(Platform platform, std::shared_ptr<CURL> curl) {
	std::unique_ptr<BaseAuthAPI> api;
	if (platform == Platform::YOUTUBE) {
		api = std::make_unique<YoutubeAuthAPI>(curl);
	} else {
		api = std::make_unique<SpotifyAuthAPI>(curl);
	}

	// direct user to permission screen
	std::ostringstream cmd;
	cmd << "open '" << api->get_auth_url() << "'";
	system(cmd.str().c_str());

	// listen at redirect url for auth_code
	std::string auth_code;
	std::cout << "Waiting for " << title(platform) << " authentication code... " << std::flush;
	if (!api->collect_auth_code()) {
		std::cout << "Unable to complete " << title(platform) << " authentication. Please try again" << '\n';
		return false;
	}
	std::cout << "Got it!\n";

	// exchange auth_code for access & refresh tokens
	BaseAuthAPI::TokenResponse tkn_resp;
	try {
		tkn_resp = api->exchange_auth_code();
	} catch (const BaseAuthAPI::RequestError &e) {
		std::cerr << "Something went wrong. Please try again\n";
		return false;
	}

	// finally store the tokens
	if (!save_access_tkn(platform, tkn_resp.access_tkn, tkn_resp.access_duration)) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << '\n';
		return false;
	}
	if (!save_refresh_tkn(platform, tkn_resp.refresh_tkn)) {
		std::cerr << "Couldn't store tokens in keychain. Please try again" << '\n';
		return false;
	}
	std::cout << "Success! " << title(platform) << " authentication completed" << '\n';
	return true;
}

int run_init(bool init_youtube, bool init_spotify) {
	std::shared_ptr<CURL> curl(curl_easy_init(), curl_easy_cleanup);
	if (init_youtube && !get_user_permissions(Platform::YOUTUBE, curl)) {
		return 1;
	}
	if (init_spotify && !get_user_permissions(Platform::SPOTIFY, curl)) {
		return 1;
	}
	return 0;
}
