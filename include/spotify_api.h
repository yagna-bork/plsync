#ifndef GUARD_SPOTIFY_API_H
#define GUARD_SPOTIFY_API_H
#include "api.h"
#include "platform.h"
#include <string>
#include <memory>
#include <curl/curl.h>

const std::string SPOTIFY_API_URL = "https://api.spotify.com/v1";
const std::string SPOTIFY_AUTH_URl = "https://accounts.spotify.com/api";
const std::vector<std::string> SPOTIFY_SCOPES = {
	"playlist-read-private",
	"playlist-read-collaborative",
	"playlist-modify-private",
	"playlist-modify-public"
};

class SpotifyAPI : public BaseDataAPI {
public:
	SpotifyAPI(std::shared_ptr<CURL> curl, const std::string &access_tkn = "")
		: BaseDataAPI(Platform::SPOTIFY, SPOTIFY_API_URL, curl, access_tkn) 
	{
	}
};

class SpotifyAuthAPI : public BaseAuthAPI {
public:
	SpotifyAuthAPI(std::shared_ptr<CURL> curl) 
		: BaseAuthAPI(Platform::SPOTIFY, SPOTIFY_AUTH_URl, curl, SPOTIFY_SCOPES)
	{
	}
	
	virtual TokenResponse exchange_auth_code(
		const std::string &auth_code, const std::string &verifier
	);
};
#endif
