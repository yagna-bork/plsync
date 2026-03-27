#ifndef GUARD_NEW_YOUTUBE_API_H
#define GUARD_NEW_YOUTUBE_API_H
#include "models.h"
#include <string>
#include <curl/curl.h>

namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";

bool get_playlist(CURL* curl, const std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

Playlist create_playlist(CURL* curl, const std::string& access_tkn, const std::string& title);

}
#endif
