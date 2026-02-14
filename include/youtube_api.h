#ifndef GUARD_YOUTUBE_API_H
#define GUARD_YOUTUBE_API_H
#include "api.h"
#include <memory>
#include <string>
#include <curl/curl.h>

const std::string YOUTUBE_API_URL = "https://www.googleapis.com/youtube/v3";
const std::string YOUTUBE_AUTH_URL = "https://oauth2.googleapis.com";
const std::vector<std::string> YOUTUBE_SCOPES = {"https://www.googleapis.com/auth/youtube"};

class YoutubeAPI : public BaseDataAPI {
public:
	YoutubeAPI(std::shared_ptr<CURL> curl, const std::string &access_tkn = "")
		: BaseDataAPI(Platform::YOUTUBE, YOUTUBE_API_URL, curl, access_tkn)
	{
	}
};

class YoutubeAuthAPI : public BaseAuthAPI {
public:
	YoutubeAuthAPI(std::shared_ptr<CURL> curl)
		: BaseAuthAPI(Platform::YOUTUBE, YOUTUBE_AUTH_URL, curl, YOUTUBE_SCOPES)
	{
	}

	virtual TokenResponse exchange_auth_code(
		const std::string &auth_code, const std::string &verifier
	);
};
#endif
