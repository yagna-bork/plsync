#ifndef GUARD_NEW_API_H
#define GUARD_NEW_API_H
#include "cache.h"
#include "platform.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

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
bool get_playlist(Platform plat, CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res);

Playlist create_playlist(Platform plat, CURL* curl,
                         const std::string& access_tkn,
                         const std::string& title);

bool get_song_to_item_ids(Platform plat, CURL* curl,
                          const std::string& plat_access_tkn,
                          const std::string& sp_access_tkn,
                          const std::string& playlist_id,
                          SongToItemIds& out_song_to_item_ids,
                          std::string& in_out_etag);

// TODO rename to add_playlist_items
void add_songs_to_playlist(
    Platform plat, CURL* curl, const std::string& access_tkn, Playlist& pl,
    // TODO rename song_hashes -> items
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

// TODO rename to remove_playlist_items
void remove_songs_from_playlist(
    Platform plat, CURL* curl, const std::string& access_tkn, Playlist& pl,
    // TODO rename song_hashes -> items
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

} // namespace API

namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title);

bool get_song_to_item_ids(CURL* curl, const std::string& yt_access_tkn,
                          const std::string& sp_access_tkn,
                          const std::string& playlist_id,
                          SongToItemIds& out_song_to_item_ids,
                          std::string& in_out_etag);

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song);

void add_songs_to_playlist(
    CURL* curl, const std::string& access_tkn, Playlist& pl,
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

void remove_songs_from_playlist(
    CURL* curl, const std::string& access_tkn, Playlist& pl,
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

} // namespace NewYoutubeAPI

namespace NewSpotifyAPI {

const std::string base_url = "https://api.spotify.com/v1";

bool get_playlist(CURL* curl, const std::string& access_tkn,
                  const std::string& id, const std::string& etag,
                  Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn,
                         const std::string& title);

bool get_song_to_item_ids(CURL* curl, const std::string& yt_access_tkn,
                          const std::string& playlist_id,
                          SongToItemIds& out_song_to_item_ids,
                          std::string& in_out_etag);

Song get_single_truth_song(CURL* curl, const std::string& access_tkn,
                           const Song& song);

std::string search_song(CURL* curl, const std::string& access_tkn,
                        const Song& song);

void add_songs_to_playlist(
    CURL* curl, const std::string& access_tkn, Playlist& pl,
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

void remove_songs_from_playlist(
    CURL* curl, const std::string& access_tkn, Playlist& pl,
    const SongHashCounts& song_hashes,
    const std::unordered_map<size_t, Song>& songh_to_song);

} // namespace NewSpotifyAPI
#endif
