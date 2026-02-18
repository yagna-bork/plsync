#ifndef GUARD_SPOTIFY_API_H
#define GUARD_SPOTIFY_API_H
#include "api.h"
#include "platform.h"
#include <string>
#include <memory>
#include <curl/curl.h>

class SpotifyAPI : public BaseDataAPI {
public:
	SpotifyAPI(std::shared_ptr<CURL> curl, const std::string &access_tkn = "")
		: BaseDataAPI(Platform::SPOTIFY, spotify_api_url, curl, access_tkn) 
	{
	}

	virtual bool get_playlists(std::vector<Playlist> &playlists, std::string &etag);

private:
	static inline const std::string spotify_api_url = "https://api.spotify.com/v1";
};

class SpotifyAuthAPI : public BaseAuthAPI {
public:
	SpotifyAuthAPI(std::shared_ptr<CURL> curl)
		: BaseAuthAPI(Platform::SPOTIFY, spotify_auth_url, curl, spotify_scopes,
					  auth_svr_url, redirect_port)
	{
	}
	
	virtual TokenResponse exchange_auth_code();

	virtual AccessTokenResponse refresh_access_tkn(const std::string &refresh_tkn);

private:
	static inline const std::string spotify_auth_url = "https://accounts.spotify.com/api";
	static inline const std::vector<std::string> spotify_scopes = {
		"playlist-read-private",
		"playlist-read-collaborative",
		"playlist-modify-private",
		"playlist-modify-public"
	};
	static inline const std::string auth_svr_url = "https://accounts.spotify.com/authorize";
	static const int redirect_port = 8001;
};
#endif
