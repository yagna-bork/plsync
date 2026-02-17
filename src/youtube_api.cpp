#include "../include/youtube_api.h"
#include "../include/config.h"
#include <vector>
#include <string>
#include <utility>

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
		{
			"redirect_uri", 
			get_setting("redirect_url") + ":" + get_setting("redirect_port", platform)
		}
	};
	
	nlohmann::json resp;
	if(POST(/*endpoint=*/"token", fields, resp) != 200) {
		throw RequestError("invalid token response from google");
	}
	validate_scopes(resp["scope"]);
	return TokenResponse(std::move(resp));
}

BaseAuthAPI::AccessTokenResponse YoutubeAuthAPI::refresh_access_tkn(const std::string &refresh_tkn) {
	std::vector<std::pair<std::string, std::string>> fields = {
		{"client_id", get_setting("client_id", platform)}, 
		{"grant_type", "refresh_token"},
		{"refresh_token", refresh_tkn},
		{"client_secret", get_setting("client_secret", platform)}
	};
	
	nlohmann::json resp;
	if(POST(/*endpoint=*/"token", fields, resp) != 200) {
		throw RequestError("invalid token response from google");
	}
	return AccessTokenResponse(std::move(resp));
}
