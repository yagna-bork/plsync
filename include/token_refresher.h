#ifndef GUARD_TOKEN_REFRESHER_H
#define GUARD_TOKEN_REFRESHER_H
#include "config.h"
#include "platform.h"
#include <string>

class TokenRefresher {
public:
	TokenRefresher(Platform platform, const std::string &refresh_url, 
				   const std::string &client_id, const std::string &client_secret = "");

	bool access_tkn_valid();
	bool refresh_tkn_valid();
	bool refresh();

private:
	const Platform platform;
	const std::string refresh_url;
	const std::string client_id;
	const std::string client_secret;
};

class YoutubeTokenRefresher: public TokenRefresher {
public:
	YoutubeTokenRefresher();
};

class SpotifyTokenRefresher: public TokenRefresher {
public:
	SpotifyTokenRefresher();
};
#endif
