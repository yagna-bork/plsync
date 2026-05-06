#ifndef GUARD_NEW_API_H
#define GUARD_NEW_API_H
#include "cache.h"
#include "platform.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title);

bool get_playlist_items(CURL* curl, const std::string& yt_access_tkn,
                        const std::string& sp_access_tkn, Playlist& out_pl);

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song);

void playlist_items_add(CURL* curl, const std::string& access_tkn, Playlist& pl,
                        const SongCounts& song_cnts);

void playlist_items_remove(CURL* curl, const std::string& access_tkn,
                           Playlist& pl, const SongCounts& song_cnts);

} // namespace NewYoutubeAPI

namespace NewSpotifyAPI {

const std::string base_url = "https://api.spotify.com/v1";

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title);

bool get_playlist_items(CURL* curl, const std::string& yt_access_tkn,
                        Playlist& out_pl);

// spotify is the single source of truth for a song across every platforms
Song get_ssot_song(CURL* curl, const std::string& access_tkn, const Song& song);

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song);

void playlist_items_add(CURL* curl, const std::string& access_tkn, Playlist& pl,
                        const SongCounts& song_cnts);

void playlist_items_remove(CURL* curl, const std::string& access_tkn,
                           Playlist& pl, const SongCounts& song_cnts);

} // namespace NewSpotifyAPI

namespace API {

class RequestError : public std::runtime_error {
public:
    RequestError(const char* msg) : std::runtime_error(msg) {}
};

using Params = std::vector<std::pair<std::string, std::string>>;
using Fields = Params;

/*
 * Performs a GET request at the specified url.
 * Throws RequestError if the request couldn't be made.
 * If response is JSON then it's saved in `resp`
 * and returns the http response status code.
 * If you provide an etag status code can also be 304.
 * You can provide an access token to make an
 * authenticated request.
 */
long GET(CURL* curl, const std::string& url, nlohmann::json& resp,
         const Params& params = {}, const std::string& access_tkn = "",
         const std::string& etag = "");

/*
 * Performs a POST request at the specified endpoint.
 * This will use the default urlencoded POST type unless
 * specified in application_type.
 * Throws RequestError if the request couldn't be made.
 * Returns the http response status code and stores
 * the result in resp.
 */
long POST(CURL* curl, const std::string& url, const std::string& data,
          nlohmann::json& resp, const std::string& application_type = "",
          const Params& params = {}, const std::string& access_tkn = "");

/*
 * Performs a DELETE request at the specified url.
 * Throws RequestError if the request couldn't be made.
 * If response is JSON then it's saved in `resp`
 * and returns the http response status code.
 * You can provide an access token to make an
 * authenticated request.
 */
long DELETE(CURL* curl, const std::string& url, nlohmann::json& resp,
            const std::string& access_tkn, const Params& params = {},
            const std::string& data = "");

/*
 * Returns whether playlist was modified.
 * res will be an empty playlist i.e. id attribute is empty
 * if the playlist doesn't exist/was deleted.
 */
inline bool get_playlist(Platform plat, CURL* curl,
                         const std::string& access_tkn, const std::string& id,
                         const std::string& etag, Playlist& res) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::get_playlist(curl, access_tkn, id, etag, res);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::get_playlist(curl, access_tkn, id, etag, res);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

inline Playlist create_playlist(Platform plat, CURL* curl,
                                const std::string& access_tkn,
                                const std::string& title) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::create_playlist(curl, access_tkn, title);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::create_playlist(curl, access_tkn, title);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

inline bool get_playlist_items(Platform plat, CURL* curl,
                               const std::string& plat_access_tkn,
                               const std::string& sp_access_tkn,
                               Playlist& out_pl) {
    switch (plat) {
    case Platform::YOUTUBE:
        return NewYoutubeAPI::get_playlist_items(curl, plat_access_tkn,
                                                 sp_access_tkn, out_pl);
        break;
    case Platform::SPOTIFY:
        return NewSpotifyAPI::get_playlist_items(curl, plat_access_tkn, out_pl);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

inline void playlist_items_add(Platform plat, CURL* curl,
                               const std::string& access_tkn, Playlist& pl,
                               const SongCounts& song_cnts) {
    switch (plat) {
    case Platform::YOUTUBE:
        NewYoutubeAPI::playlist_items_add(curl, access_tkn, pl, song_cnts);
        break;
    case Platform::SPOTIFY:
        NewSpotifyAPI::playlist_items_add(curl, access_tkn, pl, song_cnts);
        break;
    default:
        throw std::runtime_error("Not yet implemented");
    }
}

inline void playlist_items_remove(Platform plat, CURL* curl,
                                  const std::string& access_tkn, Playlist& pl,
                                  const SongCounts& song_cnts) {
    switch (plat) {
    case Platform::YOUTUBE:
        NewYoutubeAPI::playlist_items_remove(curl, access_tkn, pl, song_cnts);
        break;
    case Platform::SPOTIFY:
        NewSpotifyAPI::playlist_items_remove(curl, access_tkn, pl, song_cnts);
        break;
    default:
        throw std::runtime_error("Not yet implemented");
    }
}

} // namespace API

#endif
