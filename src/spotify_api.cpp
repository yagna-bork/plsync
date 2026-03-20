#include "../include/spotify_api.h"
#include "../include/config.h"
#include "../include/models.h"
#include <cstdint>
#include <algorithm>
#include <string>
#include <string>
#include <unordered_set>
#include <curl/curl.h>

// TODO copy into new_spotify_api
static std::string read_etag_header(CURL* curl) {
	struct curl_header* header;
	CURLHcode status = curl_easy_header(curl, "etag", 0, CURLH_HEADER, -1, &header);
	if (status == CURLHE_MISSING) {
		return "";
	}
	if (status != CURLHE_OK) {
		throw BaseAPI::RequestError("couldn't read header");
	}
	return std::string(header->value);
}

long SpotifyAPI::paginated_GET(
	const std::string &endpoint, nlohmann::json &initial_page, Params &params, std::string &etag
) {
	std::size_t limit = 50, offset = 0;
	params.emplace_back("limit", std::to_string(limit));
	params.emplace_back("offset", std::to_string(offset));
	
	long status_code = GET(endpoint, initial_page, params, access_tkn, etag);
	if (status_code < 200 || status_code >= 300) {
		return status_code;
	}
	etag = read_etag_header(curl.get());
	std::size_t total = initial_page["total"];

	for (offset = limit; offset < total; offset += limit) {
		params.back().second = std::to_string(offset);
		nlohmann::json next_page;
		status_code = GET(endpoint, next_page, params, access_tkn);
		if (status_code < 200 || status_code >= 300) {
			return status_code;
		}
		std::move(next_page["items"].begin(), next_page["items"].end(), std::back_inserter(initial_page["items"]));
	}
	return status_code;
}

const std::string &SpotifyAPI::get_user_id() {
	if (user_id.empty()) {
		nlohmann::json resp;
		if(GET("me", resp, Params(), access_tkn) != 200) {
			throw RequestError("Invalid response from spotify");
		}
		user_id = resp["id"];
	}
	return user_id;
}

bool SpotifyAPI::get_playlists(std::vector<Playlist> &playlists, std::string &etag) {
	auto user_id = get_user_id();
	Params params = {};
	nlohmann::json resp;
	if(paginated_GET("/me/playlists", resp, params, etag) != 200L) {
		throw RequestError("Invalid response from spotify");
	}
	std::unordered_set<std::string> ids;
	for (auto &plist: resp["items"]) {
		// Ignore playlists followed by user, reserve that for a 
		// different method to be consistent across platforms
		if (plist["owner"]["id"] != user_id) {
			continue;
		}
		// deal with duplicate playlists across different pages bug
		if (ids.count(plist["id"])) {
			continue;
		}
		ids.insert(plist["id"]);
		playlists.emplace_back(
			std::move(plist["id"]), 
			/*etag=*/"",
			std::move(plist["snapshot_id"]), 
			std::move(plist["name"]),
			!plist["public"],
			plist["items"].size()
		);
	}
	return true;
}

SpotifyAuthAPI::TokenResponse SpotifyAuthAPI::exchange_auth_code() {
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
		{
			"redirect_uri", 
			get_setting("redirect_url") + ":" + get_setting("redirect_port", platform)
		}
	};

	nlohmann::json resp;
	if(POST("token", fields, resp) != 200) {
		throw RequestError("invalid token response from spotify");
	}
	validate_scopes(resp["scope"]);
	return TokenResponse(std::move(resp));
}

BaseAuthAPI::AccessTokenResponse SpotifyAuthAPI::refresh_access_tkn(const std::string &refresh_tkn) {
	std::vector<std::pair<std::string, std::string>> fields = {
		{"client_id", get_setting("client_id", platform)}, 
		{"grant_type", "refresh_token"},
		{"refresh_token", refresh_tkn}
	};
	
	nlohmann::json resp;
	if(POST(/*endpoint=*/"token", fields, resp) != 200) {
		throw RequestError("invalid token response from spotify");
	}
	return AccessTokenResponse(std::move(resp));
}
