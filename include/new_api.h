#ifndef GUARD_NEW_API_H
#define GUARD_NEW_API_H
#include "cache.h"
#include "platform.h"
#include <ctime>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";
const std::string base_auth_url = "https://oauth2.googleapis.com";
const std::string oauth_url = "https://accounts.google.com/o/oauth2/v2/auth";
const std::vector<std::string> scopes = {
    "https://www.googleapis.com/auth/youtube"};

void exchange_auth_code(CURL* curl, const std::string& verifier,
                        const std::string& auth_code,
                        std::string& out_access_tkn,
                        time_t& out_access_duration,
                        std::string& out_refresh_tkn);

void refresh_access_tkn(CURL* curl, const std::string& refresh_tkn,
                        std::string& out_access_tkn,
                        time_t& out_access_duration,
                        std::string& out_refresh_tkn);

bool get_playlists(CURL* curl, std::vector<Playlist>& out_playlists,
                   std::string& in_out_etag);

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
const std::string base_auth_url = "https://accounts.spotify.com/api";
const std::string oauth_url = "https://accounts.spotify.com/authorize";
const std::vector<std::string> scopes = {
    "playlist-read-private",   "playlist-read-collaborative",
    "playlist-modify-private", "playlist-modify-public",
    "user-library-read",       "user-follow-read"};

void exchange_auth_code(CURL* curl, const std::string& verifier,
                        const std::string& auth_code,
                        std::string& out_access_tkn,
                        time_t& out_access_duration,
                        std::string& out_refresh_tkn);

void refresh_access_tkn(CURL* curl, const std::string& refresh_tkn,
                        std::string& out_access_tkn,
                        time_t& out_access_duration,
                        std::string& out_refresh_tkn);

bool get_playlists(CURL* curl, std::vector<Playlist>& out_playlists,
                   std::string& in_out_etag);

bool was_playlist_deleted(CURL* curl, const std::string& access_tkn,
                          const std::string& id);

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
    RequestError(const std::string& msg) : std::runtime_error(msg) {}
};

class AuthError : public std::runtime_error {
public:
    AuthError(const std::string& msg) : std::runtime_error(msg) {}
};

using Params = std::vector<std::pair<std::string, std::string>>;
using Fields = Params;

const std::string redirect_url = "http://127.0.0.1";

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

std::string generate_verifier();

inline std::string get_redirect_port(Platform plat) {
    return std::to_string(8000 + plat);
}

std::string get_auth_url(Platform plat, CURL* curl, const std::string& verfier,
                         const std::string& state);

std::string collect_auth_code(Platform plat, const std::string& state);

bool are_scopes_valid(const std::string& granted, const std::string& required);

inline void
exchange_auth_code(Platform plat, CURL* curl, const std::string& verifier,
                   const std::string& auth_code, std::string& out_access_tkn,
                   time_t& out_access_duration, std::string& out_refresh_tkn) {
    switch (plat) {
    case Platform::YOUTUBE:
        NewYoutubeAPI::exchange_auth_code(curl, verifier, auth_code,
                                          out_access_tkn, out_access_duration,
                                          out_refresh_tkn);
        break;
    case Platform::SPOTIFY:
        NewSpotifyAPI::exchange_auth_code(curl, verifier, auth_code,
                                          out_access_tkn, out_access_duration,
                                          out_refresh_tkn);
        break;
    default:
        // TODO make this the default across all methods
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

inline void refresh_access_tkn(Platform plat, CURL* curl,
                               const std::string& refresh_tkn,
                               std::string& out_access_tkn,
                               time_t& out_access_duration,
                               std::string& out_refresh_tkn) {
    switch (plat) {
    case Platform::YOUTUBE:
        NewYoutubeAPI::refresh_access_tkn(curl, refresh_tkn, out_access_tkn,
                                          out_access_duration, out_refresh_tkn);
        break;
    case Platform::SPOTIFY:
        NewSpotifyAPI::refresh_access_tkn(curl, refresh_tkn, out_access_tkn,
                                          out_access_duration, out_refresh_tkn);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
}

/* returns whether users playlists have changed according to etag */
inline bool get_playlists(Platform plat, CURL* curl,
                          std::vector<Playlist>& out_playlists,
                          std::string& in_out_etag) {
    switch (plat) {
    case Platform::YOUTUBE:
        NewYoutubeAPI::get_playlists(curl, out_playlists, in_out_etag);
        break;
    case Platform::SPOTIFY:
        NewSpotifyAPI::get_playlists(curl, out_playlists, in_out_etag);
        break;
    default:
        throw std::domain_error("function not yet implemented for " +
                                platform_title_lower(plat));
    }
    // TODO remove
    return false;
}

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
