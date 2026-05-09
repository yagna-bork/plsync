#include "../include/cache.h"
#include "../include/util.h"
#include "../include/youtube_api.h"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

using namespace nlohmann;

long YoutubeAPI::paginated_GET(const std::string& endpoint, json& initial_page,
                               Params& params) {
    json next_page;
    long status_code = GET(endpoint, next_page, params, access_tkn);
    if (status_code != 200L) {
        return status_code;
    }

    std::move(next_page["items"].begin(), next_page["items"].end(),
              std::back_inserter(initial_page["items"]));

    if (next_page.contains("nextPageToken")) {
        params.back().second = next_page["nextPageToken"];
        return paginated_GET(endpoint, initial_page, params);
    } else {
        return status_code;
    }
}

long YoutubeAPI::paginated_GET(const std::string& endpoint, json& initial_page,
                               Params& params, std::string& etag) {
    long status_code = GET(endpoint, initial_page, params, access_tkn, etag);
    if (status_code != 200L) {
        return status_code;
    }
    etag = initial_page.value("etag", "");
    if (initial_page.contains("nextPageToken")) {
        params.emplace_back("pageToken", initial_page["nextPageToken"]);
        return paginated_GET(endpoint, initial_page, params);
    } else {
        return status_code;
    }
}

bool YoutubeAPI::get_playlists(std::vector<Playlist>& playlists,
                               std::string& etag) {
    Params params = {
        {"mine", "true"},
        {"part", "id,snippet,status,contentDetails"},
        {"fields",
         "etag,nextPageToken,items("
         "id,etag,snippet/title,status/privacyStatus,contentDetails/itemCount"
         ")"},
        {"maxResults", "50"}};
    json resp;
    std::string etag_copy = etag;
    long status_code = paginated_GET("playlists", resp, params, etag_copy);

    if (status_code == 304L) {
        return false;
    } else if (status_code == 200L) {
        etag = etag_copy;
        for (json& playlist : resp["items"]) {
            playlists.emplace_back(
                std::move(playlist["id"]),
                // the api has a seperate etag for the resource containing a
                // single playlist and the playlist resource itself, only get
                // former after call to get_playlist
                /*etag=*/"",
                /*version=*/std::move(playlist["etag"]),
                std::move(playlist["snippet"]["title"]), Platform::YOUTUBE,
                playlist["status"]["privacyStatus"] == "private",
                playlist["contentDetails"]["itemCount"]);
        }
        return true;
    } else {
        throw RequestError("Invalid response from youtube");
    }
}

BaseAuthAPI::TokenResponse YoutubeAuthAPI::exchange_auth_code() {
    if (verifier.empty()) {
        throw SequenceError("verifier not initialised");
    }
    if (auth_code.empty()) {
        throw SequenceError("auth_code not initialised");
    }

    std::vector<std::pair<std::string, std::string>> fields = {
        {"client_id", get_setting("client_id", platform)},
        {"code", auth_code},
        {"code_verifier", verifier},
        {"grant_type", "authorization_code"},
        {"client_secret", get_setting("client_secret", platform)},
        {"redirect_uri", get_setting("redirect_url") + ":" +
                             get_setting("redirect_port", platform)}};

    json resp;
    if (POST(/*endpoint=*/"token", fields, resp) != 200) {
        throw RequestError("invalid token response from google");
    }
    validate_scopes(resp["scope"]);
    return TokenResponse(std::move(resp));
}

BaseAuthAPI::AccessTokenResponse
YoutubeAuthAPI::refresh_access_tkn(const std::string& refresh_tkn) {
    std::vector<std::pair<std::string, std::string>> fields = {
        {"client_id", get_setting("client_id", platform)},
        {"grant_type", "refresh_token"},
        {"refresh_token", refresh_tkn},
        {"client_secret", get_setting("client_secret", platform)}};

    json resp;
    if (POST(/*endpoint=*/"token", fields, resp) != 200) {
        throw RequestError("invalid token response from google");
    }
    return AccessTokenResponse(std::move(resp));
}
