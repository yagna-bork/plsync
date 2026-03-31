#ifndef GUARD_NEW_API_H
#define GUARD_NEW_API_H
#include "platform.h"
#include "cache.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace API {

class RequestError : public std::runtime_error {
public:
	RequestError(const char *msg) : std::runtime_error(msg) {}
};

using Params = std::vector<std::pair<std::string, std::string>>;
using Fields = Params;
using Song = std::pair<std::string, std::string>;
using Songs = std::vector<Song>;

/* 
 * Performs a GET request at the specified url.
 * Throws RequestError if the request couldn't be made.
 * If response is JSON then it's saved in `resp`
 * and returns the http response status code.
 * If you provide an etag status code can also be 304.
 * You can provide an access token to make an 
 * authenticated request.
 */
long GET(
	CURL* curl,
	const std::string &url, 
	nlohmann::json &resp, 
	const Params &params = {}, 
	const std::string &access_tkn = "", 
	const std::string &etag = ""
);

/*
 * Performs a POST request at the specified endpoint.
 * This will use the default urlencoded POST type unless 
 * specified in application_type.
 * Throws RequestError if the request couldn't be made.
 * Returns the http response status code and stores 
 * the result in resp.
 */
long POST(
	CURL* curl,
	const std::string &url, 
	const std::string& data, 
	nlohmann::json &resp, 
	const std::string& application_type = "",
	const Params &params = {},
	const std::string &access_tkn = ""
);

/* 
 * Returns whether playlist was modified.
 * res will be an empty playlist i.e. id attribute is empty 
 * if the playlist doesn't exist/was deleted.
 */
bool get_playlist(Platform plat, CURL* curl, const std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

Playlist create_playlist(Platform plat, CURL* curl, const std::string& access_tkn, const std::string& title);

bool get_playlist_items(
	Platform plat, CURL* curl, const std::string& access_tkn, const std::string& playlist_id, Songs& out_songs, std::string& in_out_etag
);

} // namespace API



namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";

bool get_playlist(CURL* curl, const std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn, const std::string& title);

bool get_playlist_items(
	CURL* curl, const std::string& access_tkn, const std::string& playlist_id, API::Songs& out_songs, std::string& in_out_etag
);

} // namespace NewYoutubeAPI



namespace NewSpotifyAPI {

const std::string base_url = "https://api.spotify.com/v1";

bool get_playlist(CURL* curl, const std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn, const std::string& title);

bool get_playlist_items(
	CURL* curl, const std::string& access_tkn, const std::string& playlist_id, API::Songs& songs_out, std::string& in_out_etag
);

} // namespace NewSpotifyAPI
#endif
