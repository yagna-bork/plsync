#ifndef GUARD_NEW_YOUTUBE_API_H
#define GUARD_NEW_YOUTUBE_API_H
#include "models.h"
#include <string>
#include <curl/curl.h>

namespace NewYoutubeAPI {

const std::string base_url = "https://www.googleapis.com/youtube/v3";

/* 
 * Returns whether playlist was modified.
 * res will be an empty playlist i.e. id attribute is empty 
 * if the playlist doesn't exist/was deleted.
 */
bool get_playlist(CURL* curl, std::string& access_tkn, const std::string& id, const std::string& etag, Playlist& res);

}
#endif
