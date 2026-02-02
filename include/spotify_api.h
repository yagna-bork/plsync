#ifndef GUARD_SPOTIFY_API_H
#define GUARD_SPOTIFY_API_H
#include "api.h"
#include "platform.h"
#include <string>
#include <memory>
#include <curl/curl.h>

const std::string spotify_api_url = "https://api.spotify.com";

class SpotifyAPI : public BaseAPI {
public:
	SpotifyAPI(std::shared_ptr<CURL> curl, const std::string &access_tkn = "")
		: BaseAPI(Platform::SPOTIFY, spotify_api_url, curl, access_tkn) 
	{
	}
};
#endif
