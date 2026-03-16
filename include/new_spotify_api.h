#ifndef GUARD_NEW_SPOTIFY_API
#define GUARD_NEW_SPOTIFY_API
#include "models.h"

namespace NewSpotifyAPI {

const std::string base_url = "https://api.spotify.com/v1";

bool get_playlist(CURL* curl, std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

Playlist create_playlist(CURL* curl, std::string& access_tkn, const std::string& title);

} // namespace NewSpotifyAPI
#endif
