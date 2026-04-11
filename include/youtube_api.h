#ifndef GUARD_YOUTUBE_API_H
#define GUARD_YOUTUBE_API_H
#include "api.h"
#include "cache.h"
#include <curl/curl.h>
#include <memory>
#include <string>

class YoutubeAPI : public BaseDataAPI {
public:
    YoutubeAPI(std::shared_ptr<CURL> curl, const std::string& access_tkn = "")
        : BaseDataAPI(Platform::YOUTUBE, youtube_api_url, curl, access_tkn) {}

    virtual bool get_playlists(std::vector<Playlist>& playlists,
                               std::string& etag);

private:
    long paginated_GET(const std::string& endpoint, nlohmann::json& resp,
                       Params& params);

    long paginated_GET(const std::string& endpoint, nlohmann::json& resp,
                       Params& params, std::string& etag);

private:
    static inline const std::string youtube_api_url =
        "https://www.googleapis.com/youtube/v3";
};

class YoutubeAuthAPI : public BaseAuthAPI {
public:
    YoutubeAuthAPI(std::shared_ptr<CURL> curl)
        : BaseAuthAPI(Platform::YOUTUBE, youtube_auth_url, curl, youtube_scopes,
                      auth_svr_url, redirect_port) {}

    virtual TokenResponse exchange_auth_code();

    virtual AccessTokenResponse
    refresh_access_tkn(const std::string& refresh_tkn);

private:
    static inline const std::string youtube_auth_url =
        "https://oauth2.googleapis.com";
    static inline const std::vector<std::string> youtube_scopes = {
        "https://www.googleapis.com/auth/youtube"};
    static inline const std::string auth_svr_url =
        "https://accounts.google.com/o/oauth2/v2/auth";
    static const int redirect_port = 8000;
};
#endif
