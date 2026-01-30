#ifndef GUARD_YOUTUBE_API_H
#define GUARD_YOUTUBE_API_H
#include "api.h"
#include <memory>
#include <string>
#include <curl/curl.h>

const std::string youtube_api_url = "https://www.googleapis.com/youtube/v3";

class YoutubeAPI : public BaseAPI {
public:
	YoutubeAPI(std::shared_ptr<CURL> curl, const std::string &access_tkn = "")
		: BaseAPI(Platform::YOUTUBE, youtube_api_url, curl, access_tkn)
	{
	}
};
#endif
